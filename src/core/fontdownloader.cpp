#include "core/fontdownloader.h"
#include "core/fontsandbox.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <algorithm>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QTimer>
#include <QUrl>

namespace {

constexpr int kParallel = 4;
constexpr int kStallMs = 20000;
constexpr int kMaxNetworkFailsInARow = 3;
constexpr qint64 kMaxFile = 8 * 1024 * 1024;

struct Source { QString path, sha256; qint64 size = 0; bool font = true; };

const QJsonObject &table() {
    static const QJsonObject t = [] {
        QFile f(QStringLiteral(":/fonts/font-sources.json"));
        return f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject();
    }();
    return t;
}

// A path inside the pinned repository: plain segments only.
bool safePath(const QString &p) {
    static const QRegularExpression ok(QStringLiteral("^[A-Za-z0-9._\\[\\],@+-]+(/[A-Za-z0-9._\\[\\],@+-]+)*$"));
    return ok.match(p).hasMatch() && !p.contains(QLatin1String(".."));
}

// family (lower case) -> its files, built once. A family with any file that
// fails the checks is left out whole: all of a family or none of it.
const QHash<QString, QList<Source>> &sources() {
    static const QHash<QString, QList<Source>> all = [] {
        QHash<QString, QList<Source>> out;
        const auto families = table().value(QStringLiteral("families")).toObject();
        for (auto it = families.begin(); it != families.end(); ++it) {
            const auto entry = it.value().toObject();
            QList<Source> files;
            bool ok = true;
            const auto add = [&](const QJsonObject &o, bool font) {
                Source s{o.value("p").toString(), o.value("sha256").toString().toLower(), qint64(o.value("size").toDouble()), font};
                if (safePath(s.path) && s.sha256.size() == 64 && s.size > 0 && s.size <= kMaxFile) files.append(s);
                else if (font) ok = false;
            };
            for (const auto &v : entry.value(QStringLiteral("files")).toArray()) add(v.toObject(), true);
            if (!ok || files.isEmpty()) continue;
            if (entry.contains(QStringLiteral("license"))) add(entry.value(QStringLiteral("license")).toObject(), false);
            out.insert(it.key().toLower(), files);
        }
        return out;
    }();
    return all;
}

QList<Source> sourcesOf(const QString &family) { return sources().value(family.toLower()); }

QString folderName(QString family) { return family.remove(QLatin1Char(' ')); }

QString sha256Of(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    h.addData(&f);
    return QString::fromLatin1(h.result().toHex());
}

QString baseUrl() {
    auto base = qEnvironmentVariable("OMASHOW_FONT_BASE_URL").trimmed();
    if (base.isEmpty()) base = table().value(QStringLiteral("base")).toString();
    if (!base.endsWith(QLatin1Char('/'))) base += QLatin1Char('/');
    return base;
}

// Families are built here, beside the shared folder (never inside it: other
// apps may read that folder through fontconfig), and moved in whole with one
// directory rename, so a family folder in the sandbox always means complete.
QString stagingParent() { return FontSandbox::dir() + QStringLiteral(".staging"); }
QString stagingRoot() { return stagingParent() + QLatin1Char('/') + QString::number(QCoreApplication::applicationPid()); }
// Leftovers of OmaShow processes that are gone (killed mid-run).
void cleanDeadStaging() {
    QDir parent(stagingParent());
    for (const auto &name : parent.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok = false;
        const qint64 pid = name.toLongLong(&ok);
        if (ok && pid != QCoreApplication::applicationPid() && QFileInfo::exists(QStringLiteral("/proc/%1").arg(pid))) continue;
        QDir(parent.filePath(name)).removeRecursively();
    }
    QDir().rmdir(stagingParent());   // gone when empty
}
// A same-named folder that isn't a complete download: kept, out of the way.
void setAside(const QString &dir) {
    if (!QFileInfo(dir).isDir()) return;
    const QString aside = FontSandbox::dir() + QStringLiteral(".set-aside");
    QDir().mkpath(aside);
    QString to = aside + QLatin1Char('/') + QFileInfo(dir).fileName();
    for (int n = 2; QFileInfo::exists(to); ++n) to = aside + QLatin1Char('/') + QFileInfo(dir).fileName() + QLatin1Char('-') + QString::number(n);
    if (!QDir().rename(dir, to)) QDir().rmdir(dir);   // an empty one can just go
}
QString familyDir(const QString &family) { return FontSandbox::dir() + QLatin1Char('/') + folderName(family); }
QString stagingDir(const QString &family) { return stagingRoot() + QLatin1Char('/') + folderName(family); }

qint64 freeBytes(const QString &path) {
    if (qEnvironmentVariableIsSet("OMASHOW_FONT_TEST_FREE_BYTES")) return qEnvironmentVariable("OMASHOW_FONT_TEST_FREE_BYTES").toLongLong();
    QString p = path;
    while (!p.isEmpty() && !QFileInfo::exists(p)) p = QFileInfo(p).path();
    return QStorageInfo(p.isEmpty() ? QDir::homePath() : p).bytesAvailable();
}

} // namespace

