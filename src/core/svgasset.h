#pragma once
#include "core/scene.h"
namespace SvgAsset {
bool decode(SceneObject &object, const QByteArray &bytes,
            QString *error = nullptr);
}
