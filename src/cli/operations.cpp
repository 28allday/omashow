#include "cli/operations.h"
#include "backend.h"
#include "core/delimited.h"
#include "tablemodel.h"

#include <QMetaMethod>
#include <algorithm>
#include <QMetaProperty>
#include <QUrl>

namespace {

// Anything that would put something on screen and wait for a person, or that
// finishes later on another thread. Each says what to use instead.
const QMap<QString, QString> &refusals() {
    static const QMap<QString, QString> table = {
        {QStringLiteral("openAsync"), QStringLiteral("open the deck as the file argument instead")},
        {QStringLiteral("saveAsync"), QStringLiteral("use saveTo, which writes and reports")},
        {QStringLiteral("exportPdfAsync"), QStringLiteral("use exportPdf, or `omashow export`")},
        {QStringLiteral("insertImageAsync"), QStringLiteral("use insertImage")},
        {QStringLiteral("insertMediaAsync"), QStringLiteral("use insertMedia")},
        {QStringLiteral("copyAsync"), QStringLiteral("use copySelected or cutSelected")},
        {QStringLiteral("pasteAsync"), QStringLiteral("use paste")},
        {QStringLiteral("previewLayoutAsync"), QStringLiteral("use previewLayout")},
        {QStringLiteral("commitTextDocument"),
         QStringLiteral("set the text with setSelectedProperty")},
        {QStringLiteral("pasteEditorText"), QStringLiteral("set the text with setSelectedProperty")},
        {QStringLiteral("openInNewWindow"), QStringLiteral("there are no windows here")},
        {QStringLiteral("quit"), QStringLiteral("the command line exits on its own")},
        {QStringLiteral("table.copyRange"), QStringLiteral("use table.copyText, which answers with the text")},
        {QStringLiteral("table.clipboardText"), QStringLiteral("there is no clipboard here")},
        {QStringLiteral("table.cancelDataFile"), QStringLiteral("nothing is read in the background here")},
        {QStringLiteral("table.refreshDataFile"), QStringLiteral("read the file yourself and use table.applyDataFile")},
        // Writing happens once, when apply finishes; exporting is its own verb.
        {QStringLiteral("saveTo"), QStringLiteral("the deck is written when apply finishes; --out names the file")},
        {QStringLiteral("save"), QStringLiteral("the deck is written when apply finishes; --out names the file")},
        {QStringLiteral("exportPdf"), QStringLiteral("use `omashow export --kind pdf`")},
        {QStringLiteral("exportReview"), QStringLiteral("use `omashow review`")},
        {QStringLiteral("renderFrame"), QStringLiteral("use `omashow export --kind images`")},
        {QStringLiteral("queueExport"), QStringLiteral("use `omashow export`")},
        {QStringLiteral("retryExport"), QStringLiteral("use `omashow export`")},
        {QStringLiteral("cancelExport"), QStringLiteral("nothing exports in the background here")},
        {QStringLiteral("clearFinishedExports"), QStringLiteral("nothing exports in the background here")},
        // These change this computer, not the deck.
        {QStringLiteral("saveAsTemplate"), QStringLiteral("templates are installed from the app")},
        {QStringLiteral("installTemplate"), QStringLiteral("templates are installed from the app")},
        {QStringLiteral("removeTemplate"), QStringLiteral("templates are removed from the app")},
        {QStringLiteral("removeRecent"), QStringLiteral("the recent list belongs to the app")},
        {QStringLiteral("pinRecent"), QStringLiteral("the recent list belongs to the app")},
        {QStringLiteral("openRecent"), QStringLiteral("open the deck as the file argument instead")},
        {QStringLiteral("discardRecovery"), QStringLiteral("recovery journals belong to the app that wrote them")},
        {QStringLiteral("recoverFrom"), QStringLiteral("recovery journals belong to the app that wrote them")},
        {QStringLiteral("importFromDeck"), QStringLiteral("finishes later on another thread; use the import dialog in the app")},
    };
    return table;
}

// Table and chart cells are edited through the table model, a second object
// beside Backend that follows the selection. Its operations are named
// "table.<method>" so the two sets never collide.
const QString kTable = QStringLiteral("table.");

struct Target {
    QObject *object = nullptr;
    const QMetaObject *meta = nullptr;
    QString method;   // the name without the "table." prefix
};

Target targetFor(Backend &backend, const QString &name) {
    if (name.startsWith(kTable))
        return {backend.tableModel(), &TableModel::staticMetaObject, name.mid(kTable.size())};
    return {&backend, &Backend::staticMetaObject, name};
}

bool runnable(const QMetaMethod &method) {
    if (method.methodType() == QMetaMethod::Signal) return false;
    if (method.access() != QMetaMethod::Public) return false;
    if (method.parameterCount() > 8) return false;
    for (int i = 0; i < method.parameterCount(); ++i)
        if (method.parameterMetaType(i).flags().testFlag(QMetaType::PointerToQObject) ||
            method.parameterMetaType(i).id() == QMetaType::UnknownType)
            return false;
    const auto name = QString::fromLatin1(method.name());
    // Qt's own machinery, and the property notifications, are not operations.
    return !name.startsWith(QStringLiteral("_q_")) && name != QStringLiteral("deleteLater") &&
           !name.endsWith(QStringLiteral("Changed"));
}

QVector<QMetaMethod> methodsNamed(const QMetaObject *meta, const QString &name) {
    QVector<QMetaMethod> found;
    // Backend's whole surface is its own; the table model's inherited model
    // machinery (fetchMore, submit, …) is not an operation.
    const int first = meta == &Backend::staticMetaObject ? 0 : meta->methodOffset();
    for (int i = first; i < meta->methodCount(); ++i) {
        const auto method = meta->method(i);
        if (!runnable(method) || QString::fromLatin1(method.name()) != name) continue;
        found.append(method);
    }
    // Fewest arguments first, so a call with two arguments does not match an
    // overload that would silently default the rest.
    std::sort(found.begin(), found.end(), [](const QMetaMethod &a, const QMetaMethod &b) {
        return a.parameterCount() < b.parameterCount();
    });
    return found;
}

bool convert(const QVariant &given, QMetaType wanted, QVariant *out, QString *why) {
    QVariant value = given;
    if (wanted.id() == QMetaType::QUrl && value.metaType().id() == QMetaType::QString) {
        // A path is what an agent has; a URL is what the method wants.
        const auto text = value.toString();
        value = text.contains(QStringLiteral("://")) ? QUrl(text) : QUrl::fromLocalFile(text);
    }
    if (wanted.id() == QMetaType::QStringList && value.metaType().id() == QMetaType::QVariantList) {
        QStringList list;
        for (const auto &row : value.toList()) list.append(row.toString());
        value = list;
    }
    if (wanted.id() == QMetaType::QVariant) { *out = value; return true; }
    if (value.metaType() != wanted && !value.convert(wanted)) {
        *why = QStringLiteral("cannot read %1 as %2")
                   .arg(QString::fromUtf8(given.metaType().name()),
                        QString::fromUtf8(wanted.name()));
        return false;
    }
    *out = value;
    return true;
}

// `setCurrentSlide` is not a method at all: it is the WRITE half of a property,
// which the meta-object keeps in a different list. An agent should not have to
// know that, so a setter name that matches a writable property is honoured.
int propertyBehindSetter(const QString &name) {
    if (!name.startsWith(QStringLiteral("set")) || name.size() < 4) return -1;
    auto property = name.mid(3);
    property[0] = property.at(0).toLower();
    const int index =
        Backend::staticMetaObject.indexOfProperty(property.toLatin1().constData());
    if (index < 0) return -1;
    return Backend::staticMetaObject.property(index).isWritable() ? index : -1;
}

} // namespace

