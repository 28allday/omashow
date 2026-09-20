#include "core/deckaudit.h"
#include "core/design.h"
#include <QHash>
#include <QSet>

namespace {

struct Assets { QHash<QString, qint64> bytes; QHash<QString, int> uses; };

void gather(const QVector<SceneObject> &objects, Assets &assets, bool count) {
    for (const auto &o : objects) {
        if (o.imageOriginal) gather({*o.imageOriginal}, assets, count);
        if (o.mediaOriginal) gather({*o.mediaOriginal}, assets, count);
        if (!o.imageId.isEmpty()) {
            assets.bytes[o.imageId] = o.imageData.size();
            if (count) ++assets.uses[o.imageId];
        }
        if (o.type == ObjectType::Media && o.mediaPath.isEmpty() && !o.mediaId.isEmpty()) {
            assets.bytes[o.mediaId] = o.mediaData.size();
            if (count) ++assets.uses[o.mediaId];
        }
    }
}

// Every asset in the deck, with how many places refer to it. Assets are written
// once per content hash, so only the last reference to one gives its bytes back.
Assets inventory(const Document &d) {
    Assets assets;
    for (const auto &slide : d.slides) gather(slide.objects, assets, true);
    for (const auto &master : d.masters) gather(master.objects, assets, true);
    for (const auto &layout : d.layouts) gather(layout.placeholders, assets, true);
    for (const auto &style : d.objectStyles) gather({style.appearance}, assets, true);
    return assets;
}

qint64 exclusiveBytes(const Assets &assets, const QVector<SceneObject> &objects) {
    Assets mine;
    gather(objects, mine, true);
    qint64 bytes = 0;
    for (auto it = mine.uses.cbegin(); it != mine.uses.cend(); ++it)
        if (assets.uses.value(it.key()) <= it.value()) bytes += assets.bytes.value(it.key());
    return bytes;
}

bool masterIsUsed(const Document &d, const QString &id) {
    for (const auto &layout : d.layouts)
        if (layout.masterId == id) return true;
    return false;
}
bool layoutIsUsed(const Document &d, const QString &id) {
    for (const auto &slide : d.slides)
        if (slide.layoutId == id) return true;
    return false;
}
bool sectionIsUsed(const Document &d, const QString &id) {
    for (const auto &slide : d.slides)
        if (slide.sectionId == id) return true;
    return false;
}

QString describeBytes(qint64 bytes) {
    if (bytes >= 1024 * 1024) return QString("%1 MB").arg(bytes / double(1024 * 1024), 0, 'f', 1);
    if (bytes >= 1024) return QString("%1 kB").arg(bytes / 1024);
    return QString("%1 bytes").arg(bytes);
}

} // namespace

