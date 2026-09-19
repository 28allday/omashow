#pragma once
#include "core/scene.h"
namespace ObjectCopy {
Document extract(const Document &source, int slide, const QStringList &ids, int scopeDepth = 0);
QStringList insert(Document &destination, int slide, const Document &fragment,
                   const QStringList &scope = {}, const QPointF &offset = QPointF(24, 24));
} // namespace ObjectCopy
