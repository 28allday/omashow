// Free matches for Office fonts: what was swapped, what is worth fetching,
// and fetching it into OmaShow's own font folder once the person says yes
// (no packages, no password, nothing system-wide).
#include <QTimer>
#include "backend.h"
#include "core/fontdownloader.h"
#include "core/fontsandbox.h"
#include "core/fontswap.h"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace {

QString fontSettingsPath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/view.ini";
}

QString fontCheckKey(const QString &source) {
    return QStringLiteral("fontCheckHidden/") + QString::fromLatin1(QCryptographicHash::hash(QFileInfo(source).absoluteFilePath().toUtf8(), QCryptographicHash::Sha1).toHex());
}

} // namespace

QVariantList Backend::fontSwaps() const { return FontSwap::swapped(m_document); }
QVariantList Backend::fontsToInstall() const { return FontSwap::needed(m_document); }
QString Backend::fontSandboxDir() const { return FontSandbox::dir(); }

bool Backend::fontCheckHidden(const QString &source) const {
    if (source.isEmpty()) return false;
    QSettings settings(fontSettingsPath(), QSettings::IniFormat);
    return settings.value(fontCheckKey(source), false).toBool();
}
void Backend::setFontCheckHidden(const QString &source, bool hidden) {
    if (source.isEmpty()) return;
    QSettings settings(fontSettingsPath(), QSettings::IniFormat);
    if (hidden) settings.setValue(fontCheckKey(source), true); else settings.remove(fontCheckKey(source));
    settings.sync();
}
bool Backend::fontAutoDownload() const {
    QSettings settings(fontSettingsPath(), QSettings::IniFormat);
    return settings.value("fontAutoDownload", false).toBool();
}
void Backend::setFontAutoDownload(bool on) {
    if (on == fontAutoDownload()) return;
    QSettings settings(fontSettingsPath(), QSettings::IniFormat);
    settings.setValue("fontAutoDownload", on);
    settings.sync();
    emit fontAutoDownloadChanged();
}
bool Backend::fontCheckOnOpen() const {
    QSettings settings(fontSettingsPath(), QSettings::IniFormat);
    return settings.value("fontCheckOnOpen", true).toBool();
}
void Backend::setFontCheckOnOpen(bool on) {
    if (on == fontCheckOnOpen()) return;
    QSettings settings(fontSettingsPath(), QSettings::IniFormat);
    settings.setValue("fontCheckOnOpen", on);
    settings.sync();
    emit fontAutoDownloadChanged();
}

