#pragma once
#include "core/scene.h"

namespace LayoutApply {
struct Result {
    Document document;
    QVariantList slides, mappings;
    QString error;
    bool ok() const { return error.isEmpty() && !slides.isEmpty(); }
};
QString key(const QString &layoutId, const QString &role);
Result preview(const Document &document, const QString &layoutId,
               const QStringList &slideIds, const QVariantMap &mapping, int geometry);
}
