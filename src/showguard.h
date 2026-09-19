#pragma once

// Everything that must not happen while you are presenting.
//
//  - The lock screen must not arrive mid-talk. Nothing is worse.
//  - A notification must never land on the audience display.
//  - The pointer must not sit in the middle of a slide.
//
// Idle is held off with a `systemd-inhibit` child whose stdin is a pipe we
// keep open. When the show ends we close it; if this process dies or crashes
// the pipe closes anyway, the child sees EOF and exits, and the lock is
// released. A lock we could only release on a clean exit would be worse than
// no lock at all.
//
// (The Wayland idle-inhibit protocol would be tidier, but Qt does not expose
// it, and shelling out is honest and observable.)
//
// Do-not-disturb goes through the Omarchy shell's IPC, and the previous state
// is put back exactly as it was — a user who was already in do-not-disturb
// stays there when the show ends.

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <functional>

class ShowGuard : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool engaged READ engaged NOTIFY engagedChanged)
    Q_PROPERTY(QString state READ state NOTIFY engagedChanged)

public:
    // Runs a command and returns its trimmed stdout, or a null string if it
    // could not run. Injected so tests can drive the state machine without a
    // shell, a compositor or a notification daemon.
    using Runner = std::function<QString(const QString &, const QStringList &)>;

    explicit ShowGuard(QObject *parent = nullptr);

    void setRunner(Runner runner) { m_runner = std::move(runner); }
    // Empty disables the idle inhibitor — for a machine without systemd, and
    // for tests, which have no business spawning one.
    void setIdleInhibitor(const QString &program) { m_inhibitorProgram = program; }

    bool engaged() const { return m_engaged; }
    // What is actually being held, for the interface to report honestly rather
    // than claiming protection it does not have.
    QString state() const { return m_state; }

    Q_INVOKABLE void engage();
    Q_INVOKABLE void release();

signals:
    void engagedChanged();

private:
    void setState(const QString &state);

    Runner m_runner;
    QProcess m_inhibitor;
    QString m_inhibitorProgram = QStringLiteral("systemd-inhibit");
    bool m_engaged = false;
    bool m_restoreDndOff = false;
    QString m_state;
};
