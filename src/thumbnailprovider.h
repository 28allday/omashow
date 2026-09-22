#pragma once

// Slide thumbnails for the navigator, painted by the same renderer as the
// canvas and the export. The image id is "<slideIndex>/<revision>": the
// revision is what invalidates the cache, so a thumbnail is regenerated when
// its slide actually changes and not on every repaint.

#include <QQuickImageProvider>
#include <QMutex>
#include <QWaitCondition>
#include <QObject>
#include "core/scene.h"

class Backend;

class SlideThumbnailProvider : public QQuickImageProvider {
public:
    explicit SlideThumbnailProvider(Backend *backend);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    struct Snapshot {
        Document document, layoutDocument, importDocument, importSource;
        QVector<SceneObject> diagram;
        SceneObject before, after;
        QStringList selected;
        int current = 0;
        bool layoutOk = false, importOk = false;
        int layoutRevision = 0, importRevision = 0, revision = 0;
    };
    void capture(Backend *backend);
    // QML property bindings are notified before ordinary connections, so a
    // preview's first request can reach this thread before the snapshot it
    // names does. Waiting for that revision beats painting a blank the view
    // would then keep until the next change.
    // The same goes for an ordinary slide picture ("<index>/<revision>"):
    // opening a deck changes the revision the view asks for before the new
    // document is captured, and without waiting the old deck is drawn.
    Snapshot snapshotFor(int layoutRevision, int importRevision, int documentRevision = 0);
    QObject m_observer;
    QMutex m_mutex;
    QWaitCondition m_captured;
    Snapshot m_snapshot;
};
