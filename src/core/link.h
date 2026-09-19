#pragma once
#include "core/scene.h"
#include <QUrl>
namespace Links {
// None, web, email, slide, next, previous, first, last, end show.
QString validate(int kind, const QString &target, const Document &document,
                 bool allowMissingSlide = false);
QString normalized(int kind, const QString &target);
QString issue(const SceneObject &object, const Document &document);
} // namespace Links