// What OmaShow itself downloaded (a listed family, complete).
QVariantList Backend::downloadedFonts() const {
    QVariantList rows;
    const QDir root(FontSandbox::dir());
    for (const auto &folder : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase)) {
        const QString family = FontDownloader::familyForFolder(folder);
        if (family.isEmpty() || !FontDownloader::complete(family)) continue;
        rows.append(QVariantMap{{QStringLiteral("family"), family}, {QStringLiteral("bytes"), double(FontDownloader::bytesFor({family}))}});
    }
    return rows;
}
void Backend::removeDownloadedFonts() {
    if (fontRemoveRunning()) return;
    setExtraFontsPack(false);   // or it would want them straight back
    if (m_fontDownloader) m_fontDownloader->cancel();
    const QDir root(FontSandbox::dir());
    for (const auto &folder : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        if (!FontDownloader::familyForFolder(folder).isEmpty()) m_fontRemoveLeft.append(root.filePath(folder));
    m_fontRemoveTotal = int(m_fontRemoveLeft.size());
    emit fontRemoveChanged();
    QTimer::singleShot(0, this, &Backend::removeNextFont);
}
void Backend::removeNextFont() {
    if (!m_fontRemoveLeft.isEmpty()) {
        QDir(m_fontRemoveLeft.takeFirst()).removeRecursively();
        emit fontRemoveChanged();
        if (!m_fontRemoveLeft.isEmpty()) { QTimer::singleShot(0, this, &Backend::removeNextFont); return; }
    }
    const int gone = m_fontRemoveTotal;
    m_packRowsBuilt = false;
    m_packStatus.clear();
    emit fontDownloadChanged();
    // Fonts already loaded stay usable until OmaShow is closed.
    setStatus(gone ? (gone == 1 ? tr("Removed 1 downloaded font; open presentations keep it until OmaShow is restarted") : tr("Removed %1 downloaded fonts; open presentations keep them until OmaShow is restarted").arg(gone))
                   : tr("No downloaded fonts to remove"));
    emit fontRemoveFinished(gone);
}

bool Backend::openFontSandbox() {
    const auto dir = FontSandbox::dir();
    // Made only when asked for.
    if (!QDir().mkpath(dir)) return false;
    return QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

bool Backend::extraFontsPack() const {
    QSettings settings(fontSettingsPath(), QSettings::IniFormat);
    return settings.value("extraFontsPack", false).toBool();
}
// On: the pack is wanted (the window offers the download). Off: nothing is
// deleted; a pack download still running is stopped (a deck's fonts go on).
void Backend::setExtraFontsPack(bool on) {
    if (on == extraFontsPack()) return;
    QSettings settings(fontSettingsPath(), QSettings::IniFormat);
    settings.setValue("extraFontsPack", on);
    settings.sync();
    if (!on) cancelPack();
    emit extraFontsPackChanged();
}
const QVariantList &Backend::packRows() const {
    if (!m_packRowsBuilt) {
        m_packRows = FontSwap::pack();
        m_packReadyCount = 0;
        for (const auto &r : std::as_const(m_packRows)) m_packReadyCount += r.toMap().value(QStringLiteral("installed")).toBool();
        m_packRowsBuilt = true;
    }
    return m_packRows;
}
QVariantList Backend::fontPack() const { m_packRowsBuilt = false; return packRows(); }   // fresh for the dialog
int Backend::packTotal() const { return packRows().size(); }
int Backend::packReady() const { packRows(); return m_packReadyCount; }
// Without the families that failed (the bar never runs backwards).
static QStringList without(const QStringList &families, const QVariantMap &failed) {
    QStringList out;
    for (const auto &f : families) if (!failed.contains(f)) out << f;
    return out;
}
double Backend::packDone() const { return m_fontDownloader ? double(m_fontDownloader->bytesDone(without(m_packFamilies, m_packFailed))) : 0; }
double Backend::packBytes() const {
    if (!m_packFamilies.isEmpty()) return double(FontDownloader::bytesFor(without(m_packFamilies, m_packFailed)));
    QStringList todo;   // idle: what a download would fetch now
    for (const auto &r : packRows())
        if (!r.toMap().value(QStringLiteral("installed")).toBool()) todo << r.toMap().value(QStringLiteral("family")).toString();
    return double(FontDownloader::bytesFor(todo));
}

bool Backend::fontDownloadRunning() const { return !m_deckLeft.isEmpty(); }
double Backend::fontDownloadDone() const { return m_fontDownloader ? double(m_fontDownloader->bytesDone(without(m_deckFamilies, m_deckFailed))) : 0; }
double Backend::fontDownloadTotal() const { return double(FontDownloader::bytesFor(without(m_deckFamilies, m_deckFailed))); }

void Backend::ensureDownloader() {
    if (m_fontDownloader) return;
    m_fontDownloader = new FontDownloader(this);
    connect(m_fontDownloader, &FontDownloader::progress, this, &Backend::fontDownloadChanged);
    connect(m_fontDownloader, &FontDownloader::familyReady, this, &Backend::familyArrived);
    connect(m_fontDownloader, &FontDownloader::familyFailed, this, &Backend::familyLost);
    connect(m_fontDownloader, &FontDownloader::finished, this, &Backend::runFinished);
}

// A deck's fonts: ahead of anything queued (a running pack included).
// A second deck opened mid-download joins the same deck group.
bool Backend::downloadFonts(const QStringList &families) {
    if (families.isEmpty()) return false;
    ensureDownloader();
    if (m_deckLeft.isEmpty()) { m_deckFamilies.clear(); m_deckInstalled.clear(); m_deckFailed.clear(); m_deckCancelled = false; }
    for (const auto &f : families) if (!m_deckFamilies.contains(f)) { m_deckFamilies << f; m_deckLeft.insert(f); m_deckFailed.remove(f); }
    for (const auto &f : families) m_deckNeeded.insert(f);
    m_fontDownloader->add(families, true);
    setStatus(tr("Downloading free fonts: %1…").arg(families.join(QStringLiteral(", "))));
    emit fontDownloadChanged();
    return true;
}
// The deck group ends now; families the pack also wants keep coming for it.
void Backend::cancelFontDownload() {
    if (!m_fontDownloader || m_deckLeft.isEmpty()) return;
    m_deckCancelled = true;
    QStringList mine;
    for (const auto &f : std::as_const(m_deckLeft)) if (!m_packLeft.contains(f)) mine.append(f);
    m_deckLeft.clear();
    if (m_packLeft.isEmpty()) m_fontDownloader->cancel(); else m_fontDownloader->drop(mine);
    finishDeck();
}

bool Backend::downloadPack() {
    ensureDownloader();
    QStringList families;
    m_packRowsBuilt = false;
    for (const auto &r : packRows()) {
        const auto row = r.toMap();
        if (!row.value(QStringLiteral("installed")).toBool()) families.append(row.value(QStringLiteral("family")).toString());
    }
    m_packFamilies = families;
    m_packFailed.clear();
    m_packStatus = tr("Extra fonts pack: %1 of %2 fonts ready").arg(packReady()).arg(packTotal());
    m_packLeft = QSet<QString>(families.cbegin(), families.cend());
    if (families.isEmpty()) {
        m_packStatus = tr("Extra fonts pack: all %1 fonts ready").arg(packTotal());
        QTimer::singleShot(8000, this, [this] { if (m_packFamilies.isEmpty() && packReady() == packTotal()) { m_packStatus.clear(); emit fontDownloadChanged(); } });
        emit fontDownloadChanged(); emit packFinished(packReady(), packTotal(), {}, false); return true;
    }
    m_fontDownloader->add(families, false);
    emit fontDownloadChanged();
    return true;
}
void Backend::cancelPack() {
    if (!m_fontDownloader || m_packLeft.isEmpty()) return;
    QStringList drop;
    for (const auto &f : std::as_const(m_packLeft)) if (!m_deckLeft.contains(f)) drop.append(f);
    m_fontDownloader->drop(drop);
}

void Backend::familyArrived(const QString &family) {
    // The open deck uses it: applied (by the window, when safe) per family.
    if (m_deckNeeded.contains(family)) m_fontsPending = true;
    if (m_deckLeft.remove(family) && !m_deckInstalled.contains(family)) m_deckInstalled.append(family);
    m_packLeft.remove(family);
    if (m_packRowsBuilt)   // that one row, not the whole pack again
        for (auto &r : m_packRows) {
            auto row = r.toMap();
            if (row.value(QStringLiteral("family")).toString() != family || row.value(QStringLiteral("installed")).toBool()) continue;
            row.insert(QStringLiteral("installed"), true); r = row; ++m_packReadyCount;
        }
    if (!m_packFamilies.isEmpty()) m_packStatus = tr("Extra fonts pack: %1 of %2 fonts ready").arg(packReady()).arg(packTotal());
    emit fontDownloadChanged();
    runFinished({}, {}, false, false);   // settles whichever group just completed
}
void Backend::familyLost(const QString &family, const QString &why, bool network) {
    const QVariantMap entry{{"why", why}, {"network", network}, {"cancelled", why == FontDownloader::cancelledReason()}};
    if (m_deckLeft.remove(family)) m_deckFailed.insert(family, entry);
    if (m_packLeft.remove(family)) m_packFailed.insert(family, entry);
    // Queued: a burst (a cancel, an outage) is settled after its signals, not inside them.
    QMetaObject::invokeMethod(this, [this] { emit fontDownloadChanged(); runFinished({}, {}, false, false); }, Qt::QueuedConnection);
}

// Groups end when their last family is in (or failed); the run's own end
// only matters for families that never reported.
static void summarise(const QVariantMap &failed, QVariantMap &plain, bool &offline) {
    offline = false;
    bool allNetwork = true;
    for (auto it = failed.cbegin(); it != failed.cend(); ++it) {
        const auto e = it.value().toMap();
        if (e.value("cancelled").toBool()) continue;   // not a failure
        offline = true;
        plain.insert(it.key(), e.value("why"));
        allNetwork = allNetwork && e.value("network").toBool();
    }
    offline = offline && allNetwork;
}
void Backend::finishDeck() {
    if (m_deckFamilies.isEmpty()) return;
    QVariantMap failed; bool offline = false;
    summarise(m_deckFailed, failed, offline);
    offline = offline && m_deckInstalled.isEmpty();
    const auto installed = m_deckInstalled;
    const bool cancelled = m_deckCancelled;
    m_deckFamilies.clear(); m_deckInstalled.clear(); m_deckFailed.clear(); m_deckCancelled = false;
    QString message;
    if (cancelled) message = tr("Font download cancelled");
    else {
        if (!installed.isEmpty()) message = tr("Installed free fonts: %1").arg(installed.join(QStringLiteral(", ")));
        if (offline) message += (message.isEmpty() ? QString() : QStringLiteral(". ")) + tr("Couldn't reach the font server; similar fonts are used");
        else if (!failed.isEmpty()) message += (message.isEmpty() ? QString() : QStringLiteral(". ")) + tr("Not installed: %1").arg(failed.keys().join(QStringLiteral(", ")));
    }
    setStatus(message);
    emit fontDownloadChanged();
    emit fontDownloadFinished(installed, failed, offline, cancelled);
    // A deck held for a silent download is shown now, with whatever came.
    // (only the deck that was waiting: never a newer one)
    if (m_gate.active && m_gate.silent) {
        const int id = m_gate.id;
        QMetaObject::invokeMethod(this, [this, id] { if (m_gate.active && m_gate.id == id) finishFontGate(); }, Qt::QueuedConnection);
    }
}

// Each group ends when its last family is in (or failed).
void Backend::runFinished(const QStringList &, const QVariantMap &, bool, bool) {
    if (m_deckLeft.isEmpty()) finishDeck();
    if (m_packLeft.isEmpty() && !m_packFamilies.isEmpty()) {
        QVariantMap failed; bool offline = false;
        summarise(m_packFailed, failed, offline);
        m_packFamilies.clear(); m_packFailed.clear();
        m_packRowsBuilt = false;   // what is on disk now
        const int ready = packReady(), total = packTotal();
        m_packStatus = ready == total ? tr("Extra fonts pack: all %1 fonts ready").arg(total)
                  : offline ? tr("Extra fonts pack: %1 of %2 fonts ready; couldn't reach the font server").arg(ready).arg(total)
                  : failed.isEmpty() ? tr("Extra fonts pack paused: %1 of %2 fonts ready").arg(ready).arg(total)
                  : tr("Extra fonts pack: %1 of %2 fonts ready, %3 not installed").arg(ready).arg(total).arg(failed.size());
        // "All ready" goes after a while; anything less stays to be clicked.
        if (ready == total)
            QTimer::singleShot(8000, this, [this] { if (m_packFamilies.isEmpty() && packReady() == packTotal()) { m_packStatus.clear(); emit fontDownloadChanged(); } });
        emit fontDownloadChanged();
        emit packFinished(ready, total, failed, offline);
    }
}

// Fonts that just arrived are drawn at once, in place. This is not an edit:
// no undo step, no "unsaved changes", and every undo snapshot gets them too,
// so Undo never brings the fallbacks back.
void Backend::applyArrivedFonts() {
    if (!m_fontsPending) return;
    m_fontsPending = false;
    emit fontDownloadChanged();
    if (qEnvironmentVariableIsSet("OMASHOW_NO_FONT_SWAP")) { emit importReportChanged(); return; }
    const auto report = FontSwap::apply(m_document);
    if (report.changed) {
        // Snapshots are only touched when the deck itself had something to upgrade.
        m_history.rewrite([](Document &d) { FontSwap::apply(d); });
        applyActiveShow();
        ++m_revision;
        emit documentChanged();
        emit deckChanged();
        emit selectionChanged();
    }
    emit importReportChanged();
}
