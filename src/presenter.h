#pragma once
#include "backend.h"
#include "showguard.h"
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <QWindow>

class Presenter : public QObject {
  Q_OBJECT
  Q_PROPERTY(QUrl pendingExternalLink READ pendingExternalLink NOTIFY stateChanged)
  Q_PROPERTY(bool running READ running NOTIFY stateChanged)
  Q_PROPERTY(bool paused READ paused NOTIFY stateChanged)
  Q_PROPERTY(bool rehearsal READ rehearsal NOTIFY stateChanged)
  Q_PROPERTY(QString status READ status NOTIFY stateChanged)
  Q_PROPERTY(QVariantList displays READ displays NOTIFY displaysChanged)
  Q_PROPERTY(int audienceIndex READ audienceIndex WRITE setAudienceIndex NOTIFY
                 displaysChanged)
  Q_PROPERTY(int presenterIndex READ presenterIndex NOTIFY displaysChanged)
  Q_PROPERTY(qreal audienceTime READ audienceTime NOTIFY timeChanged)
  Q_PROPERTY(
      int blankMode READ blankMode WRITE setBlankMode NOTIFY stateChanged)
  Q_PROPERTY(bool frozen READ frozen WRITE setFrozen NOTIFY stateChanged)
  Q_PROPERTY(qreal elapsed READ elapsed NOTIFY clockChanged)
  Q_PROPERTY(int targetMinutes READ targetMinutes WRITE setTargetMinutes NOTIFY
                 clockChanged)
  Q_PROPERTY(int buildNumber READ buildNumber NOTIFY timeChanged)
  Q_PROPERTY(int nextSlideIndex READ nextSlideIndex NOTIFY timeChanged)
  Q_PROPERTY(int buildCount READ buildCount NOTIFY timeChanged)
public:
  Presenter(Backend *backend, ShowGuard *guard, QObject *parent = nullptr);
  ~Presenter() override;
  bool running() const { return m_running; }
  bool paused() const { return m_userPaused; }
  bool rehearsal() const { return m_rehearsal; }
  QString status() const { return m_status; }
  QVariantList displays() const;
  int audienceIndex() const;
  int presenterIndex() const;
  void setAudienceIndex(int index);
  qreal audienceTime() const {
    return m_frozen ? m_frozenTime : m_backend->time();
  }
  int blankMode() const { return m_blank; }
  void setBlankMode(int mode);
  bool frozen() const { return m_frozen; }
  void setFrozen(bool frozen);
  qreal elapsed() const {
    return m_running && m_clock.isValid() ? m_clock.elapsed() / 1000.0 : m_elapsedStopped;
  }
  int targetMinutes() const { return m_targetMinutes; }
  void setTargetMinutes(int minutes);
  int buildNumber() const;
  int buildCount() const;
  int nextSlideIndex() const;
  static int chooseAudience(const QStringList &names, const QString &preferred);
  Q_INVOKABLE void attach(QWindow *audience, QWindow *console, QWindow *editor);
  Q_INVOKABLE bool start(bool fromCurrent = false, bool rehearsal = false);
  Q_INVOKABLE void stop();
  QUrl pendingExternalLink() const { return m_pendingExternalLink; }
  Q_INVOKABLE bool activateAt(qreal x,qreal y);
  Q_INVOKABLE bool openPendingLink();
  Q_INVOKABLE void cancelPendingLink();
  Q_INVOKABLE void next();
  Q_INVOKABLE void previous();
  Q_INVOKABLE void jump(int index);
  Q_INVOKABLE void pauseResume();
  Q_INVOKABLE void restartClock();
  Q_INVOKABLE void swapDisplays();
  Q_INVOKABLE void refreshDisplays();
signals:
  void externalLinkRequested(const QUrl &url);
  void stateChanged();
  void displaysChanged();
  void timeChanged();
  void clockChanged();

private:
  QVector<qreal> clickBoundaries() const;
  void runToBoundary();
  void routeWindows();
  Backend *m_backend;
  ShowGuard *m_guard;
  QPointer<QWindow> m_audience, m_console, m_editor;
  QUrl m_pendingExternalLink;
  QString m_preferredAudience, m_status;
  bool m_running = false, m_rehearsal = false, m_frozen = false,
       m_userPaused = false;
  qreal m_frozenTime = 0, m_boundary = 0, m_elapsedStopped = 0;
  int m_blank = 0, m_consumedClicks = 0, m_targetMinutes = 20;
  QElapsedTimer m_clock;
  QTimer m_timer;
  // A slide that moves on by itself: armed when its builds finish, and stopped
  // by anything that takes the show out of the speaker's hands.
  QTimer m_advance;
};
