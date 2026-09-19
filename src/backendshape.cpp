#include "backend.h"
#include "core/design.h"
#include "core/edit.h"
#include "core/objectstyle.h"
#include "core/shape.h"
#include "filepicker.h"
#include <QTransform>
#include <cmath>
QStringList Backend::shapeNames() const { return Shape::names(); }
void Backend::addShape(int kind) {
  if (kind < 0 || kind >= Shape::names().size() || m_currentSlide < 0 ||
      m_currentSlide >= m_document.slides.size())
    return;
  m_history.begin(m_document, tr("Add %1").arg(Shape::names()[kind]));
  const auto id = Edit::addRect(
      m_document, m_currentSlide,
      QPointF(m_document.size.width() / 2, m_document.size.height() / 2));
  auto *o = m_document.slides[m_currentSlide].find(id);
  o->shapeKind = kind;
  o->groups = m_groupScope;
  if (kind == 13 || kind == 22) {
    o->strokeWidth = 4;
    o->strokeColor = m_document.theme.colors.value("accent");
    o->fillStyle = 5;
  }
  m_history.commit();
  m_selectedId = id;
  m_selectedIds = {id};
  touch();
}
QVector<SceneObject> Backend::combinedShapes(int operation) const {
  const auto ids = selectedIds();
  const auto slide = Design::resolve(m_document, m_currentSlide);
  QVector<SceneObject> selected;
  // Stack order determines the style and the first operand of subtraction.
  for (const auto &o : slide.objects)
    if (ids.contains(o.id))
      selected.append(o);
  return Shape::combine(selected, operation);
}
bool Backend::combineShapes(int operation) {
  auto results = combinedShapes(operation);
  if (results.isEmpty())
    return false;
  const auto ids = selectedIds();
  const auto firstId = results.first().id;
  m_history.begin(m_document, tr("Combine shapes"));
  for (const auto &id : ids)
    if (id != firstId)
      Edit::deleteObject(m_document, m_currentSlide, id);
  QStringList added;
  auto &slide = m_document.slides[m_currentSlide];
  int firstIndex = 0;
  for (int i = 0; i < slide.objects.size(); ++i)
    if (slide.objects[i].id == firstId)
      firstIndex = i;
  for (int i = 0; i < results.size(); ++i) {
    auto &o = results[i];
    o.placeholderId.clear();
    o.overrides.clear();
    o.groups = m_groupScope;
    o.id = i == 0 ? firstId : Edit::newId("path");
    added.append(o.id);
    if (i == 0)
      slide.objects[firstIndex] = o;
    else
      slide.objects.insert(firstIndex + i, o);
  }
  m_history.commit();
  m_selectedIds = added;
  m_selectedId = added.last();
  touch();
  return true;
}
void Backend::setShapeImageDialog() {
  const auto *o = selectedObject();
  if (!o || o->type != ObjectType::Rect)
    return;
  m_imageTargetId = o->id;
  m_imageSlideId = m_document.slides.value(m_currentSlide).id;
  m_pending = Pending::ReplaceImage;
  m_chooser->openFile(tr("Choose shape picture fill"), tr("Pictures"),
                      {"*.svg", "*.SVG", "*.png", "*.jpg", "*.jpeg", "*.webp",
                       "*.PNG", "*.JPG", "*.JPEG", "*.WEBP"});
}
bool Backend::addPath(const QVariantList &points, bool closed, bool smooth) {
  if (points.size() < 2 || points.size() > 10000 || m_currentSlide < 0 ||
      m_currentSlide >= m_document.slides.size())
    return false;
  QVector<QPointF> vertices;
  for (const auto &value : points) {
    const auto row = value.toMap();
    bool okX = false, okY = false;
    qreal x = row.value("x").toDouble(&okX), y = row.value("y").toDouble(&okY);
    if (!okX || !okY || !std::isfinite(x) || !std::isfinite(y) ||
        std::abs(x) > 1000000 || std::abs(y) > 1000000)
      return false;
    vertices.append({x, y});
  }
  QPainterPath path;
  path.moveTo(vertices.first());
  for (int i = 1; i < vertices.size(); ++i) {
    if (smooth) {
      const auto before = vertices[qMax(0, i - 2)], a = vertices[i - 1],
                 b = vertices[i],
                 after = vertices[qMin(i + 1, int(vertices.size() - 1))];
      path.cubicTo(a + (b - before) / 6, b - (after - a) / 6, b);
    } else
      path.lineTo(vertices[i]);
  }
  if (closed)
    path.closeSubpath();
  SceneObject o;
  if (!Shape::assignPath(o, path))
    return false;
  o.id = Edit::newId("path");
  o.groups = m_groupScope;
  o.fill = m_document.theme.colors.value("accent");
  o.fillToken = "accent";
  o.fillStyle = closed ? 0 : 5;
  o.strokeWidth = 3;
  o.strokeColor = m_document.theme.colors.value("foreground");
  m_history.begin(m_document, tr("Draw path"));
  m_document.slides[m_currentSlide].objects.append(o);
  m_history.commit();
  m_selectedId = o.id;
  m_selectedIds = {o.id};
  touch();
  return true;
}
void Backend::convertToPath() {
  const auto shown = Design::resolve(m_document, m_currentSlide);
  const auto ids = selectedIds();
  bool valid = false;
  for (const auto &id : ids)
    if (const auto *o = shown.find(id))
      if (o->type == ObjectType::Rect && (o->shapeKind != Shape::custom || o->connector))
        valid = true;
  if (!valid)
    return;
  m_history.begin(m_document, tr("Convert to path"));
  for (const auto &id : ids) {
    const auto *source = shown.find(id);
    auto *o = m_document.slides[m_currentSlide].find(id);
    if (!o || !source || source->type != ObjectType::Rect)
      continue;
    const auto identity = o->id;
    *o = *source;
    Shape::assignPath(*o, Shape::worldPath(*source));
    o->id = identity;
    o->connector=false; o->connectorFrom.clear(); o->connectorTo.clear();
    o->placeholderId.clear();
    o->overrides.clear();
  }
  m_history.commit();
  touch();
}
void Backend::setPathClosed(bool closed) {
  const auto ids = selectedIds();
  if (ids.size() != 1)
    return;
  const auto shown = Design::resolve(m_document, m_currentSlide);
  const auto *source = shown.find(ids.first());
  if (!source || source->shapeKind != Shape::custom)
    return;
  QVariantList data;
  bool open = false;
  for (const auto &value : source->pathData) {
    const auto command = value.toList().value(0).toString();
    if (command == "Z") {
      if (closed)
        data.append(value);
      open = false;
      continue;
    }
    if (command == "M" && open && closed)
      data.append(QVariant(QVariantList{"Z"}));
    data.append(value);
    open = true;
  }
  if (open && closed)
    data.append(QVariant(QVariantList{"Z"}));
  setSelectedProperty("pathData", data);
}
void Backend::movePathNode(int command, int coordinate, qreal x, qreal y) {
  const auto ids = selectedIds();
  if (ids.size() != 1 || !std::isfinite(x) || !std::isfinite(y))
    return;
  const auto shown = Design::resolve(m_document, m_currentSlide);
  const auto *source = shown.find(ids.first());
  if (!source || source->shapeKind != Shape::custom || command < 0 ||
      command >= source->pathData.size() || source->rect.isEmpty())
    return;
  auto data = source->pathData;
  auto row = data[command].toList();
  if (coordinate < 1 || coordinate + 1 >= row.size() || coordinate % 2 != 1)
    return;
  const auto c = source->rect.center();
  QTransform t;
  t.translate(c.x(), c.y());
  t.rotate(source->rotation);
  t.translate(-c.x(), -c.y());
  const auto p = t.inverted().map(QPointF(x, y));
  row[coordinate] = (p.x() - source->rect.x()) / source->rect.width();
  row[coordinate + 1] = (p.y() - source->rect.y()) / source->rect.height();
  data[command] = row;
  setSelectedProperty("pathData", data);
}
void Backend::removePathNode(int command) {
  const auto ids = selectedIds();
  if (ids.size() != 1)
    return;
  const auto shown = Design::resolve(m_document, m_currentSlide);
  const auto *source = shown.find(ids.first());
  if (!source || source->shapeKind != Shape::custom || command < 0 ||
      command >= source->pathData.size())
    return;
  auto data = source->pathData;
  const auto row = data[command].toList();
  if (row[0].toString() == "M" || row[0].toString() == "Z")
    return;
  data.removeAt(command);
  QPainterPath path;
  if (Shape::decode(data, &path) && !path.isEmpty())
    setSelectedProperty("pathData", data);
}

