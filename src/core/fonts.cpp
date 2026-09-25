#include "core/fonts.h"

#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QRegularExpression>
#include <QVariantMap>

namespace {

template <typename F>
void eachObject(Document &document, F visit) {
    for (auto &slide : document.slides) for (auto &o : slide.objects) visit(o);
    for (auto &master : document.masters) for (auto &o : master.objects) visit(o);
    for (auto &layout : document.layouts) for (auto &o : layout.placeholders) visit(o);
    for (auto &style : document.textStyles) visit(style.look);
    for (auto &style : document.objectStyles) visit(style.appearance);
}

// Every place one object names a family, with a way to change it.
template <typename F>
void eachFamily(SceneObject &o, F visit) {
    visit(o.fontFamily);
    for (auto &run : o.runs) if (!run.fontFamily.isEmpty()) visit(run.fontFamily);
    for (auto &cell : o.table.cells) {
        auto it = cell.style.find(QStringLiteral("fontFamily"));
        if (it == cell.style.end()) continue;
        QString family = it->toString();
        visit(family);
        if (family != it->toString()) *it = family;
    }
}

bool looksSerif(const QString &family) {
    static const QRegularExpression serif(QStringLiteral(
        "serif|times|georgia|garamond|lora|merriweather|playfair|baskerville|palatino|book antiqua|cambria|charter|"
        "minion|caslon|didot|bodoni|libre|crimson|spectral|source serif|pt serif|literata|tinos|liberation serif|dejavu serif"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression sans(QStringLiteral("sans"), QRegularExpression::CaseInsensitiveOption);
    return serif.match(family).hasMatch() && !sans.match(family).hasMatch();
}

bool looksMono(const QString &family) {
    static const QRegularExpression mono(QStringLiteral("mono|code|courier|consolas|menlo|monaco|fira code|jetbrains|inconsolata|hack\\b"),
                                         QRegularExpression::CaseInsensitiveOption);
    return mono.match(family).hasMatch();
}

} // namespace

QMap<QString, int> Fonts::families(const Document &document) {
    QMap<QString, int> out;
    Document copy = document;
    eachObject(copy, [&out](SceneObject &o) {
        eachFamily(o, [&out](QString &family) { if (!family.isEmpty()) ++out[family]; });
    });
    for (const auto &family : document.theme.fonts) if (!family.isEmpty()) ++out[family];
    return out;
}

QStringList Fonts::missing(const Document &document) {
    QStringList out;
    const auto all = families(document);
    for (auto it = all.cbegin(); it != all.cend(); ++it)
        if (!QFontDatabase::hasFamily(it.key())) out.append(it.key());
    return out;
}

QString Fonts::suggested(const QString &family) {
    if (QFontDatabase::hasFamily(family)) return family;
    // "Inter SemiBold" is Inter with a weight in its name; "Roboto Serif 14pt"
    // is Roboto Serif at a size. Strip such words and try the stem.
    static const QRegularExpression weightWords(QStringLiteral(
        "\\s+(thin|extra ?light|ultra ?light|light|regular|book|normal|medium|semi ?bold|demi ?bold|bold|extra ?bold|ultra ?bold|black|heavy|"
        "italic|oblique|condensed|expanded|narrow|display|text|\\d+pt|\\d+)$"),
        QRegularExpression::CaseInsensitiveOption);
    QString stem = family.trimmed();
    for (int i = 0; i < 4; ++i) {
        const auto shorter = stem;
        stem.remove(weightWords);
        stem = stem.trimmed();
        if (stem == shorter || stem.isEmpty()) break;
        if (QFontDatabase::hasFamily(stem)) return stem;
    }
    // Otherwise the installed face nearest in kind.
    QFont font(family);
    font.setStyleHint(looksMono(family) ? QFont::Monospace : looksSerif(family) ? QFont::Serif : QFont::SansSerif);
    const QString nearest = QFontInfo(font).family();
    if (!nearest.isEmpty() && QFontDatabase::hasFamily(nearest)) return nearest;
    const auto installed = QFontDatabase::families();
    return installed.isEmpty() ? QStringLiteral("Inter") : installed.first();
}

QVariantList Fonts::report(const Document &document) {
    QVariantList rows;
    const auto all = families(document);
    for (auto it = all.cbegin(); it != all.cend(); ++it) {
        if (QFontDatabase::hasFamily(it.key())) continue;
        rows.append(QVariantMap{{QStringLiteral("family"), it.key()},
                                {QStringLiteral("uses"), it.value()},
                                {QStringLiteral("suggested"), suggested(it.key())}});
    }
    return rows;
}

int Fonts::substitute(Document &document, const QMap<QString, QString> &replacements) {
    int changed = 0;
    const auto swap = [&](QString &family) {
        const auto it = replacements.constFind(family);
        if (it == replacements.cend() || it.value().isEmpty() || it.value() == family) return;
        family = it.value();
        ++changed;
    };
    eachObject(document, [&](SceneObject &o) { eachFamily(o, swap); });
    for (auto it = document.theme.fonts.begin(); it != document.theme.fonts.end(); ++it) swap(it.value());
    return changed;
}
