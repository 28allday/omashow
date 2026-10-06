#include "backend.h"
#include "core/textruns.h"
#include "core/punctuation.h"
#include "core/mediaasset.h"
#include "core/arrange.h"
#include "core/design.h"
#include "core/edit.h"
#include "core/snap.h"
#include "render/textlayout.h"
#include "render/mathlayout.h"
#include <QQuickTextDocument>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextList>
#include <QTransform>
#include "core/shape.h"
#include "core/link.h"
#include "core/imagecrop.h"
#include "core/table.h"
#include "core/chart.h"
#include <algorithm>
#include <cmath>

QStringList Backend::selectedIds() const {
  QStringList result;
  if (m_currentSlide < 0 || m_currentSlide >= m_document.slides.size())
    return result;
  const QStringList candidates =
      m_selectedIds.isEmpty() ? QStringList{m_selectedId} : m_selectedIds;
  for (const auto &id : candidates)
    if (!id.isEmpty() && m_document.slides.at(m_currentSlide).find(id))
      result.append(id);
  return result;
}
QVariantMap Backend::selection() const {
  const auto ids = selectedIds();
  if (ids.isEmpty())
    return {};
  const auto slide = Design::resolve(m_document, m_currentSlide);
  const auto *primary = slide.find(ids.last());
  if (!primary)
    return {};
  auto values = Design::properties(*primary);
  values["linkIssue"]=Links::issue(*primary,m_document);
  if (primary->type == ObjectType::Text) {
    const auto metrics = TextLayout::measure(*primary);
    values["textOverflow"] = metrics.overflow;
    values["textNaturalHeight"] = metrics.naturalHeight;
    values["effectiveFontSize"] = metrics.effectiveSize;
    values["editHtml"] = TextLayout::editorHtml(*primary);
    // An equation says what stopped it, so the box can explain itself.
    if (primary->textKind == 1)
      values["equationError"] =
          MathLayout::build(primary->text, QFont(primary->fontFamily), primary->fontSize,
                            primary->textAlign)
              .error;
  }
  if(primary->type==ObjectType::Chart) {const auto layout=Chart::layout(*primary);values["chartIssues"]=layout.issues;values["chartWarnings"]=layout.warnings;QVariantList series;const bool circular=primary->chart.kind==7 || primary->chart.kind==8;for(int i=0;i<(circular?primary->table.rows.size()-1:primary->table.columns.size()-1);++i) {const auto &cell=primary->table.cells[circular?(i+1)*primary->table.columns.size():i+1];const auto fallback=primary->chart.resolvedColors.value(i%8,primary->textColor);series.append(QVariantMap{{"id",cell.id},{"name",cell.text},{"color",primary->chart.seriesColors.value(cell.id,fallback).name()}});}values["chartSeries"]=series;}
  if(primary->type==ObjectType::Table) values["tableOverflow"]=Table::overflowCount(*primary);
  if(primary->type==ObjectType::Media) {
    values["mediaHasOriginal"]=bool(primary->mediaOriginal);
    values["mediaState"]=MediaAsset::linkState(*primary,m_mediaPermissions.value(primary->mediaPath)==primary->mediaId);
    values["mediaOnClick"]=false;
    for(const auto &step:m_document.slides.at(m_currentSlide).timeline.steps) if(step.targetId==primary->id && step.effect==Effect::Media) values["mediaOnClick"]=step.trigger==BuildTrigger::OnClick;
  }
  values["imageHasOriginal"]=bool(primary->imageOriginal);
  if(primary->type==ObjectType::Image) { const auto frame=ImageCrop::frame(*primary); values["cropFrameX"]=frame.x(); values["cropFrameY"]=frame.y(); values["cropFrameW"]=frame.width(); values["cropFrameH"]=frame.height(); }
  const auto rect = Arrange::bounds(slide, ids);
  values["x"] = rect.x();
  values["y"] = rect.y();
  values["w"] = rect.width();
  values["h"] = rect.height();
  values["count"] = ids.size();
  if (ids.size() > 1) {
    values["type"] = "group";
    if (primary->type == ObjectType::Text) values["fill"] = primary->textColor.name(QColor::HexArgb);
    values["placeholderId"] = "";
  }
  const auto units = Arrange::units(slide, ids, m_groupScope);
  values["groupId"] =
      units.size() == 1 && primary->groups.size() > m_groupScope.size()
          ? units.first().key
          : QString();
  return values;
}
void Backend::selectIds(const QStringList &requested) {
  const auto slide = Design::resolve(m_document, m_currentSlide);
  QStringList ids;
  for (const auto &id : requested)
    if (const auto *source = slide.find(id)) {
      if (source->locked || source->hidden ||
          !Arrange::inScope(*source, m_groupScope))
        continue;
      const auto key = Arrange::unitKey(*source, m_groupScope);
      for (const auto &o : slide.objects)
        if (!o.locked && !o.hidden && Arrange::inScope(o, m_groupScope) &&
            Arrange::unitKey(o, m_groupScope) == key && !ids.contains(o.id) &&
            m_document.slides.at(m_currentSlide).find(o.id))
          ids.append(o.id);
    }
  if (ids == m_selectedIds)
    return;
  m_selectedIds = ids;
  m_selectedId = ids.isEmpty() ? QString() : ids.last();
  emit selectionChanged();
}
void Backend::select(const QString &id) {
  selectIds(id.isEmpty() ? QStringList() : QStringList{id});
}
void Backend::clearSelection() {
  m_selectedIds.clear();
  m_selectedId.clear();
  emit selectionChanged();
}
void Backend::selectAll() {
  QStringList ids;
  const auto slide = Design::resolve(m_document, m_currentSlide);
  for (const auto &o : slide.objects)
    if (!o.locked && !o.hidden && Arrange::inScope(o, m_groupScope))
      ids.append(o.id);
  selectIds(ids);
}
// The objects a click at (x, y) picks, topmost first: visible, unlocked, in
// the group being edited, and on this slide (not the layout or master).
static QStringList hitsAt(const Slide &resolved, const Slide &own, const QStringList &scope,
                          qreal x, qreal y, bool firstOnly) {
  QStringList hits;
  for (int i = resolved.objects.size() - 1; i >= 0; --i) {
    const auto &o = resolved.objects.at(i);
    if (o.hidden || o.locked || !Arrange::inScope(o, scope))
      continue;
    if (!own.find(o.id))
      continue;
    QTransform t;
    const auto c = o.rect.center();
    t.translate(c.x(), c.y());
    t.rotate(o.rotation);
    t.translate(-c.x(), -c.y());
    if (Shape::contains(o,t.inverted().map(QPointF(x, y)))) {
      hits.append(o.id);
      if (firstOnly)
        break;
    }
  }
  return hits;
}
bool Backend::hitsObject(qreal x, qreal y) const {
  if (m_currentSlide < 0 || m_currentSlide >= m_document.slides.size())
    return false;
  return !hitsAt(Design::resolve(m_document, m_currentSlide), m_document.slides.at(m_currentSlide),
                 m_groupScope, x, y, true).isEmpty();
}
bool Backend::selectAt(qreal x, qreal y, bool extend, bool behind) {
  const auto slide = Design::resolve(m_document, m_currentSlide);
  const QStringList hits = hitsAt(slide, m_document.slides.at(m_currentSlide), m_groupScope, x, y, false);
  if (hits.isEmpty()) {
    if (!extend)
      clearSelection();
    return false;
  }
  QString hit = hits.first();
  if (behind && hits.contains(m_selectedId))
    hit = hits.at((hits.indexOf(m_selectedId) + 1) % hits.size());
  if (!extend && !behind && selectedIds().contains(hit))
    return true;
  if (!extend) {
    select(hit);
    return true;
  }
  QStringList ids = selectedIds();
  const auto key = Arrange::unitKey(*slide.find(hit), m_groupScope);
  const bool remove = ids.contains(hit);
  for (const auto &o : slide.objects)
    if (Arrange::unitKey(o, m_groupScope) == key) {
      if (remove)
        ids.removeAll(o.id);
      else if (!ids.contains(o.id))
        ids.append(o.id);
    }
  selectIds(ids);
  return true;
}
void Backend::selectRegion(qreal x, qreal y, qreal w, qreal h, bool extend) {
  const QRectF region = QRectF(x, y, w, h).normalized();
  QStringList ids = extend ? selectedIds() : QStringList();
  const auto slide = Design::resolve(m_document, m_currentSlide);
  for (const auto &o : slide.objects)
    if (region.intersects(o.rect) && !ids.contains(o.id))
      ids.append(o.id);
  selectIds(ids);
}
namespace {
// What a slide's objects are, in a form that can be compared: a gesture that
// ends where it began leaves no step to undo.
QVariantList objectsOf(const Document &document, int slide) {
  QVariantList out;
  if (slide < 0 || slide >= document.slides.size()) return out;
  for (const auto &o : document.slides.at(slide).objects) out.append(Design::properties(o));
  return out;
}
} // namespace

