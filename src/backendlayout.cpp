#include "backend.h"
#include "core/layoutapply.h"
#include <QFutureWatcher>
#include <QtConcurrent>

void Backend::previewLayoutAsync(const QString &id, int scope, const QVariantMap &mapping, int geometry) {
    const auto request = ++m_layoutRequest;
    m_layoutPreview["ok"] = false;
    m_layoutPreview["busy"] = true;
    m_layoutPreview["error"] = tr("Preparing preview…");
    emit layoutPreviewChanged();
    if (m_layoutRunning) {
        m_nextLayoutPreview = [this, id, scope, mapping, geometry] { previewLayoutAsync(id, scope, mapping, geometry); };
        return;
    }
    m_layoutRunning = true;
    m_layoutScope = scope;
    m_layoutSlideIds = layoutSlideIds(scope);
    const auto ids = m_layoutSlideIds;
    const auto document = m_document;
    const auto revision = m_revision;
    auto *watcher = new QFutureWatcher<LayoutApply::Result>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, request, revision] {
        m_layoutRunning = false;
        if (request == m_layoutRequest) {
            const auto result = watcher->result();
            m_layoutPreviewDocument = result.document;
            m_layoutBaseRevision = revision;
            m_layoutPreview = {{"ok", result.ok()}, {"error", result.error}, {"busy", false},
                               {"slides", result.slides}, {"mappings", result.mappings},
                               {"revision", ++m_layoutPreviewRevision}};
            emit layoutPreviewChanged();
        }
        watcher->deleteLater();
        if (m_nextLayoutPreview) { auto next = std::exchange(m_nextLayoutPreview, {}); next(); }
    });
    watcher->setFuture(QtConcurrent::run(Workers::preview(), [document, id, ids, mapping, geometry] {
        return LayoutApply::preview(document, id, ids, mapping, geometry);
    }));
}

QStringList Backend::layoutSlideIds(int scope) const {
    if (scope == 1) return selectedSlides();
    QStringList ids;
    if (scope == 2) {
        for (const auto &slide : m_document.slides) ids.append(slide.id);
    } else if (scope == 0 && m_currentSlide >= 0 && m_currentSlide < m_document.slides.size()) {
        ids.append(m_document.slides[m_currentSlide].id);
    }
    return ids;
}

QVariantMap Backend::layoutPreview() const {
    auto preview = m_layoutPreview;
    if (!preview.isEmpty() && !preview.value("busy").toBool() && (m_layoutBaseRevision != m_revision ||
                              m_layoutSlideIds != layoutSlideIds(m_layoutScope))) {
        preview["ok"] = false;
        preview["error"] = tr("The deck or slide selection changed. Refresh the preview before applying.");
    }
    return preview;
}

void Backend::previewLayout(const QString &id, int scope, const QVariantMap &mapping, int geometry) {
    ++m_layoutRequest; m_nextLayoutPreview = {};
    m_layoutScope = scope;
    m_layoutSlideIds = layoutSlideIds(scope);
    const auto result = LayoutApply::preview(m_document, id, m_layoutSlideIds, mapping, geometry);
    m_layoutPreviewDocument = result.document;
    m_layoutBaseRevision = m_revision;
    m_layoutPreview = {{"ok", result.ok()}, {"error", result.error},
                       {"slides", result.slides}, {"mappings", result.mappings},
                       {"revision", ++m_layoutPreviewRevision}};
    emit layoutPreviewChanged();
}

void Backend::clearLayoutPreview() {
    ++m_layoutRequest; m_nextLayoutPreview = {};
    m_layoutPreview.clear();
    m_layoutPreviewDocument = {};
    m_layoutSlideIds.clear();
    m_layoutBaseRevision = -1;
    emit layoutPreviewChanged();
}

bool Backend::applyLayoutPreview() {
    if (!layoutPreview().value("ok").toBool()) return false;
    m_history.begin(m_document, tr("Apply layout to %1 slides").arg(m_layoutSlideIds.size()));
    m_document = m_layoutPreviewDocument;
    m_history.commit();
    clearLayoutPreview();
    touch();
    return true;
}

void Backend::setMasterArtworkVisible(bool visible) {
    if (m_currentSlide < 0 || m_currentSlide >= m_document.slides.size() ||
        m_document.slides[m_currentSlide].showMasterObjects == visible) return;
    m_history.begin(m_document, tr("Change master artwork visibility"));
    m_document.slides[m_currentSlide].showMasterObjects = visible;
    m_history.commit();
    touch();
}

void Backend::setMasterFieldsVisible(bool visible) {
    if (m_currentSlide < 0 || m_currentSlide >= m_document.slides.size() ||
        m_document.slides[m_currentSlide].showMasterFields == visible) return;
    m_history.begin(m_document, tr("Change master field visibility"));
    m_document.slides[m_currentSlide].showMasterFields = visible;
    m_history.commit();
    touch();
}