QString Cli::refusal(const QString &name) {
    if (name.endsWith(QStringLiteral("Dialog")))
        return QStringLiteral("%1 asks the desktop for a file; give the path to the operation "
                              "that takes one instead").arg(name);
    if (refusals().contains(name))
        return QStringLiteral("%1 finishes later or needs a window; %2")
            .arg(name, refusals().value(name));
    return {};
}

QVariantList Cli::describeOperations(const QString &filter) {
    QVariantList rows;
    const QMetaObject *meta = &Backend::staticMetaObject;
    QStringList seen;
    for (int i = 0; i < meta->methodCount(); ++i) {
        const auto method = meta->method(i);
        if (!runnable(method)) continue;
        const auto name = QString::fromLatin1(method.name());
        if (!Cli::refusal(name).isEmpty()) continue;
        if (!filter.isEmpty() && !name.contains(filter, Qt::CaseInsensitive)) continue;
        QVariantList arguments;
        for (int p = 0; p < method.parameterCount(); ++p)
            arguments.append(QVariantMap{
                {QStringLiteral("name"), QString::fromLatin1(method.parameterNames().value(p))},
                {QStringLiteral("type"), QString::fromUtf8(method.parameterMetaType(p).name())}});
        const auto signature = name + '(' +
                               QString::fromLatin1(method.parameterTypes().join(", ")) + ')';
        if (seen.contains(signature)) continue;
        seen.append(signature);
        rows.append(QVariantMap{
            {QStringLiteral("op"), name},
            {QStringLiteral("args"), arguments},
            {QStringLiteral("returns"),
             method.returnMetaType().id() == QMetaType::Void
                 ? QString()
                 : QString::fromUtf8(method.returnMetaType().name())}});
    }
    // The table model's own operations, for the table or chart selected.
    const QMetaObject *table = &TableModel::staticMetaObject;
    for (int i = table->methodOffset(); i < table->methodCount(); ++i) {
        const auto method = table->method(i);
        if (!runnable(method)) continue;
        const auto name = kTable + QString::fromLatin1(method.name());
        if (!Cli::refusal(name).isEmpty()) continue;
        if (!filter.isEmpty() && !name.contains(filter, Qt::CaseInsensitive)) continue;
        const auto signature = name + '(' +
                               QString::fromLatin1(method.parameterTypes().join(", ")) + ')';
        if (seen.contains(signature)) continue;
        seen.append(signature);
        QVariantList arguments;
        for (int p = 0; p < method.parameterCount(); ++p)
            arguments.append(QVariantMap{
                {QStringLiteral("name"), QString::fromLatin1(method.parameterNames().value(p))},
                {QStringLiteral("type"), QString::fromUtf8(method.parameterMetaType(p).name())}});
        rows.append(QVariantMap{
            {QStringLiteral("op"), name},
            {QStringLiteral("args"), arguments},
            {QStringLiteral("returns"),
             method.returnMetaType().id() == QMetaType::Void
                 ? QString()
                 : QString::fromUtf8(method.returnMetaType().name())},
            {QStringLiteral("acts on"), QStringLiteral("the selected table or chart")}});
    }
    if (filter.isEmpty() || QStringLiteral("table.setCells").contains(filter, Qt::CaseInsensitive))
        rows.append(QVariantMap{
            {QStringLiteral("op"), QStringLiteral("table.setCells")},
            {QStringLiteral("args"),
             QVariantList{QVariantMap{{QStringLiteral("name"), QStringLiteral("rows")},
                                      {QStringLiteral("type"), QStringLiteral("list of lists of text")}},
                          QVariantMap{{QStringLiteral("name"), QStringLiteral("row")},
                                      {QStringLiteral("type"), QStringLiteral("int, optional, 0")}},
                          QVariantMap{{QStringLiteral("name"), QStringLiteral("column")},
                                      {QStringLiteral("type"), QStringLiteral("int, optional, 0")}}}},
            {QStringLiteral("returns"), QStringLiteral("bool")},
            {QStringLiteral("acts on"), QStringLiteral("the selected table or chart")},
            {QStringLiteral("about"),
             QStringLiteral("Writes the grid starting at row/column, growing the table to fit. "
                            "From the top-left corner the grid replaces the whole table, extra "
                            "rows and columns included. For a chart, row 0 is the series names "
                            "and column 0 the categories.")}});

    // Properties that can be written are operations too, under the name an agent
    // would guess.
    for (int i = 0; i < meta->propertyCount(); ++i) {
        const auto declared = meta->property(i);
        if (!declared.isWritable()) continue;
        auto name = QString::fromLatin1(declared.name());
        name[0] = name.at(0).toUpper();
        name.prepend(QStringLiteral("set"));
        if (!filter.isEmpty() && !name.contains(filter, Qt::CaseInsensitive)) continue;
        if (seen.contains(name)) continue;
        seen.append(name);
        rows.append(QVariantMap{
            {QStringLiteral("op"), name},
            {QStringLiteral("args"),
             QVariantList{QVariantMap{{QStringLiteral("name"), QStringLiteral("value")},
                                      {QStringLiteral("type"),
                                       QString::fromUtf8(declared.metaType().name())}}}},
            {QStringLiteral("returns"), QStringLiteral("bool")},
            {QStringLiteral("property"), QString::fromLatin1(declared.name())}});
    }

    // And the two that are not methods at all: reading and writing a property.
    for (const auto &built : {QStringLiteral("set"), QStringLiteral("get")}) {
        if (!filter.isEmpty() && !built.contains(filter, Qt::CaseInsensitive)) continue;
        rows.append(QVariantMap{
            {QStringLiteral("op"), built},
            {QStringLiteral("args"),
             built == QStringLiteral("set")
                 ? QVariantList{QVariantMap{{QStringLiteral("name"), QStringLiteral("property")},
                                            {QStringLiteral("type"), QStringLiteral("QString")}},
                                QVariantMap{{QStringLiteral("name"), QStringLiteral("value")},
                                            {QStringLiteral("type"), QStringLiteral("QVariant")}}}
                 : QVariantList{QVariantMap{{QStringLiteral("name"), QStringLiteral("property")},
                                            {QStringLiteral("type"), QStringLiteral("QString")}}}},
            {QStringLiteral("returns"), built == QStringLiteral("set")
                                            ? QStringLiteral("bool")
                                            : QStringLiteral("QVariant")}});
    }
    return rows;
}

