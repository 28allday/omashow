#include "theme.h"

#include <QColor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStyleHints>
#include <QTextStream>
#include <QVariant>

namespace {
// Fallbacks for machines without omarchy themes. Keep them boring.
const QString kDarkBackground = QStringLiteral("#101010");
const QString kDarkForeground = QStringLiteral("#eeeeee");
const QString kDarkSelection = QStringLiteral("#186a9a");
const QString kLightBackground = QStringLiteral("#ffffff");
const QString kLightForeground = QStringLiteral("#222324");
const QString kLightSelection = QStringLiteral("#2077b2");
const QString kDefaultAccent = QStringLiteral("#5584aa");

QString currentDir() {
    return QDir::homePath() + QStringLiteral("/.local/state/omarchy/current");
}

QVariant unwrapVariant(QVariant value) {
    while (value.canConvert<QDBusVariant>())
        value = value.value<QDBusVariant>().variant();
    return value;
}

// One portal setting, or an invalid variant when the portal is missing or slow.
// The short timeout keeps a stalled portal off the GUI thread.
QVariant portalSetting(const QString &nameSpace, const QString &key) {
    const QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return {};

    QDBusMessage request = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.Settings"),
        QStringLiteral("Read"));
    request << nameSpace << key;

    const QDBusReply<QDBusVariant> reply(bus.call(request, QDBus::Block, 150));
    if (!reply.isValid())
        return {};

    return reply.value().variant();
}

bool portalSchemeIsDark(const QVariant &value, bool *known) {
    bool ok = false;
    const uint scheme = unwrapVariant(value).toUInt(&ok);
    if (!ok)
        return false;
    if (scheme == 1 || scheme == 2) {
        *known = true;
        return scheme == 1;
    }
    return false;
}

qreal sanitizedTextScale(const QVariant &value) {
    bool ok = false;
    const qreal scale = unwrapVariant(value).toDouble(&ok);
    if (!ok || scale <= 0)
        return 1.0;
    return qBound(0.5, scale, 3.0);
}

QString unquoted(QString value) {
    if (value.size() >= 2
            && ((value.front() == QLatin1Char('"') && value.back() == QLatin1Char('"'))
                || (value.front() == QLatin1Char('\'') && value.back() == QLatin1Char('\''))))
        return value.mid(1, value.size() - 2);
    return value;
}
}

OmarchyTheme::OmarchyTheme(QObject *parent) : QObject(parent) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QGuiApplication::styleHints()) {
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
                this, &OmarchyTheme::reload);
    }
#endif

    QDBusConnection::sessionBus().connect(
        QString(),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.Settings"),
        QStringLiteral("SettingChanged"),
        this,
        SLOT(handlePortalSettingChanged(QString,QString,QDBusVariant)));

    // A theme switch swaps the `current` symlink, so the watch paths have to be
    // re-armed every time, not just at startup.
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &OmarchyTheme::reload);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &OmarchyTheme::reload);

    reload();
}

QString OmarchyTheme::colorsPath() {
    return currentDir() + QStringLiteral("/theme/colors.toml");
}

QHash<QString, QString> OmarchyTheme::colorsFromFile(const QString &path) {
    QHash<QString, QString> colors;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return colors;

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;

        const int equals = line.indexOf(QLatin1Char('='));
        if (equals < 0)
            continue;

        colors.insert(line.left(equals).trimmed(), unquoted(line.mid(equals + 1).trimmed()));
    }
    return colors;
}

QString OmarchyTheme::foregroundFor(const QString &color) {
    bool known = false;
    const bool dark = isDark(color, &known);
    if (!known)
        return QStringLiteral("black");
    return dark ? QStringLiteral("white") : QStringLiteral("black");
}

bool OmarchyTheme::isDark(const QString &color, bool *known) {
    const QColor parsed = QColor::fromString(color);
    if (!parsed.isValid()) {
        *known = false;
        return false;
    }
    *known = true;
    return 0.299 * parsed.redF() + 0.587 * parsed.greenF() + 0.114 * parsed.blueF() < 0.5;
}

