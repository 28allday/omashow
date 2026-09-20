#include "core/review.h"
#include "anim/presentation.h"
#include "core/design.h"
#include "core/edit.h"
#include "render/textlayout.h"
#include <QDateTime>
#include <QRegularExpression>
#include <QSet>
#include <QFontDatabase>

namespace {

constexpr int kAuthorLimit = 120, kTextLimit = 4000;

int indexOfComment(const Document &d, const QString &id) {
    for (int i = 0; i < d.comments.size(); ++i)
        if (d.comments.at(i).id == id) return i;
    return -1;
}

const Slide *slideById(const Document &d, const QString &id) {
    for (const auto &slide : d.slides)
        if (slide.id == id) return &slide;
    return nullptr;
}

int slideIndexById(const Document &d, const QString &id) {
    for (int i = 0; i < d.slides.size(); ++i)
        if (d.slides.at(i).id == id) return i;
    return -1;
}

QVariantMap describe(const Document &d, const Comment &comment) {
    return QVariantMap{{"id", comment.id}, {"slideId", comment.slideId},
                       {"slide", slideIndexById(d, comment.slideId)},
                       {"objectId", comment.objectId}, {"author", comment.author},
                       {"created", comment.created}, {"text", comment.text},
                       {"resolved", comment.resolved}};
}

// What this object would be called in a list of findings.
QString nameFor(const SceneObject &o) {
    switch (o.type) {
    case ObjectType::Text: return o.text.trimmed().isEmpty()
            ? QStringLiteral("Empty text") : "“" + o.text.section('\n', 0, 0).left(40) + "”";
    case ObjectType::Image: return o.imageFormat == "svg" ? QStringLiteral("Drawing")
                                                          : QStringLiteral("Picture");
    case ObjectType::Media: return o.mediaName.isEmpty()
            ? (o.mediaVideo ? QStringLiteral("Film") : QStringLiteral("Sound")) : o.mediaName;
    case ObjectType::Table: return QStringLiteral("Table");
    case ObjectType::Chart: return QStringLiteral("Chart");
    case ObjectType::Rect: break;
    }
    return QStringLiteral("Shape");
}

bool needsDescription(const SceneObject &o) {
    return o.type == ObjectType::Image || o.type == ObjectType::Media ||
           o.type == ObjectType::Chart || o.type == ObjectType::Table;
}

int wordsIn(const QString &text) {
    return text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).size();
}

} // namespace

QVariantList Review::threads(const Document &d, const QString &slideId, bool includeResolved) {
    QVariantList rows;
    for (const auto &comment : d.comments) {
        if (!comment.parentId.isEmpty()) continue;
        if (!slideId.isEmpty() && comment.slideId != slideId) continue;
        QVariantList replies;
        bool anyOpen = !comment.resolved;
        for (const auto &reply : d.comments) {
            if (reply.parentId != comment.id) continue;
            if (!reply.resolved) anyOpen = true;
            replies.append(describe(d, reply));
        }
        if (!includeResolved && !anyOpen) continue;
        auto row = describe(d, comment);
        row["replies"] = replies;
        row["open"] = anyOpen;
        rows.append(row);
    }
    return rows;
}

QString Review::add(Document &d, const QString &slideId, const QString &objectId,
                    const QString &author, const QString &text, const QString &parentId,
                    const QString &created) {
    Comment comment;
    comment.id = Edit::newId("comment");
    comment.slideId = slideId;
    comment.objectId = objectId;
    comment.parentId = parentId;
    comment.author = author.trimmed();
    comment.text = text.trimmed();
    comment.created = created.isEmpty()
                          ? QDateTime::currentDateTime().toString(Qt::ISODate) : created;
    if (!parentId.isEmpty()) {
        const int parent = indexOfComment(d, parentId);
        if (parent < 0 || !d.comments.at(parent).parentId.isEmpty()) return {};
        comment.slideId = d.comments.at(parent).slideId;
        comment.objectId = d.comments.at(parent).objectId;
    }
    d.comments.append(comment);
    if (!validate(d).isEmpty()) {
        d.comments.removeLast();
        return {};
    }
    return comment.id;
}

bool Review::setText(Document &d, const QString &id, const QString &text) {
    const int at = indexOfComment(d, id);
    if (at < 0 || text.trimmed().isEmpty() || text.size() > kTextLimit) return false;
    if (d.comments.at(at).text == text.trimmed()) return false;
    d.comments[at].text = text.trimmed();
    return true;
}

bool Review::setResolved(Document &d, const QString &id, bool resolved) {
    const int at = indexOfComment(d, id);
    if (at < 0 || d.comments.at(at).resolved == resolved) return false;
    d.comments[at].resolved = resolved;
    // Resolving a thread resolves what is under it; reopening reopens it.
    if (d.comments.at(at).parentId.isEmpty())
        for (auto &reply : d.comments)
            if (reply.parentId == id) reply.resolved = resolved;
    return true;
}

