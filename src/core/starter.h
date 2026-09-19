#pragma once
#include "core/scene.h"
namespace Starter {
// Original built-in themes and layouts, generated through the same model as editing.
Document create(int theme, const QSizeF &size, int layout = 0);
bool validSize(const QSizeF &size);
} // namespace Starter
