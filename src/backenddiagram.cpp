#include "backend.h"
#include "core/design.h"
#include "core/diagram.h"
void Backend::previewDiagram(int kind, const QString &outline, bool vertical) {
  const auto result = Diagram::build(m_document.size, m_document.theme, kind,
                                     outline, vertical);
  m_diagramObjects = result.objects;
  m_diagramBaseRevision = m_revision;
  m_diagramSlide =
      m_currentSlide >= 0 && m_currentSlide < m_document.slides.size()
          ? m_document.slides[m_currentSlide].id
          : QString();
  m_diagramScope = m_groupScope;
  m_diagramPreview = {{"ok", result.ok() && !m_diagramSlide.isEmpty()},
                      {"error", result.error},
                      {"warnings", result.warnings},
                      {"nodes", result.nodes},
                      {"levels", result.levels},
                      {"revision", ++m_diagramRevision}};
  emit diagramPreviewChanged();
}
void Backend::clearDiagramPreview() {
  m_diagramObjects.clear();
  m_diagramPreview.clear();
  m_diagramBaseRevision = -1;
  emit diagramPreviewChanged();
}
QVector<SceneObject> Backend::diagramObjects() const {
  auto result = m_diagramObjects;
  for (auto &object : result)
    object = Design::themed(m_document.theme, object);
  return result;
}
bool Backend::insertDiagram() {
  if (!m_diagramPreview.value("ok").toBool() ||
      m_diagramBaseRevision != m_revision || m_diagramScope != m_groupScope ||
      m_currentSlide < 0 || m_currentSlide >= m_document.slides.size() ||
      m_document.slides[m_currentSlide].id != m_diagramSlide)
    return false;
  auto objects = m_diagramObjects;
  QStringList ids;
  for (auto &object : objects) {
    object.groups = m_groupScope + object.groups;
    ids.append(object.id);
  }
  if (objects.isEmpty())
    return false;
  m_history.begin(m_document, tr("Insert diagram"));
  m_document.slides[m_currentSlide].objects += objects;
  m_history.commit();
  m_selectedIds = ids;
  m_selectedId = ids.last();
  clearDiagramPreview();
  touch();
  return true;
}
