#pragma once
#include "core/scene.h"
#include <QVariantMap>

namespace Design {
const SlideLayout *layout(const Document &document, const QString &id);
const Master *master(const Document &document, const QString &id);
const TextStyle *textStyle(const Document &document, const QString &id);
// The text properties a named style carries. Everything else is the box's own.
QStringList textStyleKeys();
SceneObject themed(const DeckTheme &theme, SceneObject object);
Slide resolve(const Document &document, int index);
DeckTheme preset(int index);
QVariantList themeContrast(const DeckTheme &theme);
// WCAG 2.2 contrast, unrounded, with any foreground alpha composited over the
// background. 0 when either colour is unknown or the background is not opaque.
qreal contrastRatio(QColor foreground, const QColor &background);
QVariantMap fieldProperties(const MasterFields &fields);
bool setFieldProperty(MasterFields &fields, const QString &key, const QVariant &value);
QVector<SceneObject> fields(const Document &document, int index, const Master &master);
void ensureDefaults(Document &document);
bool applyLayout(Document &document, int index, const QString &id);
// Geometry: 0 keeps local overrides, 1 reapplies layout positions, 2 keeps all positions.
// Mapping keys are source placeholder roles; an empty target keeps an independent object.
bool applyLayout(Document &document, int index, const QString &id,
                 const QVariantMap &mapping, int geometry);
SceneObject detached(SceneObject object);
QVariantMap properties(const SceneObject &object);
// Manual editing constrains input; resolution/loading preserve already-authored scaled values.
bool setProperty(SceneObject &object, const QString &key, const QVariant &value, bool constrainForEditing = true);
void markOverride(SceneObject &object, const QString &key);
void reset(SceneObject &object, bool geometry);
} // namespace Design
