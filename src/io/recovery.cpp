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

bool Recovery::write(const Document &document, const QString &originalPath, QString *error) {
    QDir().mkpath(directory());

    QJsonObject meta;
    meta[QStringLiteral("originalPath")] = originalPath;
    meta[QStringLiteral("savedAt")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    meta[QStringLiteral("pid")] = QCoreApplication::applicationPid();

    // The journal is a normal deck with one extra member, so it can be opened
    // by hand if everything else fails.
    QByteArray raw = Bundle::toBytes(document);
    const Zip::Reader reader(raw);
    if (!reader.isValid()) {
        if (error) *error = QStringLiteral("could not build the journal");
        return false;
    }

    QVector<Zip::Entry> entries;
    for (const QString &name : reader.names())
        entries.append({name, reader.read(name), true});
    entries.append({QString::fromLatin1(kMetaMember),
                    QJsonDocument(meta).toJson(QJsonDocument::Indented), true});

    QSaveFile file(journalPathForPid(QCoreApplication::applicationPid()));
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    const QByteArray out = Zip::write(entries);
    if (file.write(out) != out.size()) {
        if (error) *error = file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
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
