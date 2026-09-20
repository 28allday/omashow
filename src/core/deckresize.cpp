#include "core/deckresize.h"
#include "core/starter.h"
#include "core/table.h"
bool DeckResize::apply(Document &document, const QSizeF &size,
                       bool scaleContent) {
  if (!Starter::validSize(size) || document.size.isEmpty() ||
      document.size == size)
    return false;
  if (scaleContent) {
    const qreal scale = qMin(size.width() / document.size.width(),
                             size.height() / document.size.height());
    const QPointF offset((size.width() - document.size.width() * scale) / 2,
                         (size.height() - document.size.height() * scale) / 2);
    const auto transform = [&](SceneObject &o) {
      scaleObject(o, scale);
      o.rect.translate(offset);
      o.connectorStart += offset;
      o.connectorEnd += offset;
    };
    for (auto &master : document.masters)
      for (auto &o : master.objects)
        transform(o);
    for (auto &layout : document.layouts)
      for (auto &o : layout.placeholders)
        transform(o);
    for (auto &slide : document.slides)
      for (auto &o : slide.objects)
        transform(o);
  }
  document.size = size;
  return true;
}

void DeckResize::scaleObject(SceneObject &o, qreal scale, const QPointF &origin) {
  o.rect = QRectF(origin + (o.rect.topLeft() - origin) * scale, o.rect.size() * scale);
  Table::scale(o.table, scale);
  o.fontSize *= scale;
  o.letterSpacing *= scale;
  o.paragraphSpacing *= scale;
  o.textIndent *= scale;
  o.cornerRadius *= scale;
  o.connectorStart = origin + (o.connectorStart - origin) * scale;
  o.connectorEnd = origin + (o.connectorEnd - origin) * scale;
  o.strokeWidth *= scale;
  o.shadowX *= scale;
  o.shadowY *= scale;
}