bool Review::remove(Document &d, const QString &id) {
    const int at = indexOfComment(d, id);
    if (at < 0) return false;
    const bool thread = d.comments.at(at).parentId.isEmpty();
    for (int i = d.comments.size() - 1; i >= 0; --i)
        if (d.comments.at(i).id == id || (thread && d.comments.at(i).parentId == id))
            d.comments.removeAt(i);
    return true;
}

bool Review::prune(Document &d) {
    bool changed = false;
    for (int i = d.comments.size() - 1; i >= 0; --i) {
        const auto &comment = d.comments.at(i);
        const auto *slide = slideById(d, comment.slideId);
        const bool orphan = !slide ||
                            (!comment.objectId.isEmpty() && !slide->find(comment.objectId)) ||
                            (!comment.parentId.isEmpty() && indexOfComment(d, comment.parentId) < 0);
        if (!orphan) continue;
        // A thread's replies go with it.
        const auto id = comment.id;
        for (int j = d.comments.size() - 1; j >= 0; --j)
            if (d.comments.at(j).id == id || d.comments.at(j).parentId == id)
                d.comments.removeAt(j);
        changed = true;
        i = qMin(i, int(d.comments.size()));
    }
    for (auto &slide : d.slides)
        for (int i = slide.readingOrder.size() - 1; i >= 0; --i)
            if (!slide.find(slide.readingOrder.at(i))) {
                slide.readingOrder.removeAt(i);
                changed = true;
            }
    // A custom show is a list of slides; a deleted slide simply leaves it.
    for (auto &show : d.shows)
        for (int i = show.slideIds.size() - 1; i >= 0; --i)
            if (!slideById(d, show.slideIds.at(i))) {
                show.slideIds.removeAt(i);
                changed = true;
            }
    for (int i = d.activeShow.size() - 1; i >= 0; --i)
        if (!slideById(d, d.activeShow.at(i))) {
            d.activeShow.removeAt(i);
            changed = true;
        }
    return changed;
}

QString Review::validate(const Document &d) {
    QSet<QString> ids;
    for (const auto &comment : d.comments) {
        if (comment.id.isEmpty() || ids.contains(comment.id))
            return QStringLiteral("A review comment has a missing or duplicate identity.");
        ids.insert(comment.id);
        if (comment.text.isEmpty() || comment.text.size() > kTextLimit ||
            comment.author.size() > kAuthorLimit)
            return QStringLiteral("A review comment is empty or too long.");
        if (!comment.created.isEmpty() &&
            !QDateTime::fromString(comment.created, Qt::ISODate).isValid())
            return QStringLiteral("A review comment has an invalid date.");
        const auto *slide = slideById(d, comment.slideId);
        if (!slide) return QStringLiteral("A review comment points at a missing slide.");
        if (!comment.objectId.isEmpty() && !slide->find(comment.objectId))
            return QStringLiteral("A review comment points at a missing object.");
        if (comment.parentId.isEmpty()) continue;
        const int parent = indexOfComment(d, comment.parentId);
        if (parent < 0 || !d.comments.at(parent).parentId.isEmpty())
            return QStringLiteral("A reply points at something that is not a comment.");
        if (d.comments.at(parent).slideId != comment.slideId ||
            d.comments.at(parent).objectId != comment.objectId)
            return QStringLiteral("A reply belongs to a different slide than its comment.");
    }
    return {};
}