void Backend::beginEdit(const QString &label) {
  ++m_gestureDepth;
  if (!m_gestureActive) {
    m_gestureWasModified = m_modified;
    m_gestureBefore = objectsOf(m_document, m_currentSlide);
    m_gestureBasis = Design::resolve(m_document, m_currentSlide);
    m_gestureBounds = Arrange::bounds(m_gestureBasis, selectedIds());
    m_gestureActive = true;
  }
  m_history.begin(m_document, label);
}
void Backend::endEdit() {
  if(!m_gestureActive || m_gestureDepth <= 0) return;
  // An inner gesture ends inside an outer one: the outer one carries on.
  if (m_gestureDepth > 1) { m_history.commit(); --m_gestureDepth; return; }
  m_gestureDepth = 0;
  m_gestureActive = false;
  m_guides.clear();
  emit guidesChanged();
  if (objectsOf(m_document, m_currentSlide) == m_gestureBefore) {
    // A click, or a drag that came back to where it started: nothing to undo.
    m_history.abandon();
    m_gestureBefore.clear();
    m_modified = m_gestureWasModified;
    ++m_revision;
    emit documentChanged();
    return;
  }
  m_gestureBefore.clear();
  m_history.commit();
  touch();
}
void Backend::cancelEdit() {
  if (!m_history.cancel(m_document))
    return;
  m_gestureActive = false;
  m_gestureDepth = 0;
  m_guides.clear();
  m_modified = m_gestureWasModified;
  ++m_revision;
  emit guidesChanged();
  emit documentChanged();
  emit deckChanged();
  emit selectionChanged();
}
void Backend::setSelectedRect(qreal x, qreal y, qreal w, qreal h,
                              qreal tolerance, bool keepSize) {
  const auto ids = selectedIds();
  if (ids.isEmpty() || !std::isfinite(x) || !std::isfinite(y) ||
      !std::isfinite(w) || !std::isfinite(h))
    return;
  const auto shown = Design::resolve(m_document, m_currentSlide);
  const auto basis = m_gestureActive ? m_gestureBasis : shown;
  const QRectF original =
      m_gestureActive ? m_gestureBounds : Arrange::bounds(shown, ids);
  if (original.width() <= 0 || original.height() <= 0)
    return;
  QRectF proposed(x, y, qMax(8.0, w), qMax(8.0, h));
  m_guides.clear();
  if (m_snapEnabled && tolerance > 0) {
    Slide others = shown;
    for (int i = others.objects.size() - 1; i >= 0; --i)
      if (ids.contains(others.objects.at(i).id) || others.objects.at(i).hidden)
        others.objects.removeAt(i);
    const auto snap = Snap::adjust(others, m_document.size, QString(), proposed,
                                   tolerance, keepSize);
    proposed = snap.rect;
    for (const auto &g : snap.guides)
      m_guides.append(QVariantMap{{"vertical", g.vertical},
                                  {"position", g.position},
                                  {"from", g.from},
                                  {"to", g.to}});
  }
  for (const auto &id : ids) {
    const auto *before = basis.find(id);
    const auto *current = shown.find(id);
    if (!before || !current)
      continue;
    const QRectF r = before->rect;
    const QRectF changed(proposed.x() + (r.x() - original.x()) *
                                            proposed.width() / original.width(),
                         proposed.y() + (r.y() - original.y()) *
                                            proposed.height() /
                                            original.height(),
                         r.width() * proposed.width() / original.width(),
                         r.height() * proposed.height() / original.height());
    if (auto *o = m_document.slides[m_currentSlide].find(id))
      Arrange::setRect(*o, current->rect, changed);
  }
  emit guidesChanged();
  touch();
}
void Backend::nudgeSelected(qreal dx, qreal dy) {
  const auto ids = selectedIds();
  if (ids.size()==1 && m_document.slides.at(m_currentSlide).find(ids.first())->connector) return;
  if (ids.isEmpty())
    return;
  const auto shown = Design::resolve(m_document, m_currentSlide);
  m_history.begin(m_document, tr("Move objects"));
  for (const auto &id : ids)
    if (auto *o = m_document.slides[m_currentSlide].find(id)) {
      const auto *before = shown.find(id);
      if (before)
        Arrange::setRect(*o, before->rect, before->rect.translated(dx, dy));
    }
  m_history.commit();
  touch();
}
void Backend::setSelectedProperty(const QString &key, const QVariant &given) {
  if(key.startsWith("media")) return; // Media changes must keep the cue and source consistent.
  // Assets are set by inserting a picture; links by setObjectLink, which checks them.
  static const QStringList guarded{"imageId", "imageFormat", "imageOriginal", "linkKind", "linkTarget"};
  if(guarded.contains(key)) return;
  const auto ids = selectedIds();
  if (ids.isEmpty())
    return;
  // The marks people mean rather than the ones on the keyboard, settled before
  // anything is compared so retyping a straight quote is not an edit.
  QVariant value = given;
  if (key == QLatin1String("text") && m_document.smartPunctuation) {
    const auto *first = m_document.slides.at(m_currentSlide).find(ids.first());
    if (!first || first->textKind != 1) value = Punctuation::smarten(given.toString());
  }
  if(ids.size()==1 && m_document.slides.at(m_currentSlide).find(ids.first())->connector && QStringList{"x","y","w","h","rotation","shapeKind","pathData"}.contains(key)) return;
  if (ids.size() > 1 &&
      (key == "x" || key == "y" || key == "w" || key == "h")) {
    auto s = selection();
    bool ok = false;
    const qreal n = value.toDouble(&ok);
    if (!ok || !std::isfinite(n))
      return;
    s[key] = n;
    beginEdit(tr("Transform objects"));
    setSelectedRect(s["x"].toDouble(), s["y"].toDouble(), s["w"].toDouble(),
                    s["h"].toDouble());
    endEdit();
    return;
  }
  const auto shown = Design::resolve(m_document, m_currentSlide);
  const auto propertyKey = [&ids, &key](const SceneObject &object) {
    return ids.size() > 1 && key == "fill" && object.type == ObjectType::Text ? QString("textColor") : key;
  };
  bool changed = false;
  for (const auto &id : ids)
    if (const auto *o = shown.find(id)) {
      auto copy = *o;
      if (!Design::setProperty(copy, propertyKey(*o), value))
        return;
      if (Design::properties(copy).value(propertyKey(*o)) !=
          Design::properties(*o).value(propertyKey(*o)))
        changed = true;
    }
  if (!changed)
    return;
  QString label=key;
  const QMap<QString,QString> labels={{"imageBrightness",tr("brightness")},{"imageContrast",tr("contrast")},{"imageSaturation",tr("saturation")},{"imageTintAmount",tr("tint amount")},{"imageTint",tr("tint colour")},{"imageMask",tr("picture mask")},{"imageFocalX",tr("horizontal focal point")},{"imageFocalY",tr("vertical focal point")},{"pathData",tr("path")},{"fillStyle",tr("fill type")},{"fillSecondary",tr("second fill colour")},{"strokeWidth",tr("stroke width")},{"connectorRoute",tr("connector routing")}};
  label=labels.value(key,label);
  m_history.begin(m_document, tr("Change %1").arg(label));
  for (const auto &id : ids)
    if (auto *o = m_document.slides[m_currentSlide].find(id)) {
      // Rewriting the words takes their looks along with the letters.
      if (key == QLatin1String("text") && o->type == ObjectType::Text) {
        o->runs = TextRuns::afterEdit(o->runs, o->text, value.toString());
        Design::setProperty(*o, QStringLiteral("text"), value);
        Design::markOverride(*o, QStringLiteral("text"));
        continue;
      }
      Design::setProperty(*o, propertyKey(*o), value);
      Design::markOverride(*o, propertyKey(*o));
    }
  m_history.commit();
  if (key == "hidden" || key == "locked")
    clearSelection();
  touch();
}
void Backend::deleteSelected() {
  const auto ids = selectedIds();
  if (ids.isEmpty())
    return;
  m_history.begin(m_document, tr("Delete objects"));
  for (const auto &id : ids)
    Edit::deleteObject(m_document, m_currentSlide, id);
  m_history.commit();
  clearSelection();
  touch();
}
void Backend::raiseSelected(int delta) {
  const auto ids = selectedIds();
  if (ids.isEmpty())
    return;
  m_history.begin(m_document,
                  delta > 0 ? tr("Bring forward") : tr("Send backward"));
  auto &objects = m_document.slides[m_currentSlide].objects;
  if (delta > 0) {
    for (int i = objects.size() - 2; i >= 0; --i)
      if (ids.contains(objects.at(i).id) && !ids.contains(objects.at(i + 1).id))
        objects.swapItemsAt(i, i + 1);
  } else {
    for (int i = 1; i < objects.size(); ++i)
      if (ids.contains(objects.at(i).id) && !ids.contains(objects.at(i - 1).id))
        objects.swapItemsAt(i, i - 1);
  }
  m_history.commit();
  touch();
}
void Backend::groupSelected() {
  const auto ids = selectedIds();
  const auto shown = Design::resolve(m_document, m_currentSlide);
  if (Arrange::units(shown, ids, m_groupScope).size() < 2)
    return;
  const QString group = Edit::newId("group");
  m_history.begin(m_document, tr("Group objects"));
  for (const auto &id : ids)
    if (auto *o = m_document.slides[m_currentSlide].find(id))
      o->groups.insert(m_groupScope.size(), group);
  m_history.commit();
  touch();
}
void Backend::ungroupSelected() {
  const auto ids = selectedIds();
  const auto shown = Design::resolve(m_document, m_currentSlide);
  QStringList groups;
  for (const auto &id : ids)
    if (const auto *o = shown.find(id))
      if (o->groups.size() > m_groupScope.size())
        groups.append(o->groups.at(m_groupScope.size()));
  if (groups.isEmpty())
    return;
  m_history.begin(m_document, tr("Ungroup objects"));
  for (auto &o : m_document.slides[m_currentSlide].objects)
    if (o.groups.size() > m_groupScope.size() &&
        groups.contains(o.groups.at(m_groupScope.size())))
      o.groups.removeAt(m_groupScope.size());
  m_history.commit();
  touch();
}
void Backend::enterGroup() {
  const QString group = selection().value("groupId").toString();
  if (group.isEmpty())
    return;
  m_groupScope.append(group);
  clearSelection();
}
void Backend::leaveGroup() {
  if (m_groupScope.isEmpty()) {
    clearSelection();
    return;
  }
  const QString group = m_groupScope.takeLast();
  const auto slide = Design::resolve(m_document, m_currentSlide);
  for (const auto &o : slide.objects)
    if (o.groups.contains(group)) {
      select(o.id);
      return;
    }
  clearSelection();
}
void Backend::alignSelected(const QString &mode, int reference) {
  const auto ids = selectedIds();
  const auto shown = Design::resolve(m_document, m_currentSlide);
  const auto units = Arrange::units(shown, ids, m_groupScope);
  if (units.isEmpty())
    return;
  const QStringList modes = {"left", "center", "right",
                             "top",  "middle", "bottom"};
  if (!modes.contains(mode))
    return;
  const QRectF target = reference == 1   ? QRectF(QPointF(), m_document.size)
                        : reference == 2 ? units.first().rect
                                         : Arrange::bounds(shown, ids);
  m_history.begin(m_document, tr("Align objects"));
  for (int i = 0; i < units.size(); ++i) {
    if (reference == 2 && i == 0)
      continue;
    const auto &unit = units.at(i);
    qreal dx = 0, dy = 0;
    if (mode == "left")
      dx = target.left() - unit.rect.left();
    if (mode == "center")
      dx = target.center().x() - unit.rect.center().x();
    if (mode == "right")
      dx = target.right() - unit.rect.right();
    if (mode == "top")
      dy = target.top() - unit.rect.top();
    if (mode == "middle")
      dy = target.center().y() - unit.rect.center().y();
    if (mode == "bottom")
      dy = target.bottom() - unit.rect.bottom();
    for (const auto &id : unit.ids)
      if (auto *o = m_document.slides[m_currentSlide].find(id)) {
        const auto r = shown.find(id)->rect;
        Arrange::setRect(*o, r, r.translated(dx, dy));
      }
  }
  m_history.commit();
  touch();
}
void Backend::distributeSelected(bool horizontal, int reference) {
  const auto ids = selectedIds();
  const auto shown = Design::resolve(m_document, m_currentSlide);
  auto units = Arrange::units(shown, ids, m_groupScope);
  if (units.size() < 3)
    return;
  std::stable_sort(
      units.begin(), units.end(), [horizontal](const auto &a, const auto &b) {
        return horizontal ? a.rect.x() < b.rect.x() : a.rect.y() < b.rect.y();
      });
  const QRectF target = reference == 1 ? QRectF(QPointF(), m_document.size)
                                       : Arrange::bounds(shown, ids);
  qreal extent = 0;
  for (const auto &u : units)
    extent += horizontal ? u.rect.width() : u.rect.height();
  const qreal gap = ((horizontal ? target.width() : target.height()) - extent) /
                    (units.size() - 1);
  qreal cursor = horizontal ? target.left() : target.top();
  m_history.begin(m_document, tr("Distribute objects"));
  for (const auto &u : units) {
    const qreal shift = cursor - (horizontal ? u.rect.left() : u.rect.top());
    for (const auto &id : u.ids)
      if (auto *o = m_document.slides[m_currentSlide].find(id)) {
        const auto r = shown.find(id)->rect;
        Arrange::setRect(
            *o, r,
            r.translated(horizontal ? shift : 0, horizontal ? 0 : shift));
      }
    cursor += (horizontal ? u.rect.width() : u.rect.height()) + gap;
  }
  m_history.commit();
  touch();
}
void Backend::showAllObjects() {
  m_history.begin(m_document, tr("Show all objects"));
  if (m_currentSlide >= 0 && m_currentSlide < m_document.slides.size())
    for (auto &o : m_document.slides[m_currentSlide].objects)
      o.hidden = false;
  m_history.commit();
  touch();
}
void Backend::unlockAllObjects() {
  m_history.begin(m_document, tr("Unlock all objects"));
  if (m_currentSlide >= 0 && m_currentSlide < m_document.slides.size())
    for (auto &o : m_document.slides[m_currentSlide].objects)
      o.locked = false;
  m_history.commit();
  touch();
}

