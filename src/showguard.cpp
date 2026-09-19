#include "showguard.h"

#include <QStandardPaths>

namespace {

QString runCommand(const QString &program, const QStringList &arguments) {
    if (QStandardPaths::findExecutable(program).isEmpty())
        return QString();

    QProcess process;
    process.start(program, arguments);
    // Bounded: a stalled shell must never hold up the start of a show.
    if (!process.waitForFinished(400))
        return QString();
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

} // namespace

ShowGuard::ShowGuard(QObject *parent) : QObject(parent), m_runner(runCommand) {
    setState(tr("Not presenting"));
}

void ShowGuard::setState(const QString &state) {
    if (m_state == state)
        return;
    m_state = state;
    emit engagedChanged();
}

void ShowGuard::engage() {
    if (m_engaged)
        return;
    m_engaged = true;

    QStringList held;

    // --- idle and lock ----------------------------------------------------
    if (!m_inhibitorProgram.isEmpty() && m_inhibitor.state() == QProcess::NotRunning) {
        m_inhibitor.start(m_inhibitorProgram,
                          {QStringLiteral("--what=idle:handle-lid-switch"),
                           QStringLiteral("--who=OmaShow"),
                           QStringLiteral("--why=Presenting"),
                           QStringLiteral("cat")});
        if (m_inhibitor.waitForStarted(400))
            held << tr("idle");
    }

    // --- notifications ----------------------------------------------------
    const QString before = m_runner(QStringLiteral("omarchy-shell"),
                                    {QStringLiteral("notifications"),
                                     QStringLiteral("dndState")});
    if (before == QLatin1String("off")) {
        const QString now = m_runner(QStringLiteral("omarchy-shell"),
                                     {QStringLiteral("notifications"),
                                      QStringLiteral("setDnd"), QStringLiteral("on")});
        if (now == QLatin1String("on")) {
            // Only restore what we actually changed: someone already in
            // do-not-disturb stays there when the show ends.
            m_restoreDndOff = true;
            held << tr("notifications");
        }
    } else if (before == QLatin1String("on")) {
        held << tr("notifications");
    }

    setState(held.isEmpty() ? tr("Presenting — nothing could be held")
                            : tr("Presenting — holding %1").arg(held.join(tr(" and "))));
    emit engagedChanged();
}

void ShowGuard::release() {
    if (!m_engaged)
        return;
    m_engaged = false;

    if (m_inhibitor.state() != QProcess::NotRunning) {
        // Closing our end of the pipe is the signal: `cat` reads EOF and exits,
        // which is also exactly what happens if this process dies.
        m_inhibitor.closeWriteChannel();
        if (!m_inhibitor.waitForFinished(400))
            m_inhibitor.kill();
    }

    if (m_restoreDndOff) {
        m_runner(QStringLiteral("omarchy-shell"),
                 {QStringLiteral("notifications"), QStringLiteral("setDnd"),
                  QStringLiteral("off")});
        m_restoreDndOff = false;
    }

    setState(tr("Not presenting"));
    emit engagedChanged();
}
