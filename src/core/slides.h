#pragma once
#include "core/scene.h"
namespace Slides {
QVector<int> indices(const Document &document, const QStringList &ids);
QStringList duplicate(Document &document, const QStringList &ids);
bool remove(Document &document, const QStringList &ids);
bool move(Document &document, const QStringList &ids, int boundary,
          const QString &destinationSection);
// Boundary in authored slide coordinates for an adjacent section move, or -1.
int sectionBoundary(const Document &document, const QString &id, int direction);
bool moveSection(Document &document, const QString &id, int direction);
bool nudge(Document &document, const QStringList &ids, int direction);
} // namespace Slides