void Backend::fitSelectedTextBox() {
  const auto shown = Design::resolve(m_document,m_currentSlide);
  const auto ids = selectedIds(); if (ids.isEmpty()) return;
  m_history.begin(m_document,tr("Fit box to text"));
  for (const auto &id:ids) if (const auto *source=shown.find(id)) if (source->type==ObjectType::Text) {
    auto copy=*source; copy.textFit=0;
    const auto height=qMax(8.0,TextLayout::measure(copy).naturalHeight+1);
    if (auto *o=m_document.slides[m_currentSlide].find(id)) {
      Arrange::setRect(*o,source->rect,QRectF(source->rect.topLeft(),QSizeF(source->rect.width(),height)));
      o->textFit=0; Design::markOverride(*o,"textFit");
    }
  }
  m_history.commit(); touch();
}

void Backend::commitTextDocument(const QString &slideId,const QString &id,QQuickTextDocument *editor) {
  if (!editor || !editor->textDocument()) return;
  int index=-1;
  for(int i=0;i<m_document.slides.size();++i) if(m_document.slides.at(i).id==slideId) index=i;
  if(index<0) return;
  const auto *source=m_document.slides.at(index).find(id);
  if(!source || source->type!=ObjectType::Text) return;
  const auto resolved=Design::resolve(m_document,index);
  const auto *authored=resolved.find(id); if(!authored) return;
  QStringList paragraphs;
  for(auto block=editor->textDocument()->begin();block.isValid();block=block.next()) {
    QString prefix;
    if(authored->listStyle && block.textList()) prefix=QString(qMax(0,block.textList()->format().indent()-1),'\t');
    paragraphs.append(prefix+QString(block.text()).replace(QChar(0x2028),QChar('\n')));
  }
  auto text=paragraphs.join('\n');
  // The marks people mean, settled when the words are, not under the cursor.
  if(m_document.smartPunctuation && authored->textKind!=1) text=Punctuation::smarten(text);
  if(text==authored->text) return;
  m_history.begin(m_document,tr("Edit text"));
  auto *object=m_document.slides[index].find(id);
  object->runs=TextRuns::afterEdit(object->runs,object->text,text);
  object->text=text; Design::markOverride(*object,"text");
  m_history.commit(); touch();
}

