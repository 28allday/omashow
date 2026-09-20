#include "io/decklock.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

QString held;   // the deck this process has the lock on

bool processIsAlive(qint64 pid) {
    // Linux: a live pid has a /proc entry. A lock whose owner is gone is stale.
    return pid > 0 && QFileInfo::exists(QStringLiteral("/proc/%1").arg(pid));
}

} // namespace

QString DeckLock::pathFor(const QString &deckPath) {
    if (deckPath.isEmpty()) return {};
    const QFileInfo info(deckPath);
    return info.absolutePath() + '/' + info.fileName() + QStringLiteral(".lock");
}

DeckLock::Holder DeckLock::check(const QString &deckPath) {
    Holder holder;
    const auto path = pathFor(deckPath);
    if (path.isEmpty()) return holder;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return holder;
    const auto json = QJsonDocument::fromJson(file.readAll()).object();
    holder.pid = qint64(json.value(QStringLiteral("pid")).toDouble());
    holder.host = json.value(QStringLiteral("host")).toString();
    holder.since = json.value(QStringLiteral("since")).toString();
    const bool sameMachine = holder.host.isEmpty() || holder.host == QHostInfo::localHostName();
    holder.mine = sameMachine && holder.pid == QCoreApplication::applicationPid();
    // A lock from another machine cannot be checked for life, so it is believed.
    holder.held = holder.mine || !sameMachine || processIsAlive(holder.pid);
    return holder;
}

bool DeckLock::take(const QString &deckPath) {
    release();
    const auto path = pathFor(deckPath);
    if (path.isEmpty()) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    const QJsonObject json{{"pid", double(QCoreApplication::applicationPid())},
                           {"host", QHostInfo::localHostName()},
                           {"since", QDateTime::currentDateTime().toString(Qt::ISODate)}};
    file.write(QJsonDocument(json).toJson());
    if (!file.commit()) return false;
    held = deckPath;
    return true;
}

void DeckLock::release() {
    if (held.isEmpty()) return;
    const auto holder = check(held);
    if (holder.mine) QFile::remove(pathFor(held));
    held.clear();
}
