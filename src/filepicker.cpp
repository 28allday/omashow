#include "filepicker.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QRandomGenerator>

namespace {
// The portal's filter wire format: a(sa(us)) — name, then (type, pattern) rules
// where type 0 is a glob.
struct FilterRule {
    uint type;
    QString pattern;
};
using FilterRules = QList<FilterRule>;

struct FileFilter {
    QString name;
    FilterRules rules;
};
using FileFilters = QList<FileFilter>;

QDBusArgument &operator<<(QDBusArgument &argument, const FilterRule &rule) {
    argument.beginStructure();
    argument << rule.type << rule.pattern;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, FilterRule &rule) {
    argument.beginStructure();
    argument >> rule.type >> rule.pattern;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const FileFilter &filter) {
    argument.beginStructure();
    argument << filter.name << filter.rules;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, FileFilter &filter) {
    argument.beginStructure();
    argument >> filter.name >> filter.rules;
    argument.endStructure();
    return argument;
}

void registerFilterTypes() {
    static bool registered = false;
    if (registered)
        return;
    qDBusRegisterMetaType<FilterRule>();
    qDBusRegisterMetaType<FilterRules>();
    qDBusRegisterMetaType<FileFilter>();
    qDBusRegisterMetaType<FileFilters>();
    registered = true;
}

QVariant filtersFor(const QString &name, const QStringList &patterns) {
    FilterRules rules;
    for (const QString &pattern : patterns)
        rules.append({0u, pattern});

    FileFilters filters;
    filters.append({name.isEmpty() ? QStringLiteral("Files") : name, rules});
    return QVariant::fromValue(filters);
}

QString handleToken() {
    return QStringLiteral("omashow_%1").arg(QRandomGenerator::global()->generate());
}
}

PortalFileChooser::PortalFileChooser(QObject *parent) : QObject(parent) {
    registerFilterTypes();
}

void PortalFileChooser::openFile(const QString &title, const QString &filterName,
                                 const QStringList &patterns) {
    QVariantMap options;
    if (!patterns.isEmpty())
        options.insert(QStringLiteral("filters"), filtersFor(filterName, patterns));
    request(QStringLiteral("OpenFile"), title, options);
}

void PortalFileChooser::saveFile(const QString &title, const QString &suggestedName,
                                 const QString &filterName, const QStringList &patterns) {
    QVariantMap options;
    if (!suggestedName.isEmpty())
        options.insert(QStringLiteral("current_name"), suggestedName);
    if (!patterns.isEmpty())
        options.insert(QStringLiteral("filters"), filtersFor(filterName, patterns));
    request(QStringLiteral("SaveFile"), title, options);
}

bool PortalFileChooser::request(const QString &method, const QString &title,
                                QVariantMap options) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        emit failed(QStringLiteral("No session bus; is a desktop portal running?"));
        return false;
    }

    clearPending();

    const QString token = handleToken();
    options.insert(QStringLiteral("handle_token"), token);

    // Subscribe to the request object before making the call: the portal may
    // answer faster than the reply arrives.
    QString sender = bus.baseService();
    sender.remove(0, 1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    m_pendingPath = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2")
        .arg(sender, token);
    bus.connect(QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
                QStringLiteral("org.freedesktop.portal.Request"),
                QStringLiteral("Response"), this,
                SLOT(handleResponse(uint,QVariantMap)));

    QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.FileChooser"),
        method);
    // The empty parent window handle is fine on Wayland without xdg-foreign;
    // pass "wayland:<exported handle>" if the dialog must be window-modal.
    call << QString() << title << options;

    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *watcher) {
        const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError()) {
            clearPending();
            emit failed(reply.error().message());
        }
    });
    return true;
}

void PortalFileChooser::handleResponse(uint response, const QVariantMap &results) {
    clearPending();

    if (response != 0) {
        emit canceled();
        return;
    }

    const QStringList uris = results.value(QStringLiteral("uris")).toStringList();
    if (uris.isEmpty()) {
        emit canceled();
        return;
    }

    emit selected(QUrl(uris.first()));
}

void PortalFileChooser::clearPending() {
    if (m_pendingPath.isEmpty())
        return;

    QDBusConnection::sessionBus().disconnect(
        QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
        QStringLiteral("org.freedesktop.portal.Request"),
        QStringLiteral("Response"), this,
        SLOT(handleResponse(uint,QVariantMap)));
    m_pendingPath.clear();
}
