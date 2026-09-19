#pragma once
#include "core/scene.h"
#include <QVariantMap>

namespace Design {
const SlideLayout *layout(const Document &document, const QString &id);
const Master *master(const Document &document, const QString &id);
SceneObject themed(const DeckTheme &theme, SceneObject object);
Slide resolve(const Document &document, int index);
DeckTheme preset(int index);
void ensureDefaults(Document &document);
bool applyLayout(Document &document, int index, const QString &id);
SceneObject detached(SceneObject object);
QVariantMap properties(const SceneObject &object);
// Manual editing constrains input; resolution/loading preserve already-authored scaled values.
bool setProperty(SceneObject &object, const QString &key, const QVariant &value, bool constrainForEditing = true);
void markOverride(SceneObject &object, const QString &key);
void reset(SceneObject &object, bool geometry);
} // namespace Design
