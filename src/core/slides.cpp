#include "core/slides.h"
#include "core/edit.h"
#include <QSet>
QVector<int> Slides::indices(const Document &d, const QStringList &ids) {
  QVector<int> result;
  for (int i = 0; i < d.slides.size(); ++i)
    if (ids.contains(d.slides.at(i).id))
      result.append(i);
  return result;
}
QStringList Slides::duplicate(Document &d, const QStringList &ids) {
  const auto chosen = indices(d, ids);
  if (chosen.isEmpty())
    return {};
  QVector<Slide> copies;
  QStringList result;
  for (int index : chosen) {
    auto slide = d.slides.at(index);
    slide.id = Edit::newId("slide");
    copies.append(slide);
    result.append(slide.id);
  }
  int at = chosen.last() + 1;
  for (const auto &slide : copies)
    d.slides.insert(at++, slide);
  return result;
}
bool Slides::remove(Document &d, const QStringList &ids) {
  const auto chosen = indices(d, ids);
  if (chosen.isEmpty() || chosen.size() == d.slides.size())
    return false;
  for (int i = chosen.size() - 1; i >= 0; --i)
    d.slides.removeAt(chosen.at(i));
  return true;
}
bool Slides::move(Document &d, const QStringList &ids, int boundary,
                  const QString &destinationSection) {
  const auto chosen = indices(d, ids);
  if (chosen.isEmpty())
    return false;
  boundary = qBound(0, boundary, int(d.slides.size()));
  QVector<Slide> moving, remaining;
  QSet<QString> sections;
  int insertion = 0;
  for (int i = 0; i < d.slides.size(); ++i) {
    const auto &s = d.slides.at(i);
    if (ids.contains(s.id)) {
      moving.append(s);
      sections.insert(s.sectionId);
    } else {
      remaining.append(s);
      if (i < boundary)
        ++insertion;
    }
  }
  auto reordered = remaining;
  int at = insertion;
  for (const auto &s : moving)
    reordered.insert(at++, s);
  bool changed = false;
  for (int i = 0; i < d.slides.size(); ++i)
    if (d.slides.at(i).id != reordered.at(i).id)
      changed = true;
  if (!changed)
    return false;
  // A contiguous section remains intact when multiple sections move together.
  // A selection within one section adopts its new neighbours' section.
  const QString destination = destinationSection;
  bool wholeSection = !sections.isEmpty() && !sections.contains(QString());
  for (const auto &s : remaining)
    if (sections.contains(s.sectionId))
      wholeSection = false;
  if (sections.size() == 1 && !wholeSection)
    for (int i = insertion; i < insertion + moving.size(); ++i)
      reordered[i].sectionId = destination;
  d.slides = reordered;
  return true;
}
bool Slides::nudge(Document &d, const QStringList &ids, int direction) {
  if (direction == 0)
    return false;
  QSet<QString> sections;
  for (const auto &s : d.slides)
    if (ids.contains(s.id))
      sections.insert(s.sectionId);
  bool wholeSection = !sections.contains(QString());
  for (const auto &s : d.slides)
    if (sections.contains(s.sectionId) && !ids.contains(s.id))
      wholeSection = false;
  const bool adopt = sections.size() == 1 && !wholeSection;
  bool changed = false;
  if (direction < 0) {
    for (int i = 1; i < d.slides.size(); ++i)
      if (ids.contains(d.slides.at(i).id) &&
          !ids.contains(d.slides.at(i - 1).id)) {
        if (adopt)
          d.slides[i].sectionId = d.slides.at(i - 1).sectionId;
        d.slides.swapItemsAt(i, i - 1);
        changed = true;
      }
  } else {
    for (int i = d.slides.size() - 2; i >= 0; --i)
      if (ids.contains(d.slides.at(i).id) &&
          !ids.contains(d.slides.at(i + 1).id)) {
        if (adopt)
          d.slides[i].sectionId = d.slides.at(i + 1).sectionId;
        d.slides.swapItemsAt(i, i + 1);
        changed = true;
      }
  }
  return changed;
}

int Slides::sectionBoundary(const Document &d, const QString &id,
                            int direction) {
  if (id.isEmpty() || direction == 0)
    return -1;
  struct Block {
    QString id;
    int first, last;
  };
  QVector<Block> blocks;
  for (int i = 0; i < d.slides.size(); ++i) {
    const auto section = d.slides.at(i).sectionId;
    int existing = -1;
    if (!section.isEmpty()) {
      for (int j = 0; j < blocks.size(); ++j)
        if (blocks.at(j).id == section) {
          existing = j;
          break;
        }
    } else if (i > 0 && d.slides.at(i - 1).sectionId.isEmpty())
      existing = blocks.size() - 1;
    if (existing < 0)
      blocks.append({section, i, i});
    else
      blocks[existing].last = i;
  }
  for (int i = 0; i < blocks.size(); ++i)
    if (blocks.at(i).id == id) {
      const int target = i + (direction < 0 ? -1 : 1);
      if (target < 0 || target >= blocks.size())
        return -1;
      return direction < 0 ? blocks.at(target).first
                           : blocks.at(target).last + 1;
    }
  return -1;
}
bool Slides::moveSection(Document &d, const QString &id, int direction) {
  const int boundary = sectionBoundary(d, id, direction);
  if (boundary < 0)
    return false;
  QStringList ids;
  for (const auto &slide : d.slides)
    if (slide.sectionId == id)
      ids.append(slide.id);
  if (!move(d, ids, boundary, id))
    return false;
  // Keep the definition list in the same order readers see in the deck.
  QVector<Section> ordered;
  QStringList seen;
  for (const auto &slide : d.slides)
    if (!seen.contains(slide.sectionId)) {
      seen.append(slide.sectionId);
      for (const auto &section : d.sections)
        if (section.id == slide.sectionId)
          ordered.append(section);
    }
  for (const auto &section : d.sections)
    if (!seen.contains(section.id))
      ordered.append(section);
  d.sections = ordered;
  return true;
}
