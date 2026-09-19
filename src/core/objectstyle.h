#pragma once
#include "core/scene.h"
namespace ObjectStyles {
QStringList keys();
SceneObject capture(const SceneObject &object);
void apply(SceneObject &object, const SceneObject &appearance);
} // namespace ObjectStyles