FontDownloader::FontDownloader(QObject *parent) : QObject(parent) {}
FontDownloader::~FontDownloader() { cancel(); }

bool FontDownloader::downloadable(const QString &family) { return !sourcesOf(family).isEmpty(); }

qint64 FontDownloader::bytesFor(const QStringList &families) {
    qint64 n = 0;
    for (const auto &f : families) for (const auto &s : sourcesOf(f)) n += s.size;
    return n;
}

bool FontDownloader::complete(const QString &family) {
    const auto sources = sourcesOf(family);
    if (sources.isEmpty()) return false;
    const QString dir = familyDir(family) + QLatin1Char('/');
    for (const auto &s : sources)
        if (s.font && QFileInfo(dir + QFileInfo(s.path).fileName()).size() != s.size) return false;   // stat only
    return true;
}
QString FontDownloader::cancelledReason() { return tr("cancelled"); }
QString FontDownloader::familyForFolder(const QString &folder) {
    static const QHash<QString, QString> byFolder = [] {
        QHash<QString, QString> out;
        const auto families = table().value(QStringLiteral("families")).toObject();
        for (auto it = families.begin(); it != families.end(); ++it) out.insert(folderName(it.key()).toLower(), it.key());
        return out;
    }();
    return byFolder.value(folder.toLower());
}

qint64 FontDownloader::bytesDone(const QStringList &families) const {
    qint64 n = 0;
    for (const auto &f : families) {
        const auto it = m_fam.constFind(f);
        if (it != m_fam.cend()) n += it->done;
        else if (complete(f)) n += bytesFor({f});
    }
    return n;
}

bool FontDownloader::loadFamily(const QString &family) {
    QDirIterator it(familyDir(family), {QStringLiteral("*.ttf"), QStringLiteral("*.otf")}, QDir::Files);
    bool any = false;
    while (it.hasNext()) {
        if (FontSandbox::loadFile(it.next(), folderName(family)).isEmpty()) return false;
        any = true;
    }
    return any && FontSandbox::available(family);
}

bool FontDownloader::add(const QStringList &families, bool front) {
    if (!m_running) {
        // Leftovers of runs that were killed: only half families live there.
        cleanDeadStaging();
        QDir(stagingRoot()).removeRecursively();
        m_running = true; m_cancelled = false; m_networkFails = 0;
        m_failed.clear(); m_installed.clear();
    }
    QList<File> batch;
    QStringList added;
    qint64 need = 0;
    for (const auto &family : families) {
        if (m_fam.contains(family)) {
            if (front) {   // already queued: its files go to the front
                QList<File> mine, rest;
                for (const auto &f : std::as_const(m_queue)) (f.family == family ? mine : rest).append(f);
                batch += mine; m_queue = rest;
            }
            continue;
        }
        if (m_installed.contains(family) || m_failed.contains(family)) { m_failed.remove(family); }
        const auto sources = sourcesOf(family);
        if (sources.isEmpty()) { m_failed.insert(family, tr("no download is listed for it")); emit familyFailed(family, m_failed.value(family).toString(), false); continue; }
        if (complete(family) && (FontSandbox::available(family) || loadFamily(family))) {
            // Complete on disk (an earlier run, or another OmaShow): just use it.
            if (!m_installed.contains(family)) m_installed.append(family);
            QTimer::singleShot(0, this, [this, family] { emit familyReady(family); });
            continue;
        }
        const QString dir = stagingDir(family);
        QDir(dir).removeRecursively();
        if (!QDir().mkpath(dir) || !QFileInfo(dir).isWritable()) {
            m_failed.insert(family, tr("cannot write to %1").arg(FontSandbox::dir()));
            emit familyFailed(family, m_failed.value(family).toString(), false);
            continue;
        }
        Family fam;
        for (const auto &s : sources) {
            batch.append(File{family, s.path, s.sha256, dir + QLatin1Char('/') + QFileInfo(s.path).fileName(), s.size, s.font});
            ++fam.left; fam.total += s.size;
        }
        need += fam.total;
        m_fam.insert(family, fam);
        added.append(family);
    }
    // Room for what is about to arrive and what is already queued, with a margin.
    for (const auto &f : std::as_const(m_queue)) need += f.size;
    if (!added.isEmpty() && freeBytes(stagingRoot()) < need + need / 10) {
        for (const auto &family : added) failFamily(family, tr("not enough disk space"), false);
        batch.erase(std::remove_if(batch.begin(), batch.end(), [&](const File &f) { return added.contains(f.family); }), batch.end());
    }
    m_queue = front ? batch + m_queue : m_queue + batch;
    tick();
    QTimer::singleShot(0, this, &FontDownloader::pump);
    return true;
}

