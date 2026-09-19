#include "render/liveframes.h"
#include "core/workers.h"
#include "core/imageasset.h"
#include <QFutureWatcher>
#include <QThreadPool>
#include <QtConcurrent>
#include <cmath>

QString LiveFrames::key(const SceneObject &o) {
    return o.id + '/' + o.mediaId + '/' + o.mediaPath + '/' + QString::number(o.mediaReadAllowed)
           + '/' + QString::number(o.mediaBytes) + '/' + QString::number(o.mediaModified)
           + '/' + QString::number(o.image.cacheKey()) + '/' + QString::number(o.imageBrightness, 'g', 17)
           + '/' + QString::number(o.imageContrast, 'g', 17) + '/' + QString::number(o.imageSaturation, 'g', 17)
           + '/' + QString::number(o.imageTintAmount, 'g', 17) + '/' + o.imageTint.name(QColor::HexArgb);
}
LiveFrames::~LiveFrames() { if (m_job) m_job->canceled = true; }
void LiveFrames::reset() {
    ++m_generation;
    if (m_job) m_job->canceled = true;
    m_pending.clear(); m_frames.clear();
}
QVector<SceneObject> LiveFrames::prepare(const QVector<SceneObject> &states) {
    auto painted = states;
    m_pending.clear();
    QSet<QString> visible;
    for (auto &o : painted) {
        const bool media = o.type == ObjectType::Media;
        const bool adjustments = !o.image.isNull() && o.imageFormat != "svg" &&
            (o.imageBrightness != 0 || o.imageContrast != 1 || o.imageSaturation != 1 || o.imageTintAmount != 0);
        if (!media && !adjustments) continue;
        const auto id = key(o);
        visible.insert(id);
        const bool video = media && !o.hidden && o.opacity > 0 && o.mediaVideo && o.mediaPosition >= 0
                           && (o.mediaPath.isEmpty() || o.mediaReadAllowed);
        const auto cached = m_frames.constFind(id);
        if ((video || adjustments) && (cached == m_frames.cend() || cached->time != o.mediaPosition)) m_pending.append(o);
        // During forward playback reuse the most recent frame. On a seek show
        // the poster until the requested position arrives, never an old clip.
        if (cached != m_frames.cend() && ((!video && adjustments) || (video && o.mediaPosition >= cached->time && o.mediaPosition-cached->time <= .25)))
            o.image = cached->image;
        o.imageBrightness = o.imageTintAmount = 0;
        o.imageContrast = o.imageSaturation = 1;
        if (media) o.type = ObjectType::Image; // Never decode or adjust pixels in paint().
    }
    for (auto it = m_frames.begin(); it != m_frames.end(); )
        if (!visible.contains(it.key())) it = m_frames.erase(it); else ++it;
    start();
    return painted;
}
void LiveFrames::start() {
    if (m_running || m_pending.isEmpty()) return;
    const auto pending = std::exchange(m_pending, {});
    const auto generation = m_generation;
    const auto job = std::make_shared<MediaAsset::Job>();
    m_job = job; m_running = true;
    auto *watcher = new QFutureWatcher<QHash<QString, Decoded>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation, job] {
        m_running = false;
        if (generation == m_generation && !job->canceled) {
            const auto frames = watcher->result();
            for (auto it = frames.cbegin(); it != frames.cend(); ++it) m_frames.insert(it.key(), it.value());
            emit ready();
        }
        watcher->deleteLater();
        start();
    });
    watcher->setFuture(QtConcurrent::run(Workers::video(), [pending, job] {
        QHash<QString, Decoded> frames;
        for (const auto &o : pending) {
            if (job->canceled) break;
            auto picture = o;
            if (o.type == ObjectType::Media) picture.image = MediaAsset::frameAt(o, o.mediaPosition, job, true);
            if (!job->canceled) frames.insert(key(o), {ImageAsset::displayImage(picture), o.mediaPosition});
        }
        return frames;
    }));
}