QVariantMap DeckAudit::report(const Document &d) {
    const Assets assets = inventory(d);
    QVariantList rows;
    qint64 total = 0, deckAssets = 0;
    for (auto it = assets.bytes.cbegin(); it != assets.bytes.cend(); ++it) deckAssets += it.value();
    int masters = 0, layouts = 0, sections = 0, originals = 0;
    for (const auto &master : d.masters) {
        if (masterIsUsed(d, master.id)) continue;
        ++masters;
        const qint64 bytes = exclusiveBytes(assets, master.objects);
        total += bytes;
        rows.append(QVariantMap{{"id", "master:" + master.id}, {"kind", "master"},
                                {"name", master.name}, {"bytes", bytes},
                                {"detail", QString("no layout uses this master · %1 objects")
                                               .arg(master.objects.size())}});
    }
    for (const auto &layout : d.layouts) {
        if (layoutIsUsed(d, layout.id)) continue;
        ++layouts;
        const qint64 bytes = exclusiveBytes(assets, layout.placeholders);
        total += bytes;
        const auto *master = Design::master(d, layout.masterId);
        rows.append(QVariantMap{{"id", "layout:" + layout.id}, {"kind", "layout"},
                                {"name", (master ? master->name + " / " : QString()) + layout.name},
                                {"bytes", bytes},
                                {"detail", QString("no slide uses this layout · %1 placeholders")
                                               .arg(layout.placeholders.size())}});
    }
    for (const auto &section : d.sections) {
        if (sectionIsUsed(d, section.id)) continue;
        ++sections;
        rows.append(QVariantMap{{"id", "section:" + section.id}, {"kind", "section"},
                                {"name", section.name}, {"bytes", 0},
                                {"detail", "an empty section"}});
    }
    for (const auto &slide : d.slides)
        for (const auto &o : slide.objects) {
            const bool picture = bool(o.imageOriginal), film = bool(o.mediaOriginal);
            if (!picture && !film) continue;
            ++originals;
            const qint64 kept = picture ? o.imageOriginal->imageData.size()
                                        : o.mediaOriginal->mediaData.size();
            const qint64 bytes = assets.uses.value(picture ? o.imageOriginal->imageId
                                                           : o.mediaOriginal->mediaId) <= 1
                                     ? kept : 0;
            total += bytes;
            rows.append(QVariantMap{
                {"id", "original:" + slide.id + "/" + o.id}, {"kind", "original"},
                {"name", picture ? (o.imageId.isEmpty() ? QStringLiteral("Picture")
                                                        : QStringLiteral("Optimised picture"))
                                 : (o.mediaName.isEmpty() ? QStringLiteral("Optimised film") : o.mediaName)},
                {"bytes", bytes},
                {"detail", QString("the original kept before optimising · %1").arg(describeBytes(kept))}});
        }
    return QVariantMap{{"rows", rows}, {"bytes", total}, {"assetBytes", deckAssets},
                       {"masters", masters}, {"layouts", layouts},
                       {"sections", sections}, {"originals", originals},
                       {"deckMasters", d.masters.size()}, {"deckLayouts", d.layouts.size()},
                       {"summary", describeBytes(total)}};
}

bool DeckAudit::remove(Document &document, const QStringList &ids) {
    if (ids.isEmpty()) return false;
    Document next = document;
    // Originals first, then the containers — so selecting a layout together with
    // the master it was the last user of removes both in one step.
    const QStringList order = {"original", "section", "layout", "master"};
    QStringList pending = ids;
    pending.removeDuplicates();
    int removed = 0;
    for (const auto &kind : order)
        for (const auto &row : pending) {
            if (row.section(':', 0, 0) != kind) continue;
            const QString id = row.section(':', 1);
            if (kind == "master") {
                if (masterIsUsed(next, id)) return false;
                int at = -1;
                for (int i = 0; i < next.masters.size(); ++i)
                    if (next.masters.at(i).id == id) at = i;
                if (at < 0) return false;
                next.masters.removeAt(at);
            } else if (kind == "layout") {
                if (layoutIsUsed(next, id)) return false;
                int at = -1;
                for (int i = 0; i < next.layouts.size(); ++i)
                    if (next.layouts.at(i).id == id) at = i;
                if (at < 0) return false;
                next.layouts.removeAt(at);
            } else if (kind == "section") {
                if (sectionIsUsed(next, id)) return false;
                int at = -1;
                for (int i = 0; i < next.sections.size(); ++i)
                    if (next.sections.at(i).id == id) at = i;
                if (at < 0) return false;
                next.sections.removeAt(at);
            } else if (kind == "original") {
                const QString slideId = id.section('/', 0, 0), objectId = id.section('/', 1);
                SceneObject *found = nullptr;
                for (auto &slide : next.slides)
                    if (slide.id == slideId) found = slide.find(objectId);
                if (!found || (!found->imageOriginal && !found->mediaOriginal)) return false;
                found->imageOriginal.reset();
                found->mediaOriginal.reset();
            } else {
                return false;
            }
            ++removed;
        }
    if (removed != pending.size()) return false;
    document = next;
    return true;
}