QVariantMap Review::statistics(const Document &d) {
    int words = 0, characters = 0, pictures = 0, films = 0, sounds = 0, tables = 0,
        charts = 0, shapes = 0, textBoxes = 0, notes = 0, builds = 0, skipped = 0,
        noteWords = 0, missingAlt = 0;
    qint64 assetBytes = 0;
    QSet<QString> fonts, countedAssets;
    for (int i = 0; i < d.slides.size(); ++i) {
        const auto &authored = d.slides.at(i);
        if (authored.skipped) ++skipped;
        builds += authored.timeline.steps.size();
        if (!authored.notes.trimmed().isEmpty()) {
            ++notes;
            noteWords += wordsIn(authored.notes);
        }
        for (const auto &o : Design::resolve(d, i).objects) {
            if (o.hidden) continue;
            if (needsDescription(o) && o.altText.trimmed().isEmpty()) ++missingAlt;
            switch (o.type) {
            case ObjectType::Text:
                ++textBoxes;
                words += wordsIn(o.text);
                characters += o.text.size();
                fonts.insert(o.fontFamily);
                break;
            case ObjectType::Image: ++pictures; break;
            case ObjectType::Media: o.mediaVideo ? ++films : ++sounds; break;
            case ObjectType::Table:
                ++tables;
                fonts.insert(o.fontFamily);
                for (const auto &cell : o.table.cells) words += wordsIn(cell.text);
                break;
            case ObjectType::Chart: ++charts; fonts.insert(o.fontFamily); break;
            case ObjectType::Rect: ++shapes; break;
            }
            if (!o.imageId.isEmpty() && !countedAssets.contains(o.imageId)) {
                countedAssets.insert(o.imageId);
                assetBytes += o.imageData.size();
            }
            if (o.type == ObjectType::Media && o.mediaPath.isEmpty() &&
                !countedAssets.contains(o.mediaId)) {
                countedAssets.insert(o.mediaId);
                assetBytes += o.mediaData.size();
            }
        }
    }
    QStringList families = fonts.values();
    families.sort();
    QStringList missingFonts;
    for (const auto &family : families)
        if (!QFontDatabase::hasFamily(family)) missingFonts.append(family);
    int openComments = 0;
    for (const auto &comment : d.comments)
        if (!comment.resolved) ++openComments;
    return QVariantMap{{"slides", d.slides.size()}, {"skipped", skipped},
                       {"sections", d.sections.size()}, {"words", words},
                       {"characters", characters}, {"textBoxes", textBoxes},
                       {"pictures", pictures}, {"films", films}, {"sounds", sounds},
                       {"tables", tables}, {"charts", charts}, {"shapes", shapes},
                       {"builds", builds}, {"notes", notes}, {"noteWords", noteWords},
                       {"fonts", families}, {"missingFonts", missingFonts},
                       {"assetBytes", assetBytes}, {"comments", d.comments.size()},
                       {"openComments", openComments}, {"missingAlt", missingAlt},
                       {"masters", d.masters.size()}, {"layouts", d.layouts.size()},
                       // The deck's own clock: builds, holds and transitions.
                       {"duration", Presentation::duration(d, true)},
                       {"showDuration", Presentation::duration(d, false)}};
}

QStringList Review::readingOrder(const Document &d, int index) {
    if (index < 0 || index >= d.slides.size()) return {};
    const auto &slide = d.slides.at(index);
    QStringList order;
    for (const auto &id : slide.readingOrder)
        if (slide.find(id)) order.append(id);
    for (const auto &o : slide.objects)
        if (!order.contains(o.id)) order.append(o.id);
    return order;
}

bool Review::moveReading(Document &d, int index, const QString &objectId, int delta) {
    if (index < 0 || index >= d.slides.size() || delta == 0) return false;
    auto order = readingOrder(d, index);
    const int at = order.indexOf(objectId);
    if (at < 0) return false;
    const int to = qBound(0, at + delta, int(order.size()) - 1);
    if (to == at) return false;
    order.move(at, to);
    d.slides[index].readingOrder = order;
    return true;
}

bool Review::resetReading(Document &d, int index) {
    if (index < 0 || index >= d.slides.size() || d.slides.at(index).readingOrder.isEmpty())
        return false;
    d.slides[index].readingOrder.clear();
    return true;
}

