#include "anim/presentation.h"
#include "backend.h"
#include "core/edit.h"
#include "core/deckresize.h"
#include "core/design.h"
#include "core/slides.h"
QStringList Backend::selectedSlides() const {
  QStringList ids;
  for (const auto &s : m_document.slides)
    if (m_slideSelection.contains(s.id))
      ids.append(s.id);
  if (ids.isEmpty() && !m_document.slides.isEmpty())
    ids.append(
        m_document.slides
            .at(qBound(0, m_currentSlide, int(m_document.slides.size() - 1)))
            .id);
  return ids;
}
void Backend::resetSlideSelection() {
  m_slideSelection.clear();
  m_slideAnchor.clear();
  emit slideSelectionChanged();
}
void Backend::restoreCurrentSlide(const QString &id) {
  for (int i = 0; i < m_document.slides.size(); ++i)
    if (m_document.slides.at(i).id == id) {
      m_currentSlide = i;
      break;
    }
  m_currentSlide = qBound(0, m_currentSlide, int(m_document.slides.size() - 1));
  emit currentSlideChanged();
  emit slideSelectionChanged();
}
void Backend::selectSlide(int index, bool toggle, bool range) {
  if (index < 0 || index >= m_document.slides.size())
    return;
  const auto id = m_document.slides.at(index).id;
  auto ids = selectedSlides();
  if (range) {
    int anchor = m_currentSlide;
    for (int i = 0; i < m_document.slides.size(); ++i)
      if (m_document.slides.at(i).id == m_slideAnchor)
        anchor = i;
    if (!toggle)
      ids.clear();
    for (int i = qMin(anchor, index); i <= qMax(anchor, index); ++i)
      if (!ids.contains(m_document.slides.at(i).id))
        ids.append(m_document.slides.at(i).id);
  } else {
    m_slideAnchor = id;
    if (toggle) {
      if (ids.contains(id)) {
        if (ids.size() > 1)
          ids.removeAll(id);
      } else
        ids.append(id);
    } else
      ids = {id};
  }
  m_slideSelection = ids;
  m_currentSlide = index;
  if (!ids.contains(id))
    restoreCurrentSlide(ids.last());
  m_groupScope.clear();
  clearSelection();
  emit currentSlideChanged();
  emit slideSelectionChanged();
}
void Backend::selectAllSlides() {
  m_slideSelection.clear();
  for (const auto &s : m_document.slides)
    m_slideSelection.append(s.id);
  if (m_slideAnchor.isEmpty() && !m_document.slides.isEmpty())
    m_slideAnchor = m_document.slides.at(m_currentSlide).id;
  clearSelection();
  emit slideSelectionChanged();
}
void Backend::duplicateSelectedSlides() {
  const auto ids = selectedSlides();
  m_history.begin(m_document, tr("Duplicate slides"));
  const auto copies = Slides::duplicate(m_document, ids);
  if (copies.isEmpty()) {
    m_history.abandon();
    return;
  }
  m_history.commit();
  m_slideSelection = copies;
  m_slideAnchor = copies.first();
  m_groupScope.clear();
  clearSelection();
  restoreCurrentSlide(copies.first());
  touch();
}
void Backend::deleteSelectedSlides() {
  const auto ids = selectedSlides();
  const auto current = m_document.slides.value(m_currentSlide).id;
  m_history.begin(m_document, tr("Delete slides"));
  if (!Slides::remove(m_document, ids)) {
    m_history.abandon();
    setStatus(tr("A deck keeps at least one slide."));
    return;
  }
  m_history.commit();
  resetSlideSelection();
  m_groupScope.clear();
  clearSelection();
  restoreCurrentSlide(current);
  touch();
}
void Backend::moveSelectedSlides(int target, bool after) {
  if (target < 0 || target >= m_document.slides.size())
    return;
  const auto current = m_document.slides.value(m_currentSlide).id;
  const auto ids = selectedSlides();
  m_history.begin(m_document, tr("Move slides"));
  if (!Slides::move(m_document, ids, target + (after ? 1 : 0),
                    m_document.slides.at(target).sectionId)) {
    m_history.abandon();
    return;
  }
  m_history.commit();
  restoreCurrentSlide(current);
  touch();
}
void Backend::nudgeSelectedSlides(int direction) {
  const auto current = m_document.slides.value(m_currentSlide).id;
  const auto ids = selectedSlides();
  m_history.begin(m_document, tr("Move slides"));
  if (!Slides::nudge(m_document, ids, direction)) {
    m_history.abandon();
    return;
  }
  m_history.commit();
  restoreCurrentSlide(current);
  touch();
}
void Backend::setSlidesSkipped(bool skipped) {
  const auto ids = selectedSlides();
  bool changed = false;
  for (const auto &s : m_document.slides)
    if (ids.contains(s.id) && s.skipped != skipped)
      changed = true;
  if (!changed)
    return;
  m_history.begin(m_document,
                  skipped ? tr("Skip slides") : tr("Include slides"));
  for (auto &s : m_document.slides)
    if (ids.contains(s.id))
      s.skipped = skipped;
  m_history.commit();
  touch();
}
QVariantMap Backend::slideSelectionState() const {
  const auto ids = selectedSlides();
  int skipped = 0;
  for (const auto &s : m_document.slides)
    if (ids.contains(s.id) && s.skipped)
      ++skipped;
  return {{"count", ids.size()},
          {"allSkipped", skipped == ids.size()},
          {"canDelete", ids.size() < m_document.slides.size()}};
}