QRectF Backend::selectionVisualBounds() const {
  return Arrange::visualBounds(Design::resolve(m_document,m_currentSlide),selectedIds());
}
void Backend::rotateSelection(qreal degrees,bool snap) {
  if(!m_gestureActive || !std::isfinite(degrees)) return;
  const auto ids=selectedIds();
  if(ids.isEmpty()) return;
  if(snap) {
    const qreal base=ids.size()==1?m_gestureBasis.find(ids.first())->rotation:0;
    degrees=std::round((degrees+base)/15)*15-base;
  }
  const auto pivot=m_gestureBounds.center();
  QTransform t; t.translate(pivot.x(),pivot.y()); t.rotate(degrees); t.translate(-pivot.x(),-pivot.y());
  for(const auto &id:ids) if(const auto *before=m_gestureBasis.find(id)) if(auto *o=m_document.slides[m_currentSlide].find(id)) {
    auto rect=before->rect; rect.moveCenter(t.map(rect.center()));
    Arrange::setRect(*o,before->rect,rect);
    o->rotation=std::remainder(before->rotation+degrees,360.0); Design::markOverride(*o,"rotation");
  }
  touch();
}
void Backend::resizeSelectionHandle(qreal hx,qreal hy,qreal dx,qreal dy) {
  if(!m_gestureActive || selectedIds().size()!=1 || !std::isfinite(dx) || !std::isfinite(dy)) return;
  const auto *before=m_gestureBasis.find(selectedIds().first()); if(!before) return;
  QTransform rotation; rotation.rotate(before->rotation);
  const auto local=rotation.inverted().map(QPointF(dx,dy));
  const qreal width=hx==.5?before->rect.width():qMax(8.0,before->rect.width()+local.x()*(hx==0?-1:1));
  const qreal height=hy==.5?before->rect.height():qMax(8.0,before->rect.height()+local.y()*(hy==0?-1:1));
  const auto centre=before->rect.center()+rotation.map(QPointF((width-before->rect.width())*(hx-.5),(height-before->rect.height())*(hy-.5)));
  if(auto *o=m_document.slides[m_currentSlide].find(before->id)) Arrange::setRect(*o,before->rect,QRectF(centre-QPointF(width/2,height/2),QSizeF(width,height)));
  touch();
}

