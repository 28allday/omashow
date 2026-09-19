#include "backend.h"
#include "core/starter.h"
#include "io/recents.h"
#include "io/recovery.h"
#include <QDesktopServices>
#include <QFileInfo>
#include <QStandardPaths>
void Backend::activateDocument() {
    m_hasDocument = true;
    m_startVisible = false;
    emit startChanged();
}
void Backend::showStart() {
    pause();
    m_startVisible = true;
    emit startChanged();
    emit recentsChanged();
}
void Backend::resumeDeck() {
    if (m_hasDocument) {
        m_startVisible = false;
        emit startChanged();
    }
}
bool Backend::createDeck(int theme, qreal width, qreal height, int layout) {
    const auto document = Starter::create(theme, QSizeF(width, height), layout);
    if (document.slides.isEmpty()) {
        emit failed(tr("Choose dimensions between 240 and 10,000, and a valid template."));
        return false;
    }
    m_autosave.stop();
    Recovery::discard();
    setDocument(document);
    setFileUrl(QUrl());
    setStatus(tr("New deck"));
    return true;
}
QVariantList Backend::recentFiles() const { return Recents::read(Recents::settingsPath()); }
void Backend::rememberRecent(const QString &path) {
    if (!QStandardPaths::isTestModeEnabled())
        Recents::remember(Recents::settingsPath(), path);
    emit recentsChanged();
}
void Backend::pinRecent(const QString &path, bool pinned) {
    Recents::pin(Recents::settingsPath(), path, pinned);
    emit recentsChanged();
}
void Backend::removeRecent(const QString &path) {
    Recents::remove(Recents::settingsPath(), path);
    emit recentsChanged();
}
void Backend::revealRecent(const QString &path) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}