bool Backend::resizeDeck(qreal width, qreal height, bool scaleContent) {
  m_history.begin(m_document, tr("Change slide size"));
  if (!DeckResize::apply(m_document, QSizeF(width, height), scaleContent)) {
    m_history.abandon();
    return false;
  }
  m_history.commit();
  touch();
  return true;
}

QStringList Backend::collapsedSections() const {
  QStringList ids;
  for (const auto &section : m_document.sections)
    if (m_collapsedSections.contains(section.id))
      ids.append(section.id);
  return ids;
}
QVariantMap Backend::sectionInfo(const QString &id) const {
  if (id.isEmpty())
    return {};
  QString name;
  bool exists = false;
  for (const auto &section : m_document.sections)
    if (section.id == id) {
      name = section.name;
      exists = true;
      break;
    }
  if (!exists)
    return {};
  int first = -1, last = -1, count = 0;
  for (int i = 0; i < m_document.slides.size(); ++i)
    if (m_document.slides.at(i).sectionId == id) {
      if (first < 0)
        first = i;
      last = i;
      ++count;
    }
  return {{"id", id},
          {"name", name},
          {"count", count},
          {"first", first},
          {"last", last},
          {"collapsed", m_collapsedSections.contains(id)},
          {"canMoveUp", Slides::sectionBoundary(m_document, id, -1) >= 0},
          {"canMoveDown", Slides::sectionBoundary(m_document, id, 1) >= 0}};
}
QVariantList Backend::browserSlides() const {
  QVariantList result;
  QStringList seen;
  QMap<QString, QVariantMap> sections;
  const auto rows = navigator();
  for (const auto &value : rows) {
    auto row = value.toMap();
    const auto id = row.value("sectionId").toString();
    if (!sections.contains(id))
      sections.insert(id, sectionInfo(id));
    const auto info = sections.value(id);
    const bool collapsed = info.value("collapsed").toBool();
    if (collapsed && seen.contains(id))
      continue;
    seen.append(id);
    row["collapsed"] = collapsed;
    row["sectionCount"] = info.value("count", 0);
    row["sectionFirst"] = info.value("first", row.value("index"));
    row["sectionLast"] = info.value("last", row.value("index"));
    // The summary's bounds cover the whole section, including any split runs.
    if (collapsed)
      row["sectionStart"] = true;
    result.append(row);
  }
  return result;
}
void Backend::setSectionCollapsed(const QString &id, bool collapsed) {
  if (sectionInfo(id).isEmpty() ||
      m_collapsedSections.contains(id) == collapsed)
    return;
  if (collapsed)
    m_collapsedSections.append(id);
  else
    m_collapsedSections.removeAll(id);
  emit browserChanged();
}
void Backend::collapseAllSections(bool collapsed) {
  QStringList ids;
  if (collapsed)
    for (const auto &section : m_document.sections)
      ids.append(section.id);
  if (ids == m_collapsedSections)
    return;
  m_collapsedSections = ids;
  emit browserChanged();
}
void Backend::selectSection(const QString &id, bool toggle, bool range) {
  const auto info = sectionInfo(id);
  if (info.value("count").toInt() == 0)
    return;
  const int first = info.value("first").toInt(),
            last = info.value("last").toInt();
  if (range) {
    selectSlide(m_currentSlide > first ? first : last, toggle, true);
    return;
  }
  auto ids = toggle ? selectedSlides() : QStringList();
  QStringList sectionIds;
  for (const auto &slide : m_document.slides)
    if (slide.sectionId == id)
      sectionIds.append(slide.id);
  bool all = true;
  for (const auto &slideId : sectionIds)
    if (!ids.contains(slideId))
      all = false;
  for (const auto &slideId : sectionIds) {
    if (toggle && all)
      ids.removeAll(slideId);
    else if (!ids.contains(slideId))
      ids.append(slideId);
  }
  if (ids.isEmpty())
    return;
  const auto current = m_document.slides.value(m_currentSlide).id;
  m_slideSelection = ids;
  m_slideAnchor = sectionIds.first();
  m_groupScope.clear();
  clearSelection();
  restoreCurrentSlide(ids.contains(current) ? current : ids.first());
}
void Backend::moveSection(const QString &id, int direction) {
  if (sectionInfo(id).isEmpty())
    return;
  const auto current = m_document.slides.value(m_currentSlide).id;
  m_history.begin(m_document, tr("Move section"));
  if (!Slides::moveSection(m_document, id, direction)) {
    m_history.abandon();
    return;
  }
  m_history.commit();
  restoreCurrentSlide(current);
  touch();
}

