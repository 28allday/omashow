#include "mediaplayback.h"
#include "core/mediaasset.h"
#include <QAudioOutput>
#include <QBuffer>
#include <QFile>
#include <QMediaPlayer>
#include <QSet>
#include <QStandardPaths>

MediaPlayback::MediaPlayback(QObject *parent) : QObject(parent) {}
MediaPlayback::~MediaPlayback() { clear(); }
void MediaPlayback::dispose(Slot *slot) {
  slot->player->stop();
  delete slot->player;
  delete slot->output;
  delete slot->input;
  delete slot;
}
void MediaPlayback::clear() {
  for (auto *slot : m_slots)
    dispose(slot);
  m_slots.clear();
}
QVariantList MediaPlayback::transport() const {
  QVariantList rows;
  for (auto *slot : m_slots)
    rows.append(QVariantMap{{"id", slot->id},
                            {"position", slot->position},
                            {"volume", slot->volume},
                            {"playing", slot->playing},
                            {"playerState", int(slot->player->playbackState())},
                            {"playerPosition", slot->player->position()}});
  return rows;
}
void MediaPlayback::sync(const QVector<SceneObject> &states, bool playing,
                         qreal rate) {
  QSet<QString> keep;
  for (const auto &o : states) {
    if (o.type != ObjectType::Media || !o.mediaAudio || !o.mediaActive ||
        o.hidden || o.opacity <= 0)
      continue;
    if (!o.mediaPath.isEmpty() &&
        MediaAsset::linkState(o, o.mediaReadAllowed) !=
            QStringLiteral("Linked · keep source file"))
      continue;
    const QString key = o.id + "/" + o.mediaId + "/" + o.mediaPath;
    if (keep.contains(key))
      continue;
    keep.insert(key);
    auto *slot = m_slots.value(key);
    if (!slot && playing) {
      slot = new Slot;
      slot->id = o.id;
      if (o.mediaPath.isEmpty()) {
        auto *buffer = new QBuffer;
        buffer->setData(o.mediaData);
        slot->input = buffer;
      } else
        slot->input = new QFile(o.mediaPath);
      if (!slot->input->open(QIODevice::ReadOnly)) {
        emit failed(slot->input->errorString());
        delete slot->input;
        delete slot;
        continue;
      }
      slot->player = new QMediaPlayer(this);
      slot->output = new QAudioOutput(this);
      slot->output->setMuted(QStandardPaths::isTestModeEnabled());
      slot->player->setAudioOutput(slot->output);
      connect(slot->player, &QMediaPlayer::errorOccurred, this,
              [this, name = o.mediaName](QMediaPlayer::Error,
                                         const QString &message) {
                emit failed(tr("Audio playback for %1: %2").arg(name, message));
              });
      connect(slot->player, &QMediaPlayer::tracksChanged, this,
              [slot] { slot->player->setActiveVideoTrack(-1); });
      connect(slot->player, &QMediaPlayer::mediaStatusChanged, this,
              [slot](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::LoadedMedia)
                  slot->player->setPosition(qRound64(slot->position * 1000));
              });
      m_slots.insert(key, slot);
      slot->player->setSourceDevice(slot->input);
    }
    if (!slot)
      continue;
    const qint64 position = qRound64(o.mediaPosition * 1000);
    // Explicit seeks, cue starts and loop wrap are exact; normal clock
    // jitter allows a small drift before seeking the audio device again.
    const bool seek = !playing || !slot->playing ||
                      o.mediaPosition < slot->position ||
                      qAbs(slot->player->position() - position) > 150;
    slot->position = o.mediaPosition;
    slot->volume = o.mediaVolume;
    slot->playing = playing;
    slot->output->setVolume(o.mediaVolume);
    slot->player->setPlaybackRate(rate);
    if (seek)
      slot->player->setPosition(position);
    if (playing)
      slot->player->play();
    else
      slot->player->pause();
  }
  for (auto it = m_slots.begin(); it != m_slots.end();) {
    if (!keep.contains(it.key())) {
      dispose(it.value());
      it = m_slots.erase(it);
    } else
      ++it;
  }
}
