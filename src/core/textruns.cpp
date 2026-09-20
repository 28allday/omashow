#include "core/textruns.h"
#include <QSet>
#include <algorithm>
#include <cmath>

namespace {

bool sameLook(const TextRun &a, const TextRun &b) {
    TextRun left = a, right = b;
    left.start = right.start = 0;
    left.length = right.length = 0;
    return left == right;
}

bool blank(const TextRun &run) {
    return run.weight == 0 && run.italic == 0 && run.underline == 0 && run.strike == 0 &&
           run.baseline == 0 && run.fontSize <= 0 && run.fontFamily.isEmpty() &&
           !run.color.isValid();
}

} // namespace

QVector<TextRun> TextRuns::tidy(QVector<TextRun> runs, int length) {
    QVector<TextRun> kept;
    for (auto &run : runs) {
        run.start = qMax(0, run.start);
        run.length = qMin(run.length, length - run.start);
        if (run.length <= 0 || run.start >= length || blank(run)) continue;
        if (run.fontSize < 0 || !std::isfinite(run.fontSize)) run.fontSize = 0;
        run.weight = run.weight == 0 ? 0 : qBound(100, run.weight, 900);
        run.italic = qBound(0, run.italic, 2);
        run.underline = qBound(0, run.underline, 2);
        run.strike = qBound(0, run.strike, 2);
        run.baseline = qBound(0, run.baseline, 2);
        kept.append(run);
    }
    std::stable_sort(kept.begin(), kept.end(),
                     [](const TextRun &a, const TextRun &b) { return a.start < b.start; });
    QVector<TextRun> merged;
    for (const auto &run : kept) {
        if (!merged.isEmpty()) {
            auto &last = merged.last();
            if (run.start < last.start + last.length) continue;   // overlaps: the first wins
            if (run.start == last.start + last.length && sameLook(last, run)) {
                last.length += run.length;
                continue;
            }
        }
        merged.append(run);
    }
    return merged;
}

const TextRun *TextRuns::at(const QVector<TextRun> &runs, int position) {
    for (const auto &run : runs)
        if (position >= run.start && position < run.start + run.length) return &run;
    return nullptr;
}

bool TextRuns::apply(SceneObject &object, int start, int end, const QString &key,
                     const QVariant &value) {
    const int length = object.text.size();
    start = qBound(0, start, length);
    end = qBound(0, end, length);
    if (start >= end) return false;
    static const QStringList keys{"weight", "italic", "underline", "strike",
                                  "baseline", "fontSize", "fontFamily", "color"};
    if (!keys.contains(key)) return false;

    // Work per character, then put the stretches back together.
    QVector<TextRun> spread;
    for (int i = 0; i < length; ++i) {
        TextRun run;
        if (const auto *existing = at(object.runs, i)) run = *existing;
        run.start = i;
        run.length = 1;
        if (i >= start && i < end) {
            if (key == "weight") {
                const int weight = value.toInt();
                if (weight != 0 && (weight < 100 || weight > 900)) return false;
                run.weight = weight;
            } else if (key == "fontSize") {
                bool ok = false;
                const qreal size = value.toDouble(&ok);
                if (!ok || !std::isfinite(size) || size < 0 || size > 2000) return false;
                run.fontSize = size;
            } else if (key == "fontFamily") {
                if (value.toString().size() > 120) return false;
                run.fontFamily = value.toString();
            } else if (key == "color") {
                const QColor color(value.toString());
                if (!value.toString().isEmpty() && !color.isValid()) return false;
                run.color = value.toString().isEmpty() ? QColor() : color;
            } else {
                const int state = value.toInt();
                if (state < 0 || state > 2) return false;
                if (key == "italic") run.italic = state;
                if (key == "underline") run.underline = state;
                if (key == "strike") run.strike = state;
                if (key == "baseline") run.baseline = state;
            }
        }
        if (!blank(run)) spread.append(run);
    }
    object.runs = tidy(spread, length);
    return true;
}

