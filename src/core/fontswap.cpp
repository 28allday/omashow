#include "core/fontswap.h"

#include <QSet>

#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFontInfo>
#include <QMap>
#include <QMutex>
#include <memory>
#include <QVariantMap>
#include <cmath>

namespace {

struct Entry {
    QString name;                  // as the table spells it
    int weight = 0;
    qreal ascent = -1, descent = 0, gap = 0;   // em, as PowerPoint lays the Office font out
    qreal xHeight = 0, capHeight = 0, sentence = 0;   // em; sentence: width of kSentence
    qreal advance[3] = {0, 0, 0};  // em per character of kCorpus, kerned: regular, bold, italic
    QChar kind = QLatin1Char('s');           // s sans, r serif, m mono, h script, d decorative
    QVector<FontSwap::Option> options;
};

const QHash<QString, Entry> &table() {
    static const QHash<QString, Entry> loaded = [] {
        QHash<QString, Entry> out;
        QFile file(QStringLiteral(":/fonts/office-font-map.json"));
        if (!file.open(QIODevice::ReadOnly)) return out;
        const auto fonts = QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("fonts")).toObject();
        for (auto it = fonts.begin(); it != fonts.end(); ++it) {
            const auto json = it.value().toObject();
            Entry e;
            e.name = it.key();
            e.weight = json.value(QStringLiteral("wt")).toInt();
            if (const auto xm = json.value(QStringLiteral("xm")).toArray(); xm.size() == 3) {
                e.xHeight = xm[0].toDouble(); e.capHeight = xm[1].toDouble(); e.sentence = xm[2].toDouble();
            }
            if (const auto aw = json.value(QStringLiteral("aw")).toArray(); aw.size() == 3)
                for (int i = 0; i < 3; ++i) e.advance[i] = aw[i].toDouble();
            if (const auto c = json.value(QStringLiteral("c")).toString(); c.size() == 1) e.kind = c.at(0);
            if (const auto va = json.value(QStringLiteral("va")).toArray(); va.size() == 3 && va[0].toDouble() > 0) {
                e.ascent = va[0].toDouble(); e.descent = va[1].toDouble(); e.gap = va[2].toDouble();
            }
            for (const auto &value : json.value(QStringLiteral("o")).toArray()) {
                const auto o = value.toObject();
                FontSwap::Option option;
                option.family = o.value(QStringLiteral("f")).toString();
                option.metric = o.value(QStringLiteral("m")).toInt() == 1;
                option.size = o.value(QStringLiteral("s")).toDouble(1);
                option.width = o.value(QStringLiteral("w")).toDouble(1);
                option.line = o.value(QStringLiteral("l")).toDouble(1);
                option.package = o.value(QStringLiteral("pkg")).toString();
                option.googleFonts = o.value(QStringLiteral("gf")).toString();
                option.github = o.value(QStringLiteral("gh")).toString();
                if (option.family.isEmpty() || !(option.size > 0.2 && option.size < 5)) continue;
                e.options.append(option);
            }
            if (!e.options.isEmpty()) out.insert(it.key().toLower(), e);
        }
        return out;
    }();
    return loaded;
}

bool installed(const QString &family) { return QFontDatabase::hasFamily(family); }

// Sizes written back must be the ones read: a size untouched since the swap
// is restored from what was stored, not divided back with rounding.
bool same(qreal a, qreal b) { return std::abs(a - b) <= 1e-6 * qMax<qreal>(1.0, std::abs(a)); }

// A size untouched since the swap goes back to exactly what was read; one
// changed in OmaShow is divided by the factor and rounded to PowerPoint's half
// point (the field shows sizes in half points too).
// ptPerUnit: points per slide unit of the deck being saved (its PowerPoint
// slide is 960 pt wide as written).
qreal unscale(qreal now, qreal before, qreal scale, qreal ptPerUnit) {
    if (before > 0 && same(now, before * scale)) return before;
    if (ptPerUnit <= 0) return now / scale;
    const qreal points = std::max<qreal>(1.0, std::round(now / scale * ptPerUnit * 2) / 2);
    return points / ptPerUnit;
}

// Whether a family is still the free match that stood in for an Office
// family, rather than one chosen since.
// fallbacks: the deck's record of fallbacks used (theme.fontFallbacks).
// The recorded fallback for an Office family, whatever its spelling.
QString recorded(const QMap<QString, QString> *fallbacks, const QString &office) {
    if (!fallbacks) return {};
    for (auto it = fallbacks->cbegin(); it != fallbacks->cend(); ++it)
        if (it.key().compare(office, Qt::CaseInsensitive) == 0) return it.value();
    return {};
}