QVariantList Review::issues(const Document &d) {
    QVariantList rows;
    const auto add = [&](const QString &key, int slide, const QString &slideId,
                         const QString &objectId, const QString &severity,
                         const QString &check, const QString &title, const QString &detail) {
        rows.append(QVariantMap{{"key", key}, {"slide", slide}, {"slideId", slideId},
                                {"objectId", objectId}, {"severity", severity},
                                {"check", check}, {"title", title}, {"detail", detail},
                                {"dismissed", d.dismissedIssues.contains(key)}});
    };
    const qreal scale = d.size.height() / 1080.0;
    for (int i = 0; i < d.slides.size(); ++i) {
        const Slide shown = Design::resolve(d, i);
        const auto &authored = d.slides.at(i);
        bool hasTitle = false;
        int readable = 0;
        QVector<const SceneObject *> text;
        for (const auto &o : shown.objects) {
            if (o.hidden || o.opacity <= 0.01) continue;
            if (o.type == ObjectType::Text && !o.text.trimmed().isEmpty()) {
                if (!o.id.startsWith(QStringLiteral("@field/"))) hasTitle = true;
                text.append(&o);
                ++readable;
            }
            const bool authoredHere = authored.find(o.id) != nullptr;
            if (needsDescription(o) && o.altText.trimmed().isEmpty() && authoredHere)
                add("alt/" + o.id, i, authored.id, o.id, "must", "description",
                    nameFor(o) + " has no description",
                    "Add alternative text in the Review workspace so the content is not lost "
                    "to anyone who cannot see it.");
            if (o.type == ObjectType::Text && !o.text.trimmed().isEmpty()) {
                const qreal ratio = Design::contrastRatio(o.textColor, shown.background);
                const bool large = o.fontSize >= 24 * scale * (o.fontWeight >= 600 ? 0.75 : 1);
                if (ratio > 0 && ratio < (large ? 3.0 : 4.5))
                    add("contrast/" + o.id, i, authored.id, o.id, "must", "contrast",
                        QString("%1 is %2:1 against the slide").arg(nameFor(o)).arg(ratio, 0, 'f', 2),
                        QString("Aim for %1:1. Change the text colour, the slide background, "
                                "or make the text larger.").arg(large ? "3" : "4.5"));
                if (o.fontSize < 14 * scale)
                    add("size/" + o.id, i, authored.id, o.id, "should", "size",
                        nameFor(o) + " is very small",
                        "Type below about 14 points at this slide size is hard to read from "
                        "the back of a room.");
                if (wordsIn(o.text) > 60)
                    add("density/" + o.id, i, authored.id, o.id, "info", "density",
                        nameFor(o) + " carries a lot of words",
                        QString("%1 words in one box. Consider splitting it across slides.")
                            .arg(wordsIn(o.text)));
                if (TextLayout::measure(o).overflow && o.textFit == 0)
                    add("overflow/" + o.id, i, authored.id, o.id, "should", "overflow",
                        nameFor(o) + " does not fit its box",
                        "The text is clipped. Shrink to fit, resize the box, or cut words.");
            }
            if (o.linkKind == 1 && authoredHere) {
                const auto label = o.type == ObjectType::Text ? o.text.trimmed() : QString();
                if (label.isEmpty() || label.startsWith("http", Qt::CaseInsensitive))
                    add("link/" + o.id, i, authored.id, o.id, "should", "link",
                        nameFor(o) + " links out without saying where",
                        "Give the link words that describe where it goes, rather than the "
                        "address itself.");
            }
        }
        if (!hasTitle)
            add("title/" + authored.id, i, authored.id, QString(),
                authored.skipped ? "info" : "should", "title",
                QString("Slide %1 has no title").arg(i + 1),
                "A slide with no words of its own is hard to find in the navigator, the "
                "outline or a review.");
        // Reading follows the stacking order unless the slide says otherwise, and
        // that is only worth raising when the two actually disagree.
        if (authored.readingOrder.isEmpty() && readable > 1) {
            bool descending = true;
            for (int t = 1; t < text.size(); ++t)
                if (text.at(t)->rect.top() < text.at(t - 1)->rect.top() - 1) descending = false;
            if (!descending)
                add("order/" + authored.id, i, authored.id, QString(), "should", "order",
                    QString("Slide %1 is read in a different order than it is laid out").arg(i + 1),
                    "Objects are read in the order they were added. Set a reading order in "
                    "the Review workspace to match what the slide looks like.");
        }
    }
    return rows;
}

bool Review::dismiss(Document &d, const QString &key, bool dismissed) {
    if (key.isEmpty()) return false;
    if (dismissed == d.dismissedIssues.contains(key)) return false;
    if (dismissed) d.dismissedIssues.append(key);
    else d.dismissedIssues.removeAll(key);
    return true;
}

QByteArray Review::report(const Document &d, const QVariantList &issues, const QString &deckName) {
    const auto stats = statistics(d);
    QStringList lines;
    lines << "Review of " + (deckName.isEmpty() ? QStringLiteral("this deck") : deckName)
          << QDateTime::currentDateTime().toString(Qt::ISODate)
          << QString()
          << QString("%1 slides · %2 words · %3 pictures · %4 films · %5 sounds")
                 .arg(stats["slides"].toInt()).arg(stats["words"].toInt())
                 .arg(stats["pictures"].toInt()).arg(stats["films"].toInt())
                 .arg(stats["sounds"].toInt())
          << QString("Runs about %1 minutes with every build and transition")
                 .arg(stats["duration"].toDouble() / 60, 0, 'f', 1)
          << QString("%1 comments, %2 still open")
                 .arg(stats["comments"].toInt()).arg(stats["openComments"].toInt())
          << QString();
    int counted = 0;
    for (const auto &value : issues) {
        const auto row = value.toMap();
        ++counted;
        lines << QString("Slide %1 · %2 · %3")
                     .arg(row["slide"].toInt() + 1)
                     .arg(row["severity"].toString(), row["title"].toString())
              << "    " + row["detail"].toString()
              + (row["dismissed"].toBool() ? " (marked as not a problem)" : QString());
    }
    if (counted == 0) lines << "No findings.";
    for (const auto &comment : d.comments) {
        if (comment.resolved) continue;
        lines << QString();
        lines << QString("Slide %1 · comment by %2 · %3")
                     .arg(slideIndexById(d, comment.slideId) + 1)
                     .arg(comment.author.isEmpty() ? QStringLiteral("unattributed") : comment.author,
                          comment.created)
              << "    " + comment.text;
    }
    return (lines.join('\n') + '\n').toUtf8();
}