void FontDownloader::pump() {
    if (!m_running) return;
    while (m_active.size() < kParallel && !m_queue.isEmpty()) {
        const File file = m_queue.takeFirst();
        if (!m_fam.contains(file.family)) continue;   // its family failed or was dropped
        auto *out = new QFile(file.target, this);
        if (!out->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            delete out;
            failFamily(file.family, tr("cannot write to %1").arg(FontSandbox::dir()), false);
            continue;
        }
        QNetworkRequest request(QUrl(baseUrl() + file.path));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setMaximumRedirectsAllowed(3);
        if (!m_net) m_net = new QNetworkAccessManager(this);
        auto *reply = m_net->get(request);
        auto *stall = new QTimer(reply);
        stall->setSingleShot(true);
        stall->start(kStallMs);
        connect(stall, &QTimer::timeout, reply, [reply] { reply->setProperty("why", QStringLiteral("stalled")); reply->abort(); });
        m_active.insert(reply, Active{file, out, stall, 0});
        connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
            auto it = m_active.find(reply);
            if (it == m_active.end()) return;
            const auto bytes = reply->readAll();
            it->received += bytes.size();
            it->stall->start(kStallMs);
            if (it->received > it->file.size) { reply->setProperty("why", QStringLiteral("oversize")); reply->abort(); return; }
            if (it->out->write(bytes) != bytes.size()) { reply->setProperty("why", QStringLiteral("diskfull")); reply->abort(); return; }
            auto fam = m_fam.find(it->file.family);
            if (fam != m_fam.end()) fam->done += bytes.size();
            tick();
        });
        connect(reply, &QNetworkReply::finished, this, [this, reply] { done(reply); });
    }
    if (m_active.isEmpty() && m_queue.isEmpty()) finish(m_cancelled);
}