// --- character formatting over a stretch of text -----------------------------
QVariantMap Backend::textSelection() const {
  const auto *object = selectedObject();
  const bool active = object && object->type == ObjectType::Text &&
                      m_textSelectionEnd > m_textSelectionStart;
  QVariantMap values{{"active", active},
                     {"start", m_textSelectionStart},
                     {"end", m_textSelectionEnd}};
  if (!active) return values;
  // What the whole stretch looks like, and where it disagrees with itself.
  const int start = qBound(0, m_textSelectionStart, int(object->text.size()));
  const int end = qBound(start, m_textSelectionEnd, int(object->text.size()));
  int weight = object->fontWeight;
  bool italic = object->italic, underline = object->underline, strike = false;
  int baseline = 0;
  qreal size = object->fontSize;
  QString family = object->fontFamily;
  QColor colour = object->textColor;
  QString language = object->language;
  QStringList mixed;
  bool first = true;
  for (int i = start; i < end; ++i) {
    const auto *run = TextRuns::at(object->runs, i);
    const int thisWeight = run && run->weight ? run->weight : object->fontWeight;
    const bool thisItalic = run && run->italic ? run->italic == 1 : object->italic;
    const bool thisUnderline = run && run->underline ? run->underline == 1 : object->underline;
    const bool thisStrike = run && run->strike == 1;
    const int thisBaseline = run ? run->baseline : 0;
    const qreal thisSize = run && run->fontSize > 0 ? run->fontSize : object->fontSize;
    const QString thisFamily = run && !run->fontFamily.isEmpty() ? run->fontFamily
                                                                 : object->fontFamily;
    const QColor thisColour = run && run->color.isValid() ? run->color : object->textColor;
    const QString thisLanguage = run && !run->language.isEmpty() ? run->language
                                                                 : object->language;
    if (first) {
      weight = thisWeight; italic = thisItalic; underline = thisUnderline;
      strike = thisStrike; baseline = thisBaseline; size = thisSize;
      family = thisFamily; colour = thisColour; language = thisLanguage;
      first = false;
      continue;
    }
    const auto disagree = [&mixed](const char *name) {
      if (!mixed.contains(QLatin1String(name))) mixed.append(QLatin1String(name));
    };
    if (thisWeight != weight) disagree("weight");
    if (thisItalic != italic) disagree("italic");
    if (thisUnderline != underline) disagree("underline");
    if (thisStrike != strike) disagree("strike");
    if (thisBaseline != baseline) disagree("baseline");
    if (!qFuzzyCompare(thisSize + 1, size + 1)) disagree("fontSize");
    if (thisFamily != family) disagree("fontFamily");
    if (thisColour != colour) disagree("color");
    if (thisLanguage != language) disagree("language");
  }
  values["weight"] = weight;
  values["italic"] = italic;
  values["underline"] = underline;
  values["strike"] = strike;
  values["baseline"] = baseline;
  values["fontSize"] = size;
  values["fontFamily"] = family;
  values["color"] = colour.name(QColor::HexArgb);
  values["language"] = language;
  values["mixed"] = mixed;
  return values;
}