QVariantList Backend::objectStyles() const {
  QVariantList result;
  for (const auto &style : m_document.objectStyles)
    result.append(QVariantMap{{"id", style.id}, {"name", style.name}});
  return result;
}
void Backend::saveObjectStyle(const QString &name, const QString &existingId) {
  if (selectionCount() != 1 || name.trimmed().isEmpty() ||
      (existingId.isEmpty() && m_document.objectStyles.size() >= 1000))
    return;
  const auto slide = Design::resolve(m_document, m_currentSlide);
  const auto *source = slide.find(selectedId());
  if (!source)
    return;
  if (!existingId.isEmpty()) {
    bool exists = false;
    for (const auto &style : m_document.objectStyles)
      if (style.id == existingId)
        exists = true;
    if (!exists)
      return;
  }
  const auto appearance = ObjectStyles::capture(*source);
  m_history.begin(m_document, tr("Save object style"));
  if (existingId.isEmpty())
    m_document.objectStyles.append(
        {Edit::newId("style"), name.trimmed().left(120), appearance});
  else
    for (auto &style : m_document.objectStyles)
      if (style.id == existingId) {
        style.name = name.trimmed().left(120);
        style.appearance = appearance;
        break;
      }
  m_history.commit();
  touch();
}
void Backend::applyObjectStyle(const QString &id) {
  const auto ids = selectedIds();
  if (ids.isEmpty())
    return;
  SceneObject appearance;
  bool found = false;
  for (const auto &style : m_document.objectStyles)
    if (style.id == id) {
      appearance = style.appearance;
      found = true;
      break;
    }
  if (!found)
    return;
  m_history.begin(m_document, tr("Apply object style"));
  for (const auto &objectId : ids)
    if (auto *o = m_document.slides[m_currentSlide].find(objectId))
      ObjectStyles::apply(*o, appearance);
  m_history.commit();
  touch();
}
void Backend::removeObjectStyle(const QString &id) {
  for (int i = 0; i < m_document.objectStyles.size(); ++i)
    if (m_document.objectStyles[i].id == id) {
      m_history.begin(m_document, tr("Remove saved style"));
      m_document.objectStyles.removeAt(i);
      m_history.commit();
      touch();
      return;
    }
}