bool stillSwapped(const QString &office, const QString &family, const QMap<QString, QString> *fallbacks = nullptr) {
    for (const auto &option : FontSwap::options(office))
        if (option.family.compare(family, Qt::CaseInsensitive) == 0) return true;
    if (fallbacks) return !family.isEmpty() && recorded(fallbacks, office).compare(family, Qt::CaseInsensitive) == 0;
    // Drawing (no deck at hand): the fallback this computer would pick.
    const auto *fb = FontSwap::fallback(office);
    return fb && fb->family.compare(family, Qt::CaseInsensitive) == 0;
}

// The option (table or fallback) a family stands in with, or null.
const FontSwap::Option *optionFor(const QString &office, const QString &family) {
    const auto entry = table().constFind(office.toLower());
    if (entry != table().cend())
        for (const auto &option : entry->options)
            if (option.family.compare(family, Qt::CaseInsensitive) == 0) return &option;
    const auto *fb = FontSwap::fallback(office);
    return fb && fb->family.compare(family, Qt::CaseInsensitive) == 0 ? fb : nullptr;
}

template <typename D, typename F>
void eachObject(D &document, F visit) {
    for (auto &slide : document.slides) for (auto &o : slide.objects) visit(o);
    for (auto &master : document.masters) for (auto &o : master.objects) visit(o);
    for (auto &layout : document.layouts) for (auto &o : layout.placeholders) visit(o);
    for (auto &style : document.textStyles) visit(style.look);
    for (auto &style : document.objectStyles) visit(style.appearance);
}

// office → free → uses
using Tally = QMap<QString, QMap<QString, int>>;

QVariantList tallyList(const Tally &tally) {
    QVariantList rows;
    for (auto it = tally.cbegin(); it != tally.cend(); ++it)
        for (auto f = it->cbegin(); f != it->cend(); ++f) {
            bool metric = false; qreal size = 1;
            if (const auto *o = optionFor(it.key(), f.key())) { metric = o->metric; size = o->size; }
            rows.append(QVariantMap{{QStringLiteral("office"), it.key()}, {QStringLiteral("free"), f.key()},
                                    {QStringLiteral("metric"), metric}, {QStringLiteral("size"), metric ? 1.0 : size},
                                    {QStringLiteral("uses"), f.value()}});
        }
    return rows;
}

class Swapper {
public:
    FontSwap::Report report;
    Tally tally;
    QMap<QString, QString> *fallbacks = nullptr;   // the deck's record of fallbacks used
    // The record as it was when this pass began: an upgrade early in the pass
    // erases the record, and later places (theme slots) must still know the
    // family they hold was a fallback.
    QMap<QString, QString> recordedBefore;

    const FontSwap::Option *pick(const QString &family) {
        if (family.isEmpty()) return nullptr;
        const auto key = family.toLower();
        const auto cached = cache.constFind(key);
        if (cached != cache.cend()) return *cached;
        const FontSwap::Option *chosen = nullptr;
        if (!installed(family)) {
            const auto entry = table().constFind(key);
            if (entry != table().cend())
                for (const auto &option : entry->options)
                    if (installed(option.family)) { chosen = &option; break; }
            // None installed: an installed fallback, sized to match.
            if (!chosen && entry != table().cend()) chosen = FontSwap::fallback(family);
        }
        cache.insert(key, chosen);
        return chosen;
    }
    // A fallback is sized for the style it is drawn in (bold and italic set
    // wider or narrower than regular); table matches have one size.
    static const FontSwap::Option *styled(const FontSwap::Option *o, const QString &office, int style) {
        if (!o || !o->fallback) return o;
        const auto *s = FontSwap::fallback(office, style);
        return s && s->family.compare(o->family, Qt::CaseInsensitive) == 0 ? s : o;
    }
    // The style most of a box's text is in (its own weight may be the first
    // stretch's, e.g. a bold first word).
    static int boxStyle(const SceneObject &o) {
        int count[3] = {0, 0, 0};
        const int length = o.text.size();
        int covered = 0;
        for (const auto &run : o.runs) {
            const int n = qBound(0, qMin(run.length, length - run.start), length);
            if (n <= 0) continue;
            covered += n;
            if (!run.fontFamily.isEmpty()) continue;   // sized on its own
            count[FontSwap::styleOf(run.weight ? run.weight : o.fontWeight, run.italic ? run.italic == 1 : o.italic)] += n;
        }
        count[FontSwap::styleOf(o.fontWeight, o.italic)] += qMax(0, length - covered);
        return count[1] > count[0] && count[1] >= count[2] ? 1 : count[2] > count[0] ? 2 : 0;
    }
    static bool scales(const FontSwap::Option *o) { return o && !o->metric && !qFuzzyCompare(o->size, 1.0); }