static Cli::OpResult runResolved(Backend &backend, const QVariantMap &op, const QString &name);

Cli::OpResult Cli::runOperation(Backend &backend, const QVariantMap &op) {
    OpResult result;
    const auto name = op.value(QStringLiteral("op")).toString();
    if (name.isEmpty()) {
        result.error = QStringLiteral("every operation needs an \"op\".");
        return result;
    }
    const auto no = Cli::refusal(name);
    if (!no.isEmpty()) { result.error = no; return result; }

    // Conveniences worth having, because almost every edit wants them first.
    // Each is checked: aiming at a slide or an object that is not there must
    // stop here, not quietly leave the next edit acting on something else.
    if (op.contains(QStringLiteral("slide"))) {
        bool number = false;
        const int slide = op.value(QStringLiteral("slide")).toInt(&number);
        if (!number || slide < 0 || slide >= backend.slideCount()) {
            result.error = QStringLiteral("there is no slide %1; the deck has %2, counted from 0.")
                               .arg(op.value(QStringLiteral("slide")).toString())
                               .arg(backend.slideCount());
            return result;
        }
        backend.setCurrentSlide(slide);
    }
    if (op.contains(QStringLiteral("select"))) {
        const auto wanted = op.value(QStringLiteral("select"));
        QStringList ids;
        if (wanted.metaType().id() == QMetaType::QVariantList)
            for (const auto &row : wanted.toList()) ids.append(row.toString());
        else
            ids.append(wanted.toString());
        if (ids.size() == 1) backend.select(ids.first());
        else backend.selectIds(ids);
        QStringList missing;
        const auto selected = backend.selectedIds();
        for (const auto &id : ids)
            if (!selected.contains(id)) missing.append(id);
        if (!missing.isEmpty()) {
            result.error = QStringLiteral("nothing called %1 on slide %2. Objects are found by the "
                                          "id `omashow inspect` gives, on the current slide; add "
                                          "\"slide\": N to aim at another.")
                               .arg(missing.join(QStringLiteral(", ")))
                               .arg(backend.currentSlide());
            return result;
        }
    }

    // What the deck looked like before, and anything the backend says went
    // wrong while the operation runs: both make the answer honest.
    const int before = backend.revision();
    QString complaint;
    const auto listening = QObject::connect(&backend, &Backend::failed, &backend,
                                            [&complaint](const QString &message) {
                                                if (complaint.isEmpty()) complaint = message;
                                            });
    result = runResolved(backend, op, name);
    QObject::disconnect(listening);
    result.changed = backend.revision() != before;
    if (!complaint.isEmpty()) {
        if (result.ok && !result.changed) result.ok = false;
        if (!result.ok) result.error = complaint;
    }
    // An edit that answers nothing and changed nothing is usually a key, a
    // value or a selection that did not suit it. It may also simply have been
    // the case already, so this is a warning, not a refusal.
    static const QStringList edits{
        QStringLiteral("set"),     QStringLiteral("add"),      QStringLiteral("remove"),
        QStringLiteral("delete"),  QStringLiteral("move"),     QStringLiteral("apply"),
        QStringLiteral("insert"),  QStringLiteral("format"),   QStringLiteral("rename"),
        QStringLiteral("replace"), QStringLiteral("resize"),   QStringLiteral("nudge"),
        QStringLiteral("raise"),   QStringLiteral("align"),    QStringLiteral("distribute"),
        QStringLiteral("group"),   QStringLiteral("ungroup"),  QStringLiteral("put"),
        QStringLiteral("take"),    QStringLiteral("teach"),    QStringLiteral("forget"),
        QStringLiteral("combine"), QStringLiteral("connect"),  QStringLiteral("duplicate")};
    const auto bare = name.startsWith(kTable) ? name.mid(kTable.size()) : name;
    const bool looksLikeAnEdit = std::any_of(edits.cbegin(), edits.cend(), [&bare](const QString &verb) {
        return bare.startsWith(verb);
    });
    if (result.ok && !result.changed && !result.value.isValid() && looksLikeAnEdit)
        result.warning = QStringLiteral("%1 ran but changed nothing: check the key, the value and "
                                        "what is selected, unless it was already so.").arg(name);
    return result;
}

