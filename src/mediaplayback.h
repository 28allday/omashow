#pragma once
#include "core/scene.h"
#include <QHash>
#include <QObject>
#include <QVariantList>
class QAudioOutput;
class QMediaPlayer;
class QIODevice;
class MediaPlayback : public QObject {
  Q_OBJECT
public:
  explicit MediaPlayback(QObject *parent = nullptr);
  ~MediaPlayback() override;
  void sync(const QVector<SceneObject> &states, bool playing, qreal rate);
  void clear();
  QVariantList transport() const;
signals:
  void failed(const QString &message);

private:
  struct Slot {
    QMediaPlayer *player = nullptr;
    QAudioOutput *output = nullptr;
    QIODevice *input = nullptr;
    qreal position = 0, volume = 1;
    bool playing = false;
    QString id;
  };
  QHash<QString, Slot *> m_slots;
  void dispose(Slot *slot);
};
