#include <QTransform>
#include "core/arrange.h"
#include "core/design.h"
bool Arrange::inScope(const SceneObject &o, const QStringList &scope) {
  return o.groups.mid(0, scope.size()) == scope;
}
QString Arrange::unitKey(const SceneObject &o, const QStringList &scope) {
  return o.groups.size() > scope.size() ? o.groups.at(scope.size()) : o.id;
}
QVector<Arrange::Unit> Arrange::units(const Slide &slide,
                                      const QStringList &ids,
                                      const QStringList &scope) {
  QVector<Unit> result;
  for (const auto &id : ids)
    if (const auto *o = slide.find(id)) {
      const QString key = unitKey(*o, scope);
      int index = -1;
      for (int i = 0; i < result.size(); ++i)
        if (result.at(i).key == key)
          index = i;
      if (index < 0)
        result.append({key, {id}, o->rect});
      else {
        result[index].ids.append(id);
        result[index].rect = result[index].rect.united(o->rect);
      }
    }
  return result;
}
QRectF Arrange::bounds(const Slide &slide, const QStringList &ids) {
  QRectF rect;
  bool first = true;
  for (const auto &id : ids)
    if (const auto *o = slide.find(id)) {
      rect = first ? o->rect : rect.united(o->rect);
      first = false;
    }
  return rect;
}
void Arrange::setRect(SceneObject &o, const QRectF &before,
                      const QRectF &after) {
  o.rect = after;
  if (before.x() != after.x())
    Design::markOverride(o, "x");
  if (before.y() != after.y())
    Design::markOverride(o, "y");
  if (before.width() != after.width())
    Design::markOverride(o, "w");
  if (before.height() != after.height())
    Design::markOverride(o, "h");
}

QRectF Arrange::visualBounds(const Slide &slide,const QStringList &ids) {
  QRectF bounds;
  for(const auto &o:slide.objects) if(ids.contains(o.id) && !o.hidden) {
    QTransform t; const auto c=o.rect.center(); t.translate(c.x(),c.y()); t.rotate(o.rotation); t.translate(-c.x(),-c.y());
    bounds=bounds.united(t.mapRect(o.rect));
  }
  return bounds;
}
