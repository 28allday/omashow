#pragma once
#include "core/mediaasset.h"
#include <QObject>
#include <QHash>

// GUI-owned front end; decoding only touches immutable copies on a worker.
// One active batch and one replaceable pending batch per canvas, so scrubbing
// never builds a queue of obsolete frames.
class LiveFrames : public QObject {
    Q_OBJECT
public:
    explicit LiveFrames(QObject *parent = nullptr) : QObject(parent) {}
    ~LiveFrames() override;
    QVector<SceneObject> prepare(const QVector<SceneObject> &states);
    void reset();
signals:
    void ready();
private:
    struct Decoded { QImage image; qreal time = -1; };
    QHash<QString, Decoded> m_frames;
    QVector<SceneObject> m_pending;
    std::shared_ptr<MediaAsset::Job> m_job;
    bool m_running = false;
    quint64 m_generation = 0;
    void start();
    static QString key(const SceneObject &object);
};
