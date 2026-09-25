#include "io/recovery.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include "io/bundle.h"
#include "io/zip.h"

namespace {

constexpr auto kMetaMember = "recovery.json";

QString journalPathForPid(qint64 pid) {
    return Recovery::directory() + QStringLiteral("/session-%1.omashow").arg(pid);
}

bool processIsAlive(qint64 pid) {
    if (pid <= 0)
        return false;
    // Linux: a live pid has a /proc entry. A journal whose owner is still
    // running is someone else's open document, not a crash to recover from.
    return QFileInfo::exists(QStringLiteral("/proc/%1").arg(pid));
}

} // namespace

QString Recovery::Journal::displayName() const {
    if (!originalPath.isEmpty())
        return QFileInfo(originalPath).fileName();
    return QStringLiteral("Untitled");
}

QString Recovery::directory() {
    // The state directory, not config and not cache: a journal is neither a
    // preference nor something safe to evict.
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return base + QStringLiteral("/recovery");
}

bool Recovery::write(const Document &document, const QString &originalPath, QString *error, const std::shared_ptr<Workers::Job> &job) {
    QDir().mkpath(directory());

    QJsonObject meta;
    meta[QStringLiteral("originalPath")] = originalPath;
    meta[QStringLiteral("savedAt")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    meta[QStringLiteral("pid")] = QCoreApplication::applicationPid();

    const auto bytes = Bundle::toBytes(document, QJsonDocument(meta).toJson(QJsonDocument::Compact));
    if (bytes.isEmpty() || bytes.size() > Zip::kMaxArchiveBytes) {
        if (error) *error = QStringLiteral("The deck is too large for a recovery journal (over 512 MB).");
        return false;
    }
    return Workers::write(journalPathForPid(QCoreApplication::applicationPid()), bytes, error, job);
}

void Recovery::discard() {
    QFile::remove(journalPathForPid(QCoreApplication::applicationPid()));
}

QVector<Recovery::Journal> Recovery::orphans() {
    QVector<Journal> found;
    QDir dir(directory());
    if (!dir.exists())
        return found;

    const QStringList names = dir.entryList({QStringLiteral("session-*.omashow")}, QDir::Files,
                                            QDir::Time);
    for (const QString &name : names) {
        const QString path = dir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const Zip::Reader reader(file.readAll());
        if (!reader.isValid() || !reader.contains(QString::fromLatin1(kMetaMember))) {
            // A journal we cannot read is no use to anybody, and leaving it
            // behind means offering it again on every launch.
            file.close();
            QFile::remove(path);
            continue;
        }

        const QJsonObject meta =
            QJsonDocument::fromJson(reader.read(QString::fromLatin1(kMetaMember))).object();
        Journal journal;
        journal.journalPath = path;
        journal.originalPath = meta.value(QStringLiteral("originalPath")).toString();
        journal.savedAt = QDateTime::fromString(
            meta.value(QStringLiteral("savedAt")).toString(), Qt::ISODate);
        journal.pid = qint64(meta.value(QStringLiteral("pid")).toDouble());

        if (processIsAlive(journal.pid))
            continue;   // someone else's open document, not a crash
        found.append(journal);
    }
    return found;
}

void Recovery::forget(const QString &journalPath) {
    // Only ever inside our own directory: a path from elsewhere is not ours
    // to delete.
    if (QFileInfo(journalPath).absolutePath() == QFileInfo(directory()).absoluteFilePath())
        QFile::remove(journalPath);
}