    // A family swapped earlier for a lesser match (the best was not installed
    // then): the best installed match now, or null when it already is that.
    const FontSwap::Option *better(const QString &office, const QString &current) {
        const auto entry = table().constFind(office.toLower());
        if (entry == table().cend()) return nullptr;
        int now = -1;
        for (int i = 0; i < entry->options.size(); ++i)
            if (entry->options[i].family.compare(current, Qt::CaseInsensitive) == 0) now = i;
        // A fallback stands in: any table match installed since is better.
        if (now < 0 && fallbacks && recorded(&recordedBefore, entry->name).compare(current, Qt::CaseInsensitive) == 0) now = entry->options.size();
        if (now <= 0) return nullptr;   // the best already, or chosen by hand since
        for (int i = 0; i < now; ++i)
            if (installed(entry->options[i].family)) return &entry->options[i];
        // A deck saved elsewhere with a fallback this computer
        // lacks: this computer's fallback instead, sized for it.
        if (now == entry->options.size() && !installed(current))
            if (const auto *fb = FontSwap::fallback(entry->name); fb && fb->family.compare(current, Qt::CaseInsensitive) != 0) return fb;
        return nullptr;
    }
    void count(const QString &office, const QString &free) {
        ++tally[office][free]; ++report.changed;
        const auto entry = table().constFind(office.toLower());
        if (!fallbacks || entry == table().cend()) return;
        // A table match stands in (a fallback can be the same family as some
        // other Office font's table match): any older record goes.
        for (const auto &option : entry->options)
            if (option.family.compare(free, Qt::CaseInsensitive) == 0) {
                for (auto it = fallbacks->begin(); it != fallbacks->end();)
                    it = it.key().compare(entry->name, Qt::CaseInsensitive) == 0 ? fallbacks->erase(it) : std::next(it);
                return;
            }
        const auto *fb = FontSwap::fallback(office);
        if (fb && fb->family.compare(free, Qt::CaseInsensitive) == 0) fallbacks->insert(entry->name, fb->family);
    }