void Backend::setTextSelection(int start, int end) {
  const int from = qMin(start, end), to = qMax(start, end);
  if (from == m_textSelectionStart && to == m_textSelectionEnd) return;
  m_textSelectionStart = from;
  m_textSelectionEnd = to;
  emit textSelectionChanged();
}

bool Backend::formatSelection(const QString &key, const QVariant &value) {
  const auto *object = static_cast<const Backend *>(this)->selectedObject();
  if (!object || object->type != ObjectType::Text ||
      m_textSelectionEnd <= m_textSelectionStart)
    return false;
  SceneObject changed = *object;
  if (!TextRuns::apply(changed, m_textSelectionStart, m_textSelectionEnd, key, value))
    return false;
  if (changed.runs == object->runs) return true;   // valid, nothing to record
  m_history.begin(m_document, tr("Format text"));
  auto *target = selectedObject();
  target->runs = changed.runs;
  Design::markOverride(*target, QStringLiteral("runs"));
  m_history.commit();
  touch();
  emit textSelectionChanged();
  emit textFormattingChanged();
  return true;
}

// --- named text styles -------------------------------------------------------
QVariantList Backend::textStyles() const {
  QVariantList rows;
  for (const auto &style : m_document.textStyles) {
    int uses = 0;
    for (const auto &slide : m_document.slides)
      for (const auto &object : slide.objects)
        if (object.textStyleId == style.id) ++uses;
    auto look = Design::properties(Design::themed(m_document.theme, style.look));
    rows.append(QVariantMap{{"id", style.id},
                            {"name", style.name},
                            {"uses", uses},
                            {"fontFamily", look.value("fontFamily")},
                            {"fontSize", look.value("fontSize")},
                            {"textColor", look.value("textColor")}});
  }
  return rows;
}