// --- custom shows ------------------------------------------------------------
QVariantList Backend::customShows() const {
  QVariantList rows;
  for (const auto &show : m_document.shows) {
    QVariantList slides;
    for (const auto &id : show.slideIds)
      for (int i = 0; i < m_document.slides.size(); ++i)
        if (m_document.slides.at(i).id == id)
          slides.append(QVariantMap{{"id", id}, {"index", i}});
    rows.append(QVariantMap{{"id", show.id},
                            {"name", show.name},
                            {"slides", slides},
                            {"count", slides.size()},
                            {"active", show.id == m_activeShow}});
  }
  return rows;
}

QString Backend::addCustomShow(const QString &name) {
  const auto trimmed = name.trimmed();
  if (trimmed.isEmpty() || trimmed.size() > 120 || m_document.shows.size() >= 64) return {};
  CustomShow show;
  show.id = Edit::newId("show");
  show.name = trimmed;
  // A new show starts as whatever is selected, or the whole deck.
  const auto chosen = selectedSlides();
  for (const auto &slide : m_document.slides)
    if (chosen.isEmpty() || chosen.contains(slide.id)) show.slideIds.append(slide.id);
  m_history.begin(m_document, tr("Add a custom show"));
  m_document.shows.append(show);
  m_history.commit();
  touch();
  return show.id;
}

bool Backend::renameCustomShow(const QString &id, const QString &name) {
  const auto trimmed = name.trimmed();
  if (trimmed.isEmpty() || trimmed.size() > 120) return false;
  for (int i = 0; i < m_document.shows.size(); ++i) {
    if (m_document.shows.at(i).id != id) continue;
    if (m_document.shows.at(i).name == trimmed) return true;
    m_history.begin(m_document, tr("Rename a custom show"));
    m_document.shows[i].name = trimmed;
    m_history.commit();
    touch();
    return true;
  }
  return false;
}

bool Backend::removeCustomShow(const QString &id) {
  for (int i = 0; i < m_document.shows.size(); ++i) {
    if (m_document.shows.at(i).id != id) continue;
    m_history.begin(m_document, tr("Remove a custom show"));
    m_document.shows.removeAt(i);
    m_history.commit();
    if (m_activeShow == id) setActiveShow(QString());
    touch();
    return true;
  }
  return false;
}

bool Backend::setCustomShowSlides(const QString &id, const QStringList &slideIds) {
  QStringList kept;
  for (const auto &slide : slideIds) {
    bool exists = false;
    for (const auto &known : m_document.slides)
      if (known.id == slide) exists = true;
    if (!exists || kept.contains(slide)) return false;
    kept.append(slide);
  }
  for (int i = 0; i < m_document.shows.size(); ++i) {
    if (m_document.shows.at(i).id != id) continue;
    if (m_document.shows.at(i).slideIds == kept) return true;
    m_history.begin(m_document, tr("Change a custom show"));
    m_document.shows[i].slideIds = kept;
    m_history.commit();
    if (m_activeShow == id) applyActiveShow();
    touch();
    return true;
  }
  return false;
}

bool Backend::moveCustomShowSlide(const QString &id, int from, int to) {
  for (const auto &show : m_document.shows) {
    if (show.id != id) continue;
    if (from < 0 || from >= show.slideIds.size() || to < 0 || to >= show.slideIds.size() ||
        from == to)
      return false;
    auto slides = show.slideIds;
    slides.move(from, to);
    return setCustomShowSlides(id, slides);
  }
  return false;
}

// Which slides the show, the preview and an export are about. This is a view
// on the deck, not an edit of it, so it is never part of undo or of a file.
void Backend::setActiveShow(const QString &id) {
  if (m_activeShow == id) return;
  bool known = id.isEmpty();
  for (const auto &show : m_document.shows)
    if (show.id == id) known = true;
  if (!known) return;
  m_activeShow = id;
  applyActiveShow();
  pause();
  setTime(0);
  ++m_revision;
  emit documentChanged();
  emit deckChanged();
  emit selectionChanged();
}

void Backend::applyActiveShow() {
  QStringList order;
  for (const auto &show : m_document.shows)
    if (show.id == m_activeShow) order = show.slideIds;
  m_document.activeShow = order;
}