    void object(SceneObject &o) {
        // The box's size as the deck had it, before any factor of ours.
        const qreal boxBefore = o.origFont.isEmpty() || qFuzzyCompare(o.fontScale, 1.0) ? o.fontSize : o.fontSize / o.fontScale;
        qreal boxScale = 1;
        upgrade(o, boxBefore);
        if (o.origFont.isEmpty()) if (const auto *opt = styled(pick(o.fontFamily), o.fontFamily, boxStyle(o))) {
            const QString office = o.fontFamily;
            o.origFont = office;
            o.origFontSize = o.fontSize;
            const int named = FontSwap::namedWeight(office);
            if (named && o.fontWeight == 400) { o.origWeight = o.fontWeight; o.fontWeight = named; }
            o.fontFamily = opt->family;
            if (scales(opt)) { o.fontScale = opt->size; o.fontSize = o.fontSize * opt->size; boxScale = opt->size; }
            count(office, opt->family);
        }
        for (auto &run : o.runs) {
            if (!run.fontFamily.isEmpty()) {
                if (!run.origFont.isEmpty()) continue;
                const auto *opt = styled(pick(run.fontFamily), run.fontFamily,
                                         FontSwap::styleOf(run.weight ? run.weight : o.origWeight ? o.origWeight : o.fontWeight,
                                                           run.italic ? run.italic == 1 : o.italic));
                if (!opt) {
                    // A family of its own drawn at the box's size: the box's
                    // factor is not this family's, so it keeps the deck's size.
                    if (boxScale != 1 && run.fontSize <= 0) run.fontSize = boxBefore;
                    continue;
                }
                const QString office = run.fontFamily;
                run.origFont = office;
                const int named = FontSwap::namedWeight(office);
                const int weightNow = run.weight ? run.weight : o.origWeight ? o.origWeight : o.fontWeight;
                if (named && weightNow == 400) { run.origWeight = run.weight ? run.weight : 1; run.weight = named; }
                run.fontFamily = opt->family;
                if (scales(opt)) {
                    run.fontScale = opt->size;
                    run.origFontSize = run.fontSize;   // 0: it took the box's size
                    run.fontSize = (run.fontSize > 0 ? run.fontSize : boxBefore) * opt->size;
                } else if (boxScale != 1 && run.fontSize <= 0) {
                    run.fontSize = boxBefore;
                }
                count(office, opt->family);
            } else if (boxScale != 1 && run.fontSize > 0 && qFuzzyCompare(run.fontScale, 1.0)) {
                run.origFontSize = run.fontSize;
                run.fontScale = boxScale;
                run.fontSize *= boxScale;
            }
        }
        for (auto &cell : o.table.cells) {
            auto &style = cell.style;
            const auto family = style.value(QStringLiteral("fontFamily")).toString();
            if (family.isEmpty() || style.contains(QStringLiteral("origFont"))) continue;
            const auto *opt = pick(family);
            if (!opt) continue;
            style.insert(QStringLiteral("origFont"), family);
            style.insert(QStringLiteral("fontFamily"), opt->family);
            const int named = FontSwap::namedWeight(family);
            const int weightNow = style.value(QStringLiteral("fontWeight"), 400).toInt();
            if (named && weightNow == 400) {
                style.insert(QStringLiteral("origWeight"), 400);
                style.insert(QStringLiteral("fontWeight"), named);
            }
            if (scales(opt)) {
                style.insert(QStringLiteral("fontScale"), opt->size);
                const bool own = style.contains(QStringLiteral("fontSize"));
                const qreal before = own ? style.value(QStringLiteral("fontSize")).toDouble() : boxBefore;
                if (own) style.insert(QStringLiteral("origFontSize"), before);
                style.insert(QStringLiteral("fontSize"), before * opt->size);
            }
            count(family, opt->family);
        }
    }

private:
    QHash<QString, const FontSwap::Option *> cache;

    // Moves what an earlier import swapped to a lesser match on to the best
    // one now installed, where the size is still the one the swap set.
    void upgrade(SceneObject &o, qreal boxBefore) {
        if (!o.origFont.isEmpty()) if (const auto *opt = styled(better(o.origFont, o.fontFamily), o.origFont, boxStyle(o))) {
            const bool untouched = qFuzzyCompare(o.fontScale, 1.0) ? same(o.fontSize, o.origFontSize) : same(o.fontSize, o.origFontSize * o.fontScale);
            if (untouched) {
                const qreal scale = scales(opt) ? opt->size : 1.0;
                o.fontFamily = opt->family;
                o.fontScale = scale;
                o.fontSize = o.origFontSize * scale;
                // Stretches that took the box's family were sized with its factor.
                for (auto &run : o.runs) {
                    if (!run.fontFamily.isEmpty() || qFuzzyCompare(run.fontScale, 1.0) || run.origFontSize <= 0) continue;
                    run.fontSize = run.origFontSize * scale;
                    run.fontScale = scale;
                    if (qFuzzyCompare(scale, 1.0)) run.origFontSize = 0;
                }
                if (scale != 1)
                    for (auto &run : o.runs)
                        if (run.fontFamily.isEmpty() && run.fontSize > 0 && qFuzzyCompare(run.fontScale, 1.0) && run.origFontSize <= 0) {
                            run.origFontSize = run.fontSize; run.fontScale = scale; run.fontSize *= scale;
                        }
                count(o.origFont, opt->family);
            }
        }
        for (auto &run : o.runs) {
            if (run.origFont.isEmpty()) continue;
            const auto *opt = better(run.origFont, run.fontFamily);
            if (!opt) continue;
            const bool scaled = !qFuzzyCompare(run.fontScale, 1.0);
            // The size the run had before any swap: recorded when it was
            // scaled, otherwise its own explicit size, otherwise the box's.
            qreal before;
            if (scaled) {
                before = run.origFontSize > 0 ? run.origFontSize : boxBefore;
                if (!same(run.fontSize, before * run.fontScale)) continue;   // resized since
            } else {
                if (run.origFontSize > 0 && run.fontSize > 0 && !same(run.fontSize, run.origFontSize)) continue;
                before = run.fontSize > 0 ? run.fontSize : boxBefore;
            }
            const qreal scale = scales(opt) ? opt->size : 1.0;
            run.fontFamily = opt->family;
            run.fontScale = scale;
            if (scale != 1) {
                if (!scaled && run.fontSize > 0) run.origFontSize = run.fontSize;
                run.fontSize = before * scale;
            } else if (scaled) {
                run.fontSize = run.origFontSize > 0 ? run.origFontSize : !qFuzzyCompare(o.fontScale, 1.0) ? boxBefore : 0;
            }
            count(run.origFont, opt->family);
        }
        for (auto &cell : o.table.cells) {
            auto &style = cell.style;
            const auto office = style.value(QStringLiteral("origFont")).toString();
            if (office.isEmpty()) continue;
            const auto *opt = better(office, style.value(QStringLiteral("fontFamily")).toString());
            if (!opt) continue;
            const qreal oldScale = style.value(QStringLiteral("fontScale"), 1.0).toDouble();
            const bool own = style.contains(QStringLiteral("origFontSize")) || (qFuzzyCompare(oldScale, 1.0) && style.contains(QStringLiteral("fontSize")));
            const qreal before = style.contains(QStringLiteral("origFontSize")) ? style.value(QStringLiteral("origFontSize")).toDouble()
                               : own ? style.value(QStringLiteral("fontSize")).toDouble() : boxBefore;
            if (style.contains(QStringLiteral("fontSize")) && !same(style.value(QStringLiteral("fontSize")).toDouble(), before * oldScale)) continue;
            style.insert(QStringLiteral("fontFamily"), opt->family);
            if (scales(opt)) {
                style.insert(QStringLiteral("fontScale"), opt->size);
                if (own) style.insert(QStringLiteral("origFontSize"), before);
                style.insert(QStringLiteral("fontSize"), before * opt->size);
            } else {
                style.remove(QStringLiteral("fontScale"));
                style.remove(QStringLiteral("origFontSize"));
                if (own) style.insert(QStringLiteral("fontSize"), before); else style.remove(QStringLiteral("fontSize"));
            }
            count(office, opt->family);
        }
    }
};

} // namespace

