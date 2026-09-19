#include "backend.h"
#include "core/connector.h"
#include "core/design.h"
#include "core/edit.h"
#include "core/shape.h"
QVariantList Backend::connectorTargets() const {
  QVariantList targets;
  const auto slide = Design::resolve(m_document, m_currentSlide);
  int number = 0;
  for (const auto &o : slide.objects) {
    ++number;
    if (o.connector)
      continue;
    targets.append(
        QVariantMap{{"id", o.id},
                    {"name", o.text.isEmpty() ? tr("Object %1").arg(number)
                                              : o.text.left(45)}});
  }
  return targets;
}
bool Backend::connectSelected(int route) {
  const auto ids = selectedIds();
  if (ids.size() != 2 || route < 0 || route > 2)
    return false;
  const auto slide = Design::resolve(m_document, m_currentSlide);
  const auto *from = slide.find(ids[0]), *to = slide.find(ids[1]);
  if (!from || !to || from->connector || to->connector)
    return false;
  SceneObject o;
  o.id = Edit::newId("connector");
  o.connector = true;
  o.connectorFrom = from->id;
  o.connectorTo = to->id;
  o.connectorStart = from->rect.center();
  o.connectorEnd = to->rect.center();
  o.connectorRoute = route;
  o.shapeKind = Shape::custom;
  o.fillStyle = 5;
  o.strokeWidth = 3;
  o.strokeColor = m_document.theme.colors.value("foreground");
  o.groups = m_groupScope;
  m_history.begin(m_document, tr("Connect objects"));
  m_document.slides[m_currentSlide].objects.append(o);
  m_history.commit();
  m_selectedId = o.id;
  m_selectedIds = {o.id};
  touch();
  return true;
}
void Backend::attachConnector(bool start, const QString &target, int side) {
  if (selectionCount() != 1)
    return;
  const auto shown = Design::resolve(m_document, m_currentSlide);
  const auto *line = shown.find(selectedId()), *object = shown.find(target);
  if (!line || !line->connector ||
      (!target.isEmpty() && (!object || object->connector)) || side < 0 ||
      side > 4)
    return;
  const auto id = line->id;
  const auto point = start ? line->connectorStart : line->connectorEnd;
  m_history.begin(m_document, tr("Attach connector"));
  auto *o = m_document.slides[m_currentSlide].find(id);
  if (start) {
    o->connectorFrom = target;
    o->connectorFromSide = side;
    o->connectorStart = point;
  } else {
    o->connectorTo = target;
    o->connectorToSide = side;
    o->connectorEnd = point;
  }
  m_history.commit();
  touch();
}
