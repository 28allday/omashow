#pragma once

// Slide thumbnails for the navigator, painted by the same renderer as the
// canvas and the export. The image id is "<slideIndex>/<revision>": the
// revision is what invalidates the cache, so a thumbnail is regenerated when
// its slide actually changes and not on every repaint.

#include <QQuickImageProvider>

class Backend;

class SlideThumbnailProvider : public QQuickImageProvider {
public:
    explicit SlideThumbnailProvider(Backend *backend);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    Backend *m_backend;
};