QVector<FontSwap::Option> FontSwap::options(const QString &officeFamily) {
    const auto it = table().constFind(officeFamily.toLower());
    return it == table().cend() ? QVector<Option>() : it->options;
}

int FontSwap::namedWeight(const QString &officeFamily) {
    const auto it = table().constFind(officeFamily.toLower());
    return it == table().cend() ? 0 : it->weight;
}

bool FontSwap::isOffice(const QString &family) { return table().contains(family.toLower()); }

QString FontSwap::officeFor(const QString &freeFamily) {
    // Only free families drawn with the Office family's own advance widths
    // (metric-compatible, measured), so the saved deck lays out the same in
    // PowerPoint with no size change.
    static const QHash<QString, QString> pairs = {
        {QStringLiteral("carlito"), QStringLiteral("Calibri")},
        {QStringLiteral("liberation sans"), QStringLiteral("Arial")},
        {QStringLiteral("arimo"), QStringLiteral("Arial")},
        {QStringLiteral("nimbus sans"), QStringLiteral("Arial")},
        {QStringLiteral("liberation sans narrow"), QStringLiteral("Arial Narrow")},
        {QStringLiteral("nimbus sans narrow"), QStringLiteral("Arial Narrow")},
        {QStringLiteral("liberation serif"), QStringLiteral("Times New Roman")},
        {QStringLiteral("tinos"), QStringLiteral("Times New Roman")},
        {QStringLiteral("nimbus roman"), QStringLiteral("Times New Roman")},
        {QStringLiteral("liberation mono"), QStringLiteral("Courier New")},
        {QStringLiteral("cousine"), QStringLiteral("Courier New")},
        {QStringLiteral("nimbus mono ps"), QStringLiteral("Courier New")},
        {QStringLiteral("gelasio"), QStringLiteral("Georgia")},
        {QStringLiteral("caladea"), QStringLiteral("Cambria")},
        {QStringLiteral("intos"), QStringLiteral("Aptos")},
        {QStringLiteral("intos display"), QStringLiteral("Aptos Display")},
        {QStringLiteral("intos narrow"), QStringLiteral("Aptos Narrow")},
        {QStringLiteral("intos serif"), QStringLiteral("Aptos Serif")},
        {QStringLiteral("urw gothic"), QStringLiteral("Century Gothic")},
        {QStringLiteral("p052"), QStringLiteral("Palatino Linotype")},
        {QStringLiteral("c059"), QStringLiteral("Century Schoolbook")},
        {QStringLiteral("urw bookman"), QStringLiteral("Bookman Old Style")},
        {QStringLiteral("selawik"), QStringLiteral("Segoe UI")},
    };
    return pairs.value(freeFamily.toLower());
}