QString Backend::addTextStyle(const QString &name) {
  const auto *source = static_cast<const Backend *>(this)->selectedObject();
  const auto trimmed = name.trimmed();
  if (!source || source->type != ObjectType::Text || trimmed.isEmpty() ||
      trimmed.size() > 120 || m_document.textStyles.size() >= 200)
    return {};
  TextStyle style;
  style.id = Edit::newId("textstyle");
  style.name = trimmed;
  // The style keeps the look as it is seen, so it carries the layout's values
  // as well as the box's own.
  const auto shown = Design::resolve(m_document, m_currentSlide);
  style.look = shown.find(source->id) ? *shown.find(source->id) : *source;
  style.look.text.clear();
  style.look.runs.clear();
  m_history.begin(m_document, tr("Keep a text style"));
  m_document.textStyles.append(style);
  // The box that made it follows it, so changing the style changes it too.
  for (auto &object : m_document.slides[m_currentSlide].objects)
    if (object.id == source->id) {
      object.textStyleId = style.id;
      for (const auto &key : Design::textStyleKeys()) object.overrides.removeAll(key);
    }
  m_history.commit();
  touch();
  return style.id;
}

bool Backend::applyTextStyle(const QString &id) {
  if (!id.isEmpty() && !Design::textStyle(m_document, id)) return false;
  const auto ids = selectedIds();
  if (ids.isEmpty()) return false;
  m_history.begin(m_document, id.isEmpty() ? tr("Unlink the text style")
                                           : tr("Use a text style"));
  bool changed = false;
  for (const auto &objectId : ids) {
    auto *object = m_document.slides[m_currentSlide].find(objectId);
    if (!object || object->type != ObjectType::Text) continue;
    if (object->textStyleId == id) continue;
    if (id.isEmpty()) {
      // Unlinking keeps what is on screen: the values are written in.
      const auto shown = Design::resolve(m_document, m_currentSlide);
      if (const auto *visible = shown.find(objectId)) {
        const auto values = Design::properties(*visible);
        for (const auto &key : Design::textStyleKeys()) {
          Design::setProperty(*object, key, values.value(key), false);
          // Written in as this box's own, so a layout cannot take it back.
          Design::markOverride(*object, key);
        }
      }
    } else {
      for (const auto &key : Design::textStyleKeys()) object->overrides.removeAll(key);
    }
    object->textStyleId = id;
    changed = true;
  }
  if (!changed) {
    m_history.abandon();
    return false;
  }
  m_history.commit();
  touch();
  return true;
}

