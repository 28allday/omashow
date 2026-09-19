#include "backend.h"
#include "core/design.h"
#include "core/link.h"
QString Backend::setObjectLink(int kind, const QString &target) {
  const auto ids = selectedIds();
  if (ids.isEmpty())
    return tr("Select an object first.");
  const auto error = Links::validate(kind, target, m_document);
  if (!error.isEmpty())
    return error;
  const auto normalized = Links::normalized(kind, target);
  m_history.begin(m_document, tr("Change link or action"));
  for (const auto &id : ids)
    if (auto *o = m_document.slides[m_currentSlide].find(id)) {
      o->linkKind = kind;
      o->linkTarget = normalized;
      Design::markOverride(*o, "linkKind");
      Design::markOverride(*o, "linkTarget");
    }
  m_history.commit();
  touch();
  return {};
}
