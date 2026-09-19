#pragma once
#include "core/scene.h"
namespace Diagram {
struct Result {
  QVector<SceneObject> objects;
  QString error;
  QStringList warnings;
  int nodes = 0, levels = 0;
  bool ok() const { return error.isEmpty() && !objects.isEmpty(); }
};
Result build(const QSizeF &size, const DeckTheme &theme, int kind,
             const QString &outline, bool vertical);
} // namespace Diagram
