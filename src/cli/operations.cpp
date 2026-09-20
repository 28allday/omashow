#include "cli/operations.h"
#include "backend.h"

#include <QMetaMethod>
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
    };
    return table;
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

QVector<QMetaMethod> methodsNamed(const QString &name) {
    QVector<QMetaMethod> found;
    const QMetaObject *meta = &Backend::staticMetaObject;
    for (int i = 0; i < meta->methodCount(); ++i) {
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
    if (op.contains(QStringLiteral("slide")))
        backend.setCurrentSlide(op.value(QStringLiteral("slide")).toInt());
    if (op.contains(QStringLiteral("select"))) {
        const auto wanted = op.value(QStringLiteral("select"));
        if (wanted.metaType().id() == QMetaType::QVariantList) {
            QStringList ids;
            for (const auto &row : wanted.toList()) ids.append(row.toString());
            backend.selectIds(ids);
        } else {
            backend.select(wanted.toString());
        }
    }

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

    const auto candidates = methodsNamed(name);
    if (candidates.isEmpty()) {
        const int property = propertyBehindSetter(name);
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
                ? method.invoke(&backend, Qt::DirectConnection, passed[0], passed[1], passed[2],
                                passed[3], passed[4], passed[5], passed[6], passed[7])
                : method.invoke(&backend, Qt::DirectConnection,
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