static Cli::OpResult runResolved(Backend &backend, const QVariantMap &op, const QString &name) {
    Cli::OpResult result;

    const auto given = op.value(QStringLiteral("args"));
    QVariantList positional;
    QVariantMap named;
    if (given.metaType().id() == QMetaType::QVariantList) positional = given.toList();
    else if (given.metaType().id() == QMetaType::QVariantMap) named = given.toMap();
    else if (given.isValid()) positional = QVariantList{given};

    // Reading and writing a property, which no method covers.
    if (name == QStringLiteral("set") || name == QStringLiteral("get")) {
        const auto property = positional.value(0).toString();
        const int index = Backend::staticMetaObject.indexOfProperty(property.toLatin1().constData());
        if (index < 0) {
            result.error = QStringLiteral("there is no property called %1.").arg(property);
            return result;
        }
        const auto declared = Backend::staticMetaObject.property(index);
        if (name == QStringLiteral("get")) {
            result.ok = true;
            result.value = declared.read(&backend);
            return result;
        }
        if (!declared.isWritable()) {
            result.error = QStringLiteral("%1 can be read but not set.").arg(property);
            return result;
        }
        result.ok = declared.write(&backend, positional.value(1));
        result.value = result.ok;
        if (!result.ok) result.error = QStringLiteral("%1 refused that value.").arg(property);
        return result;
    }

    // A whole grid at once, which is what an agent has in hand: the table model
    // takes it as pasted text from the chosen cell, growing the table to fit.
    if (name == QStringLiteral("table.setCells")) {
        auto *table = backend.tableModel();
        const auto type = backend.selectionCount() == 1
                              ? backend.selection().value(QStringLiteral("type")).toString()
                              : QString();
        if (type != QStringLiteral("table") && type != QStringLiteral("chart")) {
            result.error = QStringLiteral("select one table or chart first.");
            return result;
        }
        const auto grid = named.isEmpty() ? positional.value(0) : named.value(QStringLiteral("rows"));
        QVector<QStringList> rows;
        for (const auto &row : grid.toList()) {
            QStringList cells;
            for (const auto &cell : row.toList()) cells.append(cell.toString());
            if (cells.isEmpty()) { result.error = QStringLiteral("every row needs at least one cell."); return result; }
            rows.append(cells);
        }
        if (rows.isEmpty()) {
            result.error = QStringLiteral("table.setCells takes rows: a list of lists of text.");
            return result;
        }
        const int row = (named.isEmpty() ? positional.value(1) : named.value(QStringLiteral("row"))).toInt();
        const int column = (named.isEmpty() ? positional.value(2) : named.value(QStringLiteral("column"))).toInt();
        table->selectCell(row, column);
        const auto text = Delimited::write(rows, QLatin1Char('\t'));
        const auto preview = table->previewPaste(text, '\t');
        if (!preview.value(QStringLiteral("ok")).toBool()) {
            result.error = preview.value(QStringLiteral("error"), QStringLiteral("those cells do not fit")).toString();
            return result;
        }
        result.ok = table->pasteText(text, '\t');
        result.value = result.ok;
        if (!result.ok) { result.error = QStringLiteral("the table refused those cells."); return result; }
        // Written from the top-left corner, the grid is the whole table: rows
        // and columns beyond it would be stale data nobody asked to keep.
        if (row == 0 && column == 0) {
            int width = 0;
            for (const auto &cells : rows) width = qMax(width, int(cells.size()));
            while (table->rowCount() > rows.size() && table->changeAxis(true, table->rowCount() - 1, true)) {}
            while (table->columnCount() > width && table->changeAxis(false, table->columnCount() - 1, true)) {}
        }
        return result;
    }

    // The property most often mistyped: say so, and what there is instead.
    if (name == QStringLiteral("setSelectedProperty")) {
        const auto key = (named.isEmpty() ? positional.value(0) : named.value(QStringLiteral("name"))).toString();
        if (!backend.hasSelection()) {
            result.error = QStringLiteral("nothing is selected; pass \"select\": \"<id>\" with the operation.");
            return result;
        }
        if (!key.isEmpty() && !backend.selection().contains(key)) {
            result.error = QStringLiteral("%1 is not a property of the selected %2. `omashow ops` "
                                          "lists each kind of object's properties under "
                                          "vocabulary.objectProperties.")
                               .arg(key, backend.selection().value(QStringLiteral("type")).toString());
            return result;
        }
    }

    const auto target = targetFor(backend, name);
    const auto candidates = methodsNamed(target.meta, target.method);
    if (candidates.isEmpty()) {
        const int property = target.object == &backend ? propertyBehindSetter(name) : -1;
        if (property >= 0) {
            const auto declared = Backend::staticMetaObject.property(property);
            if (positional.size() != 1) {
                result.error = QStringLiteral("%1 takes one value.").arg(name);
                return result;
            }
            QVariant value;
            QString why;
            if (!convert(positional.first(), declared.metaType(), &value, &why)) {
                result.error = why;
                return result;
            }
            result.ok = declared.write(&backend, value);
            result.value = declared.read(&backend);
            if (!result.ok)
                result.error = QStringLiteral("%1 refused that value.").arg(name);
            return result;
        }
        result.error = QStringLiteral("there is no operation called %1. `omashow ops` lists them.")
                           .arg(name);
        return result;
    }

    QString why;
    for (const auto &method : candidates) {
        if (!named.isEmpty()) {
            // Named arguments, in the order the method declares them.
            positional.clear();
            bool complete = true;
            for (int p = 0; p < method.parameterCount(); ++p) {
                const auto parameter = QString::fromLatin1(method.parameterNames().value(p));
                if (!named.contains(parameter)) { complete = false; break; }
                positional.append(named.value(parameter));
            }
            if (!complete || positional.size() != named.size()) {
                why = QStringLiteral("%1 takes %2")
                          .arg(name, QString::fromLatin1(method.parameterNames().join(", ")));
                continue;
            }
        }
        if (positional.size() != method.parameterCount()) {
            why = QStringLiteral("%1 takes %2 argument(s), not %3")
                      .arg(name).arg(method.parameterCount()).arg(positional.size());
            continue;
        }
        QVariant converted[8];
        QGenericArgument passed[8];
        bool ready = true;
        for (int p = 0; p < method.parameterCount(); ++p) {
            if (!convert(positional.at(p), method.parameterMetaType(p), &converted[p], &why)) {
                ready = false;
                break;
            }
            // A QVariant parameter is handed the variant itself, by address;
            // anything else is handed what is inside it.
            passed[p] = method.parameterMetaType(p).id() == QMetaType::QVariant
                            ? QGenericArgument("QVariant", &converted[p])
                            : QGenericArgument(converted[p].metaType().name(),
                                               converted[p].constData());
        }
        if (!ready) continue;
        const bool nothingBack = method.returnMetaType().id() == QMetaType::Void;
        QVariant returned(method.returnMetaType());
        const bool called =
            nothingBack
                ? method.invoke(target.object, Qt::DirectConnection, passed[0], passed[1], passed[2],
                                passed[3], passed[4], passed[5], passed[6], passed[7])
                : method.invoke(target.object, Qt::DirectConnection,
                                QGenericReturnArgument(method.returnMetaType().name(),
                                                       returned.data()),
                                passed[0], passed[1], passed[2], passed[3], passed[4], passed[5],
                                passed[6], passed[7]);
        if (!called) {
            result.error = QStringLiteral("%1 could not be called.").arg(name);
            return result;
        }
        result.ok = true;
        if (!nothingBack) {
            result.value = returned;
            // An operation that answers "no" is not a failure to call, but it is
            // worth saying so plainly.
            if (returned.metaType().id() == QMetaType::Bool && !returned.toBool()) {
                result.ok = false;
                result.error = QStringLiteral("%1 refused: the selection, the arguments or the "
                                              "state of the deck did not suit it.").arg(name);
            }
        }
        return result;
    }
    result.error = why.isEmpty() ? QStringLiteral("%1 did not suit those arguments.").arg(name) : why;
    return result;
}
