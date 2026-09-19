#pragma once

#include <QHash>
#include <QFileSystemWatcher>
#include <QObject>
#include <QString>

class QDBusVariant;

// Everything the app needs to look like it belongs on this desktop: the current
// Omarchy palette, the desktop's dark/light preference, and its text scale.
// All of it is optional at runtime — on a machine without Omarchy themes or a
// portal, the built-in fallbacks keep the app looking sane.
class OmarchyTheme : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool darkMode READ darkMode NOTIFY darkModeChanged)
    Q_PROPERTY(qreal textScale READ textScale NOTIFY textScaleChanged)
    Q_PROPERTY(QString background READ background NOTIFY colorsChanged)
    Q_PROPERTY(QString foreground READ foreground NOTIFY colorsChanged)
    Q_PROPERTY(QString accent READ accent NOTIFY colorsChanged)
    // Whether that accent actually came from an Omarchy theme. On plain Arch,
    // in a container or in CI there is no colors.toml, and the app still has to
    // look finished — so the palette falls back to the Oma accent rather than
    // to whatever the default happens to be.
    Q_PROPERTY(bool accentFollowed READ accentFollowed NOTIFY colorsChanged)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY colorsChanged)
    Q_PROPERTY(QString selection READ selection NOTIFY colorsChanged)

public:
    explicit OmarchyTheme(QObject *parent = nullptr);

    bool darkMode() const { return m_darkMode; }
    qreal textScale() const { return m_textScale; }
    QString background() const { return m_background; }
    QString foreground() const { return m_foreground; }
    QString accent() const { return m_accent; }
    bool accentFollowed() const { return m_accentFollowed; }
    QString selection() const { return m_selection; }
    QString accentForeground() const { return foregroundFor(m_accent); }

    // key = value pairs from an omarchy colors.toml; empty when there is none.
    static QHash<QString, QString> colorsFromFile(const QString &path);
    // "black" or "white", whichever stays legible on the given colour.
    static QString foregroundFor(const QString &color);
    // Whether a colour reads as dark, for deciding a theme's mode.
    static bool isDark(const QString &color, bool *known);

public slots:
    void reload();

private slots:
    void handlePortalSettingChanged(const QString &nameSpace, const QString &key,
                                    const QDBusVariant &value);

private:
    static QString colorsPath();
    void watchTheme();
    bool detectDarkMode() const;
    qreal detectTextScale() const;

    bool m_darkMode = true;
    qreal m_textScale = 1.0;
    QString m_background;
    QString m_foreground;
    QString m_accent;
    bool m_accentFollowed = false;
    QString m_selection;
    QFileSystemWatcher m_watcher;

signals:
    void darkModeChanged();
    void textScaleChanged();
    void colorsChanged();
};
