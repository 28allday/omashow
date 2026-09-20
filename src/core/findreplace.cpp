#include "core/findreplace.h"
#include "core/design.h"
#include <QSet>

namespace {

struct Options {
    Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
    bool wholeWords = false, notes = true;
    QSet<QString> slides;
};

Options optionsFrom(const QVariantMap &map) {
    Options options;
    options.sensitivity = map.value("caseSensitive").toBool() ? Qt::CaseSensitive
                                                              : Qt::CaseInsensitive;
    options.wholeWords = map.value("wholeWords").toBool();
    options.notes = map.value("notes", true).toBool();
    const auto slides = map.value("slides").toStringList();
    options.slides = QSet<QString>(slides.cbegin(), slides.cend());
    return options;
}

bool wordBoundary(const QString &haystack, int start, int length) {
    const auto letter = [](QChar c) { return c.isLetterOrNumber() || c == '_'; };
    if (start > 0 && letter(haystack.at(start - 1))) return false;
    const int after = start + length;
    return after >= haystack.size() || !letter(haystack.at(after));
}

QList<int> positions(const QString &haystack, const QString &needle, const Options &options) {
    QList<int> found;
    if (needle.isEmpty()) return found;
    int at = haystack.indexOf(needle, 0, options.sensitivity);
    while (at >= 0) {
        if (!options.wholeWords || wordBoundary(haystack, at, needle.size())) found.append(at);
        at = haystack.indexOf(needle, at + 1, options.sensitivity);
    }
    return found;
}

// Enough of the line around a match to recognise it in a list.
QString contextAround(const QString &haystack, int start, int length) {
    const int from = qMax(0, start - 24);
    const int to = qMin(haystack.size(), start + length + 24);
    QString context = haystack.mid(from, to - from).simplified();
    if (from > 0) context.prepend(QStringLiteral("…"));
    if (to < haystack.size()) context.append(QStringLiteral("…"));
    return context;
}

// The text a match points at, or nothing if it no longer points anywhere.
QString *textAt(Document &d, const QVariantMap &match) {
    const QString slideId = match.value("slideId").toString();
    for (auto &slide : d.slides) {
        if (slide.id != slideId) continue;
        if (match.value("notes").toBool()) return &slide.notes;
        auto *object = slide.find(match.value("objectId").toString());
        if (!object) return nullptr;
        const auto cellId = match.value("cellId").toString();
        if (cellId.isEmpty())
            return object->type == ObjectType::Text ? &object->text : nullptr;
        for (auto &cell : object->table.cells)
            if (cell.id == cellId) return &cell.text;
        return nullptr;
    }
    return nullptr;
}

} // namespace

QVariantList FindReplace::find(const Document &d, const QString &needle,
                               const QVariantMap &optionMap) {
    const Options options = optionsFrom(optionMap);
    QVariantList rows;
    if (needle.isEmpty()) return rows;
    for (int i = 0; i < d.slides.size(); ++i) {
        const auto &slide = d.slides.at(i);
        if (!options.slides.isEmpty() && !options.slides.contains(slide.id)) continue;
        const auto row = [&](const QString &objectId, const QString &cellId, bool notes,
                             const QString &haystack, const QString &label) {
            for (int at : positions(haystack, needle, options))
                rows.append(QVariantMap{{"slide", i}, {"slideId", slide.id},
                                        {"objectId", objectId}, {"cellId", cellId},
                                        {"notes", notes}, {"start", at},
                                        {"length", needle.size()},
                                        {"context", contextAround(haystack, at, needle.size())},
                                        {"label", label}});
        };
        for (const auto &o : slide.objects) {
            if (o.type == ObjectType::Text) row(o.id, QString(), false, o.text, "Text");
            for (const auto &cell : o.table.cells)
                row(o.id, cell.id, false,
                    cell.text, o.type == ObjectType::Chart ? "Chart data" : "Table");
        }
        if (options.notes) row(QString(), QString(), true, slide.notes, "Notes");
    }
    return rows;
}

bool FindReplace::replaceOne(Document &d, const QVariantMap &match, const QString &needle,
                             const QString &replacement, const QVariantMap &optionMap) {
    const Options options = optionsFrom(optionMap);
    if (needle.isEmpty() || replacement.contains(QChar(0))) return false;
    QString *text = textAt(d, match);
    if (!text) return false;
    const int start = match.value("start").toInt(), length = match.value("length").toInt();
    if (start < 0 || length <= 0 || start + length > text->size()) return false;
    // A match found before an edit must not be applied to different words.
    if (text->mid(start, length).compare(needle, options.sensitivity) != 0) return false;
    if (options.wholeWords && !wordBoundary(*text, start, length)) return false;
    text->replace(start, length, replacement);
    if (!match.value("notes").toBool() && match.value("cellId").toString().isEmpty())
        for (auto &slide : d.slides)
            if (slide.id == match.value("slideId").toString())
                if (auto *object = slide.find(match.value("objectId").toString()))
                    Design::markOverride(*object, QStringLiteral("text"));
    return true;
}

int FindReplace::replaceAll(Document &d, const QString &needle, const QString &replacement,
                            const QVariantMap &optionMap) {
    const auto matches = find(d, needle, optionMap);
    int replaced = 0;
    // Backwards, so the positions of the matches still ahead stay true.
    for (int i = matches.size() - 1; i >= 0; --i)
        if (replaceOne(d, matches.at(i).toMap(), needle, replacement, optionMap)) ++replaced;
    return replaced;
}
