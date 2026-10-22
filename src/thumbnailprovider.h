#pragma once

// Slide thumbnails for the navigator, painted by the same renderer as the
// canvas and the export. The image id is "<slideIndex>/<stamp>": the stamp
// (Bundle::slideStamps) changes only when that slide's picture would, so an
// edit to one slide regenerates one thumbnail and not every one.

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
        QStringList selected, stamps;
        int current = 0;
        bool layoutOk = false, importOk = false;
        int layoutRevision = 0, importRevision = 0, revision = 0;
    };
    void capture(Backend *backend);
    // QML property bindings are notified before ordinary connections, so a
    // preview's first request can reach this thread before the snapshot it
    // names does. Waiting for that revision beats painting a blank the view
    // would then keep until the next change.
    // The same goes for an ordinary slide picture ("<index>/<stamp>"): an edit
    // changes the stamp the view asks for before the new document is captured,
    // and without waiting the old slide is drawn.
    Snapshot snapshotFor(int layoutRevision, int importRevision, int slide = -1, const QString &stamp = QString());
    QObject m_observer;
    QMutex m_mutex;
    QWaitCondition m_captured;
    Snapshot m_snapshot;
};