QVector<TextRun> TextRuns::afterEdit(const QVector<TextRun> &runs, const QString &before,
                                     const QString &after) {
    if (before == after) return runs;
    // The unchanged head and tail keep their looks; what was typed in between
    // takes the look of whatever was there before it.
    int head = 0;
    const int most = qMin(before.size(), after.size());
    while (head < most && before.at(head) == after.at(head)) ++head;
    int tail = 0;
    while (tail < most - head && before.at(before.size() - 1 - tail) == after.at(after.size() - 1 - tail))
        ++tail;
    const int removed = before.size() - head - tail;
    const int inserted = after.size() - head - tail;
    QVector<TextRun> moved;
    for (const auto &run : runs) {
        TextRun copy = run;
        const int end = run.start + run.length;
        if (end <= head) {
            moved.append(copy);
            continue;
        }
        if (run.start >= head + removed) {
            copy.start += inserted - removed;
            moved.append(copy);
            continue;
        }
        // Straddles the edit: keep the part before it, and the part after.
        if (run.start < head) {
            TextRun kept = run;
            kept.length = head - run.start;
            moved.append(kept);
        }
        if (end > head + removed) {
            TextRun kept = run;
            kept.start = head + inserted;
            kept.length = end - (head + removed);
            moved.append(kept);
        }
    }
    return tidy(moved, after.size());
}

QVariantList TextRuns::encode(const QVector<TextRun> &runs) {
    QVariantList rows;
    for (const auto &run : runs)
        rows.append(QVariantMap{{"start", run.start}, {"length", run.length},
                                {"weight", run.weight}, {"italic", run.italic},
                                {"underline", run.underline}, {"strike", run.strike},
                                {"baseline", run.baseline}, {"fontSize", run.fontSize},
                                {"fontFamily", run.fontFamily},
                                {"color", run.color.isValid() ? run.color.name(QColor::HexArgb)
                                                              : QString()}});
    return rows;
}

bool TextRuns::decode(const QVariant &value, QVector<TextRun> &runs) {
    if (value.metaType().id() != QMetaType::QVariantList) return false;
    QVector<TextRun> parsed;
    for (const auto &row : value.toList()) {
        if (row.metaType().id() != QMetaType::QVariantMap) return false;
        const auto map = row.toMap();
        static const QSet<QString> known{"start", "length", "weight", "italic", "underline",
                                         "strike", "baseline", "fontSize", "fontFamily", "color"};
        for (auto it = map.cbegin(); it != map.cend(); ++it)
            if (!known.contains(it.key())) return false;
        TextRun run;
        bool ok = false;
        run.start = map.value("start").toInt(&ok);
        if (!ok || run.start < 0) return false;
        run.length = map.value("length").toInt(&ok);
        if (!ok || run.length <= 0) return false;
        run.weight = map.value("weight").toInt();
        if (run.weight != 0 && (run.weight < 100 || run.weight > 900)) return false;
        for (const auto &key : {"italic", "underline", "strike", "baseline"}) {
            const int state = map.value(key).toInt();
            if (state < 0 || state > 2) return false;
            if (QLatin1String(key) == QLatin1String("italic")) run.italic = state;
            if (QLatin1String(key) == QLatin1String("underline")) run.underline = state;
            if (QLatin1String(key) == QLatin1String("strike")) run.strike = state;
            if (QLatin1String(key) == QLatin1String("baseline")) run.baseline = state;
        }
        run.fontSize = map.value("fontSize").toDouble();
        if (!std::isfinite(run.fontSize) || run.fontSize < 0 || run.fontSize > 2000) return false;
        run.fontFamily = map.value("fontFamily").toString();
        if (run.fontFamily.size() > 120) return false;
        const auto colour = map.value("color").toString();
        if (!colour.isEmpty()) {
            run.color = QColor(colour);
            if (!run.color.isValid()) return false;
        }
        parsed.append(run);
    }
    runs = parsed;
    return true;
}