void FontDownloader::done(QNetworkReply *reply) {
    reply->deleteLater();
    auto it = m_active.find(reply);
    if (it == m_active.end()) return;
    Active a = *it;
    m_active.erase(it);
    QString why = reply->property("why").toString();
    if (why.isEmpty() && reply->error() == QNetworkReply::NoError) {
        const auto tail = reply->readAll();
        a.received += tail.size();
        if (a.received > a.file.size) why = QStringLiteral("oversize");
        else if (a.out->write(tail) != tail.size()) why = QStringLiteral("diskfull");
    }
    a.out->close();
    a.out->deleteLater();
    const QString family = a.file.family;
    // Dropped meanwhile: its staging folder is gone already. Not removing the
    // path: the family may have been asked for again and that file is new.
    if (why == QLatin1String("dropped") || !m_fam.contains(family)) { pump(); return; }
    bool network = false;
    QString error;
    if (why == QLatin1String("stalled")) { error = tr("the download stalled"); network = true; }
    else if (why == QLatin1String("oversize")) error = tr("the server sent a different file");
    else if (why == QLatin1String("diskfull")) error = tr("disk full");
    else if (reply->error() != QNetworkReply::NoError) {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        error = status ? tr("the server answered %1").arg(status) : reply->errorString();
        network = status == 0 || status == 429 || status >= 500;
    } else if (sha256Of(a.file.target) != a.file.sha256) error = tr("the file did not match its checksum");

    if (!error.isEmpty()) {
        // A licence text is not worth the family (counted as a network failure still).
        if (a.file.font || why == QLatin1String("diskfull")) failFamily(family, error, network);
        else { QFile::remove(a.file.target); if (--m_fam[family].left == 0) settle(family); }   // a licence text: not worth the family
        if (why == QLatin1String("diskfull")) stopAll(tr("disk full"), false);
        else if (network && ++m_networkFails >= kMaxNetworkFailsInARow) stopAll(tr("couldn't reach the font server"), true);
    } else {
        m_networkFails = 0;
        if (--m_fam[family].left == 0) settle(family);
    }
    tick();
    pump();
}
void FontDownloader::tick() {
    if (!m_progressTick) {
        m_progressTick = new QTimer(this);
        m_progressTick->setSingleShot(true);
        m_progressTick->setInterval(250);
        connect(m_progressTick, &QTimer::timeout, this, &FontDownloader::progress);
    }
    if (!m_progressTick->isActive()) m_progressTick->start();
}

// Every file of the family is in staging and verified: one rename moves the
// whole folder into the sandbox, then it is loaded.
void FontDownloader::settle(const QString &family) {
    m_fam.remove(family);
    const QString target = familyDir(family);
    QDir().mkpath(FontSandbox::dir());
    if (complete(family)) {
        // Another OmaShow finished it meanwhile: keep theirs.
        QDir(stagingDir(family)).removeRecursively();
    } else {
        setAside(target);   // not a complete download: out of the way, not deleted
        if (!QDir().rename(stagingDir(family), target)) {
        QDir(stagingDir(family)).removeRecursively();
        m_failed.insert(family, tr("cannot write to %1").arg(FontSandbox::dir()));
        emit familyFailed(family, m_failed.value(family).toString(), false);
        return;
        }
    }
    if (!FontSandbox::available(family) && !loadFamily(family)) {
        QDir(target).removeRecursively();   // ours, verified yet unusable: not kept
        m_failed.insert(family, tr("not a usable font"));
        emit familyFailed(family, m_failed.value(family).toString(), false);
        return;
    }
    m_installed.append(family);
    emit familyReady(family);
}

void FontDownloader::failFamily(const QString &family, const QString &why, bool network) {
    if (!m_fam.contains(family)) return;
    m_fam.remove(family);
    for (auto it = m_active.begin(); it != m_active.end(); ++it)
        if (it->file.family == family) { it.key()->setProperty("why", QStringLiteral("dropped")); QTimer::singleShot(0, it.key(), &QNetworkReply::abort); }
    QDir(stagingDir(family)).removeRecursively();
    if (!network) m_allNetwork = false;
    m_failed.insert(family, why);
    emit familyFailed(family, why, network);
}

void FontDownloader::stopAll(const QString &why, bool network) {
    const auto families = m_fam.keys();
    for (const auto &f : families) failFamily(f, why, network);
    m_queue.clear();
}

void FontDownloader::drop(const QStringList &families) {
    for (const auto &f : families) failFamily(f, cancelledReason(), false);
    pump();
}

void FontDownloader::cancel() {
    if (!m_running) return;
    m_cancelled = true;
    stopAll(cancelledReason(), false);
    const auto replies = m_active.keys();
    for (auto *reply : replies) reply->abort();
    for (auto it = m_active.begin(); it != m_active.end(); ++it) { it->out->close(); QFile::remove(it->file.target); }
    m_active.clear();
    QDir(stagingRoot()).removeRecursively();
    finish(true);
}

void FontDownloader::finish(bool cancelled) {
    if (!m_running) return;
    m_running = false;
    QDir(stagingRoot()).removeRecursively();
    QDir().rmdir(stagingParent());   // gone when empty
    if (m_progressTick) m_progressTick->stop();
    emit progress();
    const bool offline = m_installed.isEmpty() && !m_failed.isEmpty() && m_allNetwork;
    emit finished(m_installed, m_failed, offline, cancelled || m_cancelled);
    m_allNetwork = true;
}