FontSwap::Report FontSwap::apply(Document &document) {
    Swapper swapper;
    swapper.fallbacks = &document.theme.fontFallbacks;
    swapper.recordedBefore = document.theme.fontFallbacks;
    eachObject(document, [&](SceneObject &o) { swapper.object(o); });
    for (auto it = document.theme.fonts.begin(); it != document.theme.fonts.end(); ++it) {
        if (document.theme.fontOrigins.contains(it.key())) {
            const auto office = document.theme.fontOrigins.value(it.key());
            if (const auto *opt = swapper.better(office, it.value())) { swapper.count(office, opt->family); it.value() = opt->family; }
            continue;
        }
        if (const auto *opt = swapper.pick(it.value())) {
            document.theme.fontOrigins.insert(it.key(), it.value());
            swapper.count(it.value(), opt->family);
            it.value() = opt->family;
        }
    }
    swapper.report.swaps = tallyList(swapper.tally);
    return swapper.report;
}

void FontSwap::revert(Document &document) {
    const qreal ptPerUnit = 960.0 / qMax<qreal>(1.0, document.size.width());
    const auto *fallbacks = &document.theme.fontFallbacks;
    eachObject(document, [ptPerUnit, fallbacks](SceneObject &o) {
        bool boxSwapped = false;
        if (!o.origFont.isEmpty() && stillSwapped(o.origFont, o.fontFamily, fallbacks)) {
            boxSwapped = true;
            if (!qFuzzyCompare(o.fontScale, 1.0)) o.fontSize = unscale(o.fontSize, o.origFontSize, o.fontScale, ptPerUnit);
            if (o.origWeight && o.fontWeight == namedWeight(o.origFont)) o.fontWeight = o.origWeight;
            o.fontFamily = o.origFont;
        } else if (const auto office = officeFor(o.fontFamily); !office.isEmpty()) {
            o.fontFamily = office;
        }
        for (auto &run : o.runs) {
            if (!run.fontFamily.isEmpty()) {
                if (!run.origFont.isEmpty() && stillSwapped(run.origFont, run.fontFamily, fallbacks)) {
                    if (!qFuzzyCompare(run.fontScale, 1.0)) {
                        if (run.origFontSize > 0) run.fontSize = unscale(run.fontSize, run.origFontSize, run.fontScale, ptPerUnit);
                        else if (same(run.fontSize, o.fontSize * run.fontScale)) run.fontSize = 0;   // took the box's
                        else run.fontSize = unscale(run.fontSize, 0, run.fontScale, ptPerUnit);
                    }
                    if (run.origWeight && run.weight == namedWeight(run.origFont)) run.weight = run.origWeight == 1 ? 0 : run.origWeight;
                    run.fontFamily = run.origFont;
                } else if (const auto office = officeFor(run.fontFamily); !office.isEmpty()) {
                    run.fontFamily = office;
                }
            } else if (boxSwapped && !qFuzzyCompare(run.fontScale, 1.0) && run.fontSize > 0) {
                run.fontSize = unscale(run.fontSize, run.origFontSize, run.fontScale, ptPerUnit);
            }
            run.origFont.clear(); run.origFontSize = 0; run.fontScale = 1; run.origWeight = 0;
        }
        for (auto &cell : o.table.cells) {
            auto &style = cell.style;
            const auto family = style.value(QStringLiteral("fontFamily")).toString();
            const auto office = style.value(QStringLiteral("origFont")).toString();
            if (!office.isEmpty() && stillSwapped(office, family, fallbacks)) {
                if (style.contains(QStringLiteral("fontScale"))) {
                    const qreal scale = style.value(QStringLiteral("fontScale")).toDouble();
                    const qreal now = style.value(QStringLiteral("fontSize")).toDouble();
                    if (style.contains(QStringLiteral("origFontSize")))
                        style.insert(QStringLiteral("fontSize"), unscale(now, style.value(QStringLiteral("origFontSize")).toDouble(), scale, ptPerUnit));
                    else if (same(now, o.fontSize * scale)) style.remove(QStringLiteral("fontSize"));
                    else style.insert(QStringLiteral("fontSize"), unscale(now, 0, scale, ptPerUnit));
                }
                if (style.contains(QStringLiteral("origWeight")) && style.value(QStringLiteral("fontWeight")).toInt() == namedWeight(office))
                    style.insert(QStringLiteral("fontWeight"), style.value(QStringLiteral("origWeight")));
                style.insert(QStringLiteral("fontFamily"), office);
            } else if (const auto back = officeFor(family); !back.isEmpty()) {
                style.insert(QStringLiteral("fontFamily"), back);
            }
            for (const auto *key : {"origFont", "origFontSize", "fontScale", "origWeight"}) style.remove(QLatin1String(key));
        }
        o.origFont.clear(); o.origFontSize = 0; o.fontScale = 1; o.origWeight = 0;
    });
    auto &theme = document.theme;
    for (auto it = theme.fonts.begin(); it != theme.fonts.end(); ++it) {
        const auto office = theme.fontOrigins.value(it.key());
        if (!office.isEmpty() && stillSwapped(office, it.value(), &document.theme.fontFallbacks)) it.value() = office;
        else if (const auto back = officeFor(it.value()); !back.isEmpty()) it.value() = back;
    }
    theme.fontOrigins.clear();
    theme.fontFallbacks.clear();
}