bool Backend::updateTextStyleFromSelection() {
  const auto *source = static_cast<const Backend *>(this)->selectedObject();
  if (!source || source->type != ObjectType::Text || source->textStyleId.isEmpty()) return false;
  const auto shown = Design::resolve(m_document, m_currentSlide);
  const auto *visible = shown.find(source->id);
  if (!visible) return false;
  m_history.begin(m_document, tr("Update the text style"));
  for (auto &style : m_document.textStyles) {
    if (style.id != source->textStyleId) continue;
    const auto values = Design::properties(*visible);
    for (const auto &key : Design::textStyleKeys())
      Design::setProperty(style.look, key, values.value(key), false);
    style.look.textColorToken = visible->textColorToken;
    style.look.fontToken = visible->fontToken;
  }
  // What was a local difference is now what the style says.
  for (auto &object : m_document.slides[m_currentSlide].objects)
    if (object.id == source->id)
      for (const auto &key : Design::textStyleKeys()) object.overrides.removeAll(key);
  m_history.commit();
  touch();
  return true;
}

bool Backend::renameTextStyle(const QString &id, const QString &name) {
  const auto trimmed = name.trimmed();
  if (trimmed.isEmpty() || trimmed.size() > 120) return false;
  for (int i = 0; i < m_document.textStyles.size(); ++i) {
    if (m_document.textStyles.at(i).id != id) continue;
    if (m_document.textStyles.at(i).name == trimmed) return true;
    m_history.begin(m_document, tr("Rename a text style"));
    m_document.textStyles[i].name = trimmed;
    m_history.commit();
    touch();
    return true;
  }
  return false;
}

bool Backend::removeTextStyle(const QString &id) {
  if (!Design::textStyle(m_document, id)) return false;
  m_history.begin(m_document, tr("Remove a text style"));
  // Boxes that followed it keep the look they had.
  for (int i = 0; i < m_document.slides.size(); ++i) {
    const auto shown = Design::resolve(m_document, i);
    for (auto &object : m_document.slides[i].objects) {
      if (object.textStyleId != id) continue;
      if (const auto *visible = shown.find(object.id)) {
        const auto values = Design::properties(*visible);
        for (const auto &key : Design::textStyleKeys()) {
          Design::setProperty(object, key, values.value(key), false);
          Design::markOverride(object, key);
        }
      }
      object.textStyleId.clear();
    }
  }
  for (int i = 0; i < m_document.textStyles.size(); ++i)
    if (m_document.textStyles.at(i).id == id) {
      m_document.textStyles.removeAt(i);
      break;
    }
  m_history.commit();
  touch();
  return true;
}
