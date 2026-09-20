#include "backend.h"
#include "core/deckaudit.h"
#include "core/deckimport.h"
#include "core/design.h"
#include "filepicker.h"
#include "io/bundle.h"
#include <QFileInfo>
#include <QFontDatabase>
#include <QFutureWatcher>
#include <QtConcurrent>

void Backend::importDeckDialog() {
  m_pending = Pending::ImportDeck;
  m_chooser->openFile(tr("Import slides and design"), tr("OmaShow decks"),
                      {QStringLiteral("*.omashow")});
}

void Backend::importFromDeck(const QUrl &url) {
  if (!url.isLocalFile()) {
    emit failed(tr("Only local decks can be imported."));
    return;
  }
  if (!beginOperation(tr("Reading deck…")))
    return;
  const auto job = m_operationJob;
  const int generation = m_documentGeneration;
  auto *watcher = new QFutureWatcher<Bundle::ReadResult>(this);
  connect(watcher, &QFutureWatcherBase::finished, this,
          [this, watcher, job, url, generation] {
            const auto read = watcher->result();
            watcher->deleteLater();
            endOperation();
            if (job->canceled || generation != m_documentGeneration)
              return;
            if (!read.ok) {
              setStatus(read.error);
              emit failed(read.error);
              return;
            }
            if (read.document.slides.isEmpty()) {
              const auto message = tr("%1 has no slides to import.")
                                       .arg(displayNameFor(url));
              setStatus(message);
              emit failed(message);
              return;
            }
            m_importSource = read.document;
            m_importName = displayNameFor(url);
            m_importSlides.clear();
            for (int i = 0; i < m_importSource.slides.size(); ++i)
              m_importSlides.append(i);
            m_importOptions = QVariantMap{{"design", DeckImport::MatchByName},
                                         {"theme", false},
                                         {"dropMissingMedia", false},
                                         {"fonts", QVariantMap()}};
            refreshImport();
            emit importReady();
          });
  watcher->setFuture(QtConcurrent::run(Workers::io(), [url, job] {
    Bundle::ReadResult read;
    if (!job->canceled)
      read = Bundle::load(url.toLocalFile());
    return read;
  }));
}

void Backend::refreshImport() {
  if (m_importSource.slides.isEmpty()) {
    m_importPlan.clear();
    m_importDocument = Document();
    emit deckImportChanged();
    return;
  }
  auto options = m_importOptions;
  options["after"] = m_currentSlide;
  const auto result =
      DeckImport::build(m_document, m_importSource, m_importSlides, options);
  m_importDocument = result.document;
  m_importSlideIds = result.slideIds;
  m_importRevision = ++m_importPreviewSerial;
  m_importBaseRevision = m_revision;
  QVariantList sourceSlides;
  for (int i = 0; i < m_importSource.slides.size(); ++i) {
    const auto slide = Design::resolve(m_importSource, i);
    QString title;
    for (const auto &o : slide.objects)
      if (title.isEmpty() && o.type == ObjectType::Text &&
          !o.text.trimmed().isEmpty() && !o.id.startsWith(QStringLiteral("@field/")))
        title = o.text.section('\n', 0, 0);
    const auto *layout = Design::layout(m_importSource, m_importSource.slides.at(i).layoutId);
    sourceSlides.append(QVariantMap{
        {"index", i},
        {"title", title.isEmpty() ? tr("Untitled slide") : title},
        {"layout", layout ? layout->name : tr("Freeform")},
        {"skipped", m_importSource.slides.at(i).skipped},
        {"selected", m_importSlides.contains(i)}});
  }
  m_importPlan = result.plan;
  m_importPlan["source"] = m_importName;
  m_importPlan["sourceSlides"] = sourceSlides;
  m_importPlan["selectedCount"] = m_importSlides.size();
  m_importPlan["revision"] = m_importRevision;
  emit deckImportChanged();
}

QVariantMap Backend::deckImport() const { return m_importPlan; }

void Backend::setImportOption(const QString &key, const QVariant &value) {
  if (m_importSource.slides.isEmpty())
    return;
  if (key == QLatin1String("design")) {
    const int mode = value.toInt();
    if (mode < DeckImport::MatchByName || mode > DeckImport::KeepAppearance)
      return;
    m_importOptions["design"] = mode;
  } else if (key == QLatin1String("theme") || key == QLatin1String("dropMissingMedia")) {
    if (value.metaType().id() != QMetaType::Bool)
      return;
    m_importOptions[key] = value.toBool();
  } else if (key.startsWith(QLatin1String("font/"))) {
    const QString family = key.section('/', 1);
    const QString substitute = value.toString();
    if (family.isEmpty())
      return;
    auto fonts = m_importOptions.value("fonts").toMap();
    if (substitute.isEmpty())
      fonts.remove(family);
    else
      fonts[family] = substitute;
    m_importOptions["fonts"] = fonts;
  } else {
    return;
  }
  refreshImport();
}

void Backend::setImportSlideSelected(int index, bool selected) {
  if (index < 0 || index >= m_importSource.slides.size())
    return;
  if (selected == m_importSlides.contains(index))
    return;
  if (selected)
    m_importSlides.append(index);
  else
    m_importSlides.removeAll(index);
  std::sort(m_importSlides.begin(), m_importSlides.end());
  refreshImport();
}

void Backend::setImportSlidesSelected(bool selected) {
  m_importSlides.clear();
  if (selected)
    for (int i = 0; i < m_importSource.slides.size(); ++i)
      m_importSlides.append(i);
  refreshImport();
}

bool Backend::applyImport() {
  // What lands is the document the preview was rendered from — not a second
  // build of it, which would hand every imported object a different identity.
  if (m_importSource.slides.isEmpty() || m_importBaseRevision != m_revision ||
      !m_importPlan.value("ok").toBool() || m_importSlideIds.isEmpty())
    return false;
  const auto imported = m_importSlideIds;
  const auto name = m_importName;
  m_history.begin(m_document, tr("Import slides"));
  m_document = m_importDocument;
  m_history.commit();
  m_slideSelection = imported;
  m_slideAnchor = imported.first();
  m_groupScope.clear();
  clearSelection();
  restoreCurrentSlide(imported.first());
  setStatus(imported.size() == 1
                ? tr("Imported one slide from %1").arg(name)
                : tr("Imported %1 slides from %2").arg(imported.size()).arg(name));
  touch();
  clearImport();
  return true;
}

void Backend::clearImport() {
  m_importSource = Document();
  m_importDocument = Document();
  m_importSlides.clear();
  m_importSlideIds.clear();
  m_importPlan.clear();
  m_importOptions.clear();
  m_importName.clear();
  m_importBaseRevision = -1;
  emit deckImportChanged();
}

QVariantMap Backend::designAudit() const { return DeckAudit::report(m_document); }

bool Backend::removeUnusedDesign(const QStringList &ids) {
  auto document = m_document;
  if (!DeckAudit::remove(document, ids)) {
    setStatus(tr("That list is out of date — check the audit again."));
    return false;
  }
  m_history.begin(m_document, ids.size() == 1 ? tr("Remove unused design")
                                             : tr("Remove %1 unused items").arg(ids.size()));
  m_document = document;
  m_history.commit();
  touch();
  return true;
}