QVariantList FontSwap::swapped(const Document &document) {
    Tally tally;
    Document copy = document;
    eachObject(copy, [&](SceneObject &o) {
        if (!o.origFont.isEmpty()) ++tally[o.origFont][o.fontFamily];
        for (const auto &run : o.runs) if (!run.origFont.isEmpty()) ++tally[run.origFont][run.fontFamily];
        for (const auto &cell : o.table.cells)
            if (cell.style.contains(QStringLiteral("origFont")))
                ++tally[cell.style.value(QStringLiteral("origFont")).toString()][cell.style.value(QStringLiteral("fontFamily")).toString()];
    });
    for (auto it = document.theme.fontOrigins.cbegin(); it != document.theme.fontOrigins.cend(); ++it)
        ++tally[it.value()][document.theme.fonts.value(it.key())];
    return tallyList(tally);
}



FontSwap::VerticalFix FontSwap::verticalFix(const SceneObject &o, qreal size) {
    VerticalFix fix;
    if (o.origFont.isEmpty() || o.fontFamily.isEmpty() || size <= 0 || !stillSwapped(o.origFont, o.fontFamily)) return fix;
    const auto entry = table().constFind(o.origFont.toLower());
    if (entry == table().cend() || entry->ascent <= 0) return fix;
    const auto *option = optionFor(o.origFont, o.fontFamily);
    // Metric-compatible matches get their lines put where PowerPoint's land.
    // A scaled table match is left alone (measured: moving it made it sit
    // worse, Segoe UI). A fallback gets both: sized down by x-height it would otherwise
    // sit high by the ascent it lost (measured: 10 px on a 44 pt title).
    if (!option || (!option->metric && !option->fallback)) return fix;
    // Worked out from the box's font: a stretch in another family or a larger
    // size sets its own lines, so such a box is left as it was.
    for (const auto &run : o.runs)
        if (!run.fontFamily.isEmpty() || run.fontSize > o.fontSize + 1e-6) return fix;
    struct Em { qreal ascent, descent, leading; };
    static thread_local QHash<QString, Em> cache;
    const QString key = o.fontFamily + QLatin1Char('|') + QString::number(o.fontWeight) + QLatin1Char('|') + QString::number(o.italic);
    auto it = cache.constFind(key);
    if (it == cache.cend()) {
        QFont font(o.fontFamily);
        font.setPixelSize(1000);
        font.setWeight(QFont::Weight(o.fontWeight > 0 ? o.fontWeight : 400));
        font.setItalic(o.italic);
        const QFontMetricsF m(font);
        it = cache.insert(key, {m.ascent() / 1000, m.descent() / 1000, qMax<qreal>(0, m.leading()) / 1000});
    }
    const qreal free = it->ascent + it->descent + it->leading;
    if (free <= 0) return fix;
    const qreal officeSize = size / (o.fontScale > 0 ? o.fontScale : 1);
    fix.line = (entry->ascent + entry->descent + entry->gap) * officeSize / (free * size);
    fix.shift = it->ascent * size - entry->ascent * officeSize;
    if (std::abs(fix.line - 1) < 0.005) fix.line = 1;
    if (std::abs(fix.shift) < 0.25) fix.shift = 0;
    return fix;
}

