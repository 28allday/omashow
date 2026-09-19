#pragma once

// Slide thumbnails for the navigator, painted by the same renderer as the
// canvas and the export. The image id is "<slideIndex>/<revision>": the
// revision is what invalidates the cache, so a thumbnail is regenerated when
// its slide actually changes and not on every repaint.

#include <QQuickImageProvider>
#include <QMutex>
#include <QObject>
#include "core/scene.h"

class Backend;

class SlideThumbnailProvider : public QQuickImageProvider {
public:
    explicit SlideThumbnailProvider(Backend *backend);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    struct Snapshot {
        Document document, layoutDocument;
        QVector<SceneObject> diagram;
        SceneObject before, after;
        QStringList selected;
        int current = 0;
        bool layoutOk = false;
    };
    void capture(Backend *backend);
    QObject m_observer;
    QMutex m_mutex;
    Snapshot m_snapshot;
};
