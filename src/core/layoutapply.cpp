#include "core/layoutapply.h"
#include "core/design.h"
#include "render/textlayout.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QTransform>

QString LayoutApply::key(const QString &layoutId, const QString &role) {
    return QString::fromUtf8(QJsonDocument(QJsonArray{layoutId, role}).toJson(QJsonDocument::Compact));
}

LayoutApply::Result LayoutApply::preview(const Document &document, const QString &layoutId,
                                        const QStringList &slideIds, const QVariantMap &mapping,
                                        int geometry) {
    Result result;
    result.document = document;
    const auto *target = Design::layout(document, layoutId);
    if (!target || geometry < 0 || geometry > 2) {
        result.error = "Choose an existing layout and position option.";
        return result;
    }
    const QSet<QString> requested(slideIds.cbegin(), slideIds.cend());
    QSet<QString> found, seen;
    QVector<int> indices;
    QMap<int, QVariantMap> roles;
    // Collect all mapping rows even when one is invalid so the user can correct it.
    for (int index = 0; index < document.slides.size(); ++index) {
        const auto &source = document.slides[index];
        if (!requested.contains(source.id)) continue;
        found.insert(source.id);
        indices.append(index);
        const auto *sourceLayout = Design::layout(document, source.layoutId);
        for (const auto &object : source.objects) {
            if (object.placeholderId.isEmpty()) continue;
            const auto id = key(source.layoutId, object.placeholderId);
            QString automatic;
            QVariantList choices{QVariantMap{{"id", ""}, {"name", "Keep independent"}}};
            for (const auto &slot : target->placeholders) if (slot.type == object.type) {
                choices.append(QVariantMap{{"id", slot.id}, {"name", slot.id + " · " + slot.text.left(45)}});
                if (slot.id == object.placeholderId) automatic = slot.id;
            }
            const auto selected = mapping.value(id, automatic).toString();
            roles[index][object.placeholderId] = selected;
            if (!seen.contains(id)) {
                seen.insert(id);
                result.mappings.append(QVariantMap{
                    {"key", id}, {"name", (sourceLayout ? sourceLayout->name : QString("Freeform")) + " / " + object.placeholderId},
                    {"target", selected}, {"choices", choices}});
            }
        }
    }
    if (requested.isEmpty() || requested != found) {
        result.error = "Select existing slides to preview.";
        return result;
    }
    for (int index : indices) {
        if (!Design::applyLayout(result.document, index, layoutId, roles.value(index), geometry)) {
            result.error = "Each placeholder needs a different target of the same type. Change the mapping or keep it independent.";
            result.document = document;
            result.slides.clear();
            return result;
        }
        const auto before = Design::resolve(document, index);
        const auto after = Design::resolve(result.document, index);
        int moved = 0, detached = 0, created = 0, overflow = 0, outside = 0;
        for (const auto &object : result.document.slides[index].objects) {
            const auto *old = before.find(object.id), *now = after.find(object.id);
            if (!now) continue;
            if (!old) ++created;
            else {
                if (old->rect != now->rect || old->rotation != now->rotation) ++moved;
                if (!old->placeholderId.isEmpty() && object.placeholderId.isEmpty()) ++detached;
            }
            if (now->hidden) continue;
            if (now->type == ObjectType::Text && TextLayout::measure(*now).overflow) ++overflow;
            QTransform transform;
            const auto centre = now->rect.center();
            transform.translate(centre.x(), centre.y());
            transform.rotate(now->rotation);
            transform.translate(-centre.x(), -centre.y());
            if (!QRectF(QPointF(), document.size).contains(transform.mapRect(now->rect))) ++outside;
        }
        QStringList issues;
        if (detached) issues.append(QString("%1 unmatched objects stay in their previous positions.").arg(detached));
        if (overflow) issues.append(QString("%1 text boxes overflow after the change.").arg(overflow));
        if (outside) issues.append(QString("%1 objects extend beyond the slide.").arg(outside));
        result.slides.append(QVariantMap{
            {"id", before.id}, {"index", index}, {"name", QString("Slide %1").arg(index + 1)},
            {"moved", moved}, {"independent", detached}, {"created", created},
            {"overflow", overflow}, {"outside", outside}, {"issues", issues}});
    }
    return result;
}