namespace {
const QString kSentence = QStringLiteral("Sphinx of black quartz, judge my vow. The quick brown fox jumps over the lazy dog 0123456789");
// Ordinary English prose, letters at their usual frequencies; the map's "aw"
// is the Office font's kerned advance over exactly this text.
const QString kCorpus = QStringLiteral(
    "When the quarterly review began, the team looked first at what had changed since the last meeting. S"
    "ales in the northern region grew by about twelve percent, while costs in shipping and support fell s"
    "lightly. Several customers asked for clearer pricing, and the product group agreed to publish a simp"
    "le guide before the end of the month. We also hired four people across two offices, which should hel"
    "p with the backlog of requests that built up during the summer. Next, we will focus on three goals: "
    "keep response times under a day, finish the new reporting tools, and plan the training program for s"
    "pring. If you have questions about any of these points, please write to your manager or bring them t"
    "o the next session.");
}

namespace {
const Entry *entry0(const QString &office) {
    const auto it = table().constFind(office.toLower());
    return it == table().cend() ? nullptr : &*it;
}
}

int FontSwap::styleOf(int weight, bool italic) {
    return weight >= 600 ? 1 : italic ? 2 : 0;
}

const FontSwap::Option *FontSwap::fallback(const QString &officeFamily, int style) {
    // Measured once per Office family; drawing threads ask too.
    static QMutex lock;
    static QHash<QString, std::shared_ptr<Option>> measured;
    style = qBound(0, style, 2);
    // No width for this style in the map: the regular one, measured regular.
    if (style && entry0(officeFamily) && !(entry0(officeFamily)->advance[style] > 0)) style = 0;
    const QString key = officeFamily.toLower() + QLatin1Char('|') + QString::number(style);
    QMutexLocker locker(&lock);
    if (const auto it = measured.constFind(key); it != measured.cend()) return it->get();
    std::shared_ptr<Option> result;
    const auto entry = table().constFind(officeFamily.toLower());
    if (entry != table().cend() && (entry->sentence > 0 || entry->advance[0] > 0)) {
        // fontconfig's choice for the generic class of the Office family.
        const QChar kind = entry->kind;
        QFont probe(kind == QLatin1Char('m') ? QStringLiteral("monospace")
                    : kind == QLatin1Char('r') ? QStringLiteral("serif")
                    : kind == QLatin1Char('h') ? QStringLiteral("cursive")
                                               : QStringLiteral("sans-serif"));
        probe.setStyleHint(kind == QLatin1Char('m') ? QFont::Monospace : kind == QLatin1Char('r') ? QFont::Serif
                           : kind == QLatin1Char('h') ? QFont::Cursive : QFont::SansSerif);
        const QString family = QFontInfo(probe).family();
        if (!family.isEmpty() && installed(family)) do {
            QFont font(family);
            font.setPixelSize(1000);
            const int regular = entry->weight ? entry->weight : 400;
            font.setWeight(QFont::Weight(style == 1 ? qMax(700, regular) : regular));
            font.setItalic(style == 2);
            font.setKerning(true);
            const QFontMetricsF m(font);
            // Kerned advance per character over the corpus where the map has
            // the Office font's (per style); the old sentence otherwise.
            const bool corpus = entry->advance[style] > 0;
            const qreal width = corpus ? m.horizontalAdvance(kCorpus) / 1000 / kCorpus.size() : m.horizontalAdvance(kSentence) / 1000;
            const qreal office = corpus ? entry->advance[style] : entry->sentence;
            if (width <= 0 || office <= 0) break;
            // Sized by width: the fallback sets the same sentence
            // as wide as the Office font, so lines break where PowerPoint's do.
            const qreal sameSize = width / office;
            qreal size = office / width;
            size = qBound<qreal>(0.6, size, 1.6);
            result = std::make_shared<Option>();
            result->family = family;
            result->fallback = true;
            // Within 0.3%: drawn at the Office size.
            const bool sameWidth = std::abs(sameSize - 1) <= 0.003;
            result->size = sameWidth ? 1.0 : std::round(size * 1000) / 1000;
            result->width = sameSize * result->size;
            const qreal free = (m.ascent() + m.descent() + qMax<qreal>(0, m.leading())) / 1000;
            if (entry->ascent > 0 && free > 0)
                result->line = (entry->ascent + entry->descent + entry->gap) / (result->size * free);
        } while (false);
    }
    measured.insert(key, result);
    return result.get();
}