void OmarchyTheme::reload() {
    watchTheme();

    const bool wasDark = m_darkMode;
    const qreal wasTextScale = m_textScale;
    const QString wasBackground = m_background;
    const QString wasForeground = m_foreground;
    const QString wasAccent = m_accent;
    const bool wasAccentFollowed = m_accentFollowed;
    const QString wasSelection = m_selection;

    m_darkMode = detectDarkMode();
    m_textScale = detectTextScale();

    m_background = m_darkMode ? kDarkBackground : kLightBackground;
    m_foreground = m_darkMode ? kDarkForeground : kLightForeground;
    m_selection = m_darkMode ? kDarkSelection : kLightSelection;
    m_accent = kDefaultAccent;
    m_accentFollowed = false;

    const QHash<QString, QString> colors = colorsFromFile(colorsPath());
    const auto take = [&colors](const QString &key, QString &target) {
        const QString value = colors.value(key);
        if (value.isEmpty() || !QColor::fromString(value).isValid())
            return false;
        target = value;
        return true;
    };
    take(QStringLiteral("background"), m_background);
    take(QStringLiteral("foreground"), m_foreground);
    m_accentFollowed = take(QStringLiteral("accent"), m_accent);
    take(QStringLiteral("selection"), m_selection);

    // The theme's own mode wins over the portal's: the palette on screen is the
    // one the user picked. Fall back to the background's luminance.
    const QString mode = colors.value(QStringLiteral("mode"));
    if (mode == QStringLiteral("dark") || mode == QStringLiteral("light")) {
        m_darkMode = mode == QStringLiteral("dark");
    } else if (!colors.isEmpty()) {
        bool known = false;
        const bool dark = isDark(m_background, &known);
        if (known)
            m_darkMode = dark;
    }

    if (m_darkMode != wasDark)
        emit darkModeChanged();
    if (!qFuzzyCompare(m_textScale, wasTextScale))
        emit textScaleChanged();
    if (m_background != wasBackground || m_foreground != wasForeground
            || m_accent != wasAccent || m_accentFollowed != wasAccentFollowed
            || m_selection != wasSelection)
        emit colorsChanged();
}

void OmarchyTheme::handlePortalSettingChanged(const QString &nameSpace, const QString &key,
                                              const QDBusVariant &value) {
    Q_UNUSED(nameSpace)
    Q_UNUSED(value)
    if (key == QStringLiteral("color-scheme") || key == QStringLiteral("text-scaling-factor"))
        reload();
}

void OmarchyTheme::watchTheme() {
    const QStringList watched = m_watcher.files() + m_watcher.directories();
    if (!watched.isEmpty())
        m_watcher.removePaths(watched);

    const QString themeDir = currentDir() + QStringLiteral("/theme");
    if (QDir(currentDir()).exists())
        m_watcher.addPath(currentDir());
    if (QDir(themeDir).exists())
        m_watcher.addPath(themeDir);
    if (QFileInfo::exists(colorsPath()))
        m_watcher.addPath(colorsPath());
}

bool OmarchyTheme::detectDarkMode() const {
    bool known = false;
    const QVariant scheme = portalSetting(QStringLiteral("org.freedesktop.appearance"),
                                          QStringLiteral("color-scheme"));
    if (scheme.isValid()) {
        const bool dark = portalSchemeIsDark(scheme, &known);
        if (known)
            return dark;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QGuiApplication::styleHints()) {
        const Qt::ColorScheme qtScheme = QGuiApplication::styleHints()->colorScheme();
        if (qtScheme != Qt::ColorScheme::Unknown)
            return qtScheme == Qt::ColorScheme::Dark;
    }
#endif

    return true;
}

qreal OmarchyTheme::detectTextScale() const {
    const QVariant factor = portalSetting(QStringLiteral("org.gnome.desktop.interface"),
                                          QStringLiteral("text-scaling-factor"));
    return factor.isValid() ? sanitizedTextScale(factor) : 1.0;
}
