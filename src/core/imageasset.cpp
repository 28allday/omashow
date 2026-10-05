#include "core/imageasset.h"
#include "core/shape.h"
#include "core/svgasset.h"
#include <QBuffer>
#include <QCache>
#include <QColorSpace>
#include <QCryptographicHash>
#include <QFile>
#include <QImageReader>
#include <QPaintEngine>
#include <QPainter>
#include <cmath>
namespace {
bool fail(QString *error, const QString &message) {
  if (error)
    *error = message;
  return false;
}
QImage adjusted(const SceneObject &o) {
  if (qFuzzyIsNull(o.imageBrightness) && qFuzzyCompare(o.imageContrast, 1.0) &&
      qFuzzyCompare(o.imageSaturation, 1.0) && qFuzzyIsNull(o.imageTintAmount))
    return o.image;
  static thread_local QCache<QString, QImage> cache(
      192 * 1024); // KiB, bounded independently of the embedded source.
  const QString key = QString::number(o.image.cacheKey()) + "/" +
                      QString::number(o.imageBrightness, 'g', 17) + "/" +
                      QString::number(o.imageContrast, 'g', 17) + "/" +
                      QString::number(o.imageSaturation, 'g', 17) + "/" +
                      o.imageTint.name() + "/" +
                      QString::number(o.imageTintAmount, 'g', 17);
  if (auto *found = cache.object(key))
    return *found;
  auto image = o.image.convertToFormat(QImage::Format_ARGB32);
  for (int y = 0; y < image.height(); ++y) {
    auto *row = reinterpret_cast<QRgb *>(image.scanLine(y));
    for (int x = 0; x < image.width(); ++x) {
      const auto pixel = row[x];
      qreal r = qRed(pixel) / 255.0, g = qGreen(pixel) / 255.0,
            b = qBlue(pixel) / 255.0;
      const auto luminance = .2126 * r + .7152 * g + .0722 * b;
      const auto channel = [&](qreal value, qreal tint) {
        value = luminance + (value - luminance) * o.imageSaturation;
        value = (value - .5) * o.imageContrast + .5 + o.imageBrightness;
        return qBound(0,
                      qRound((value * (1 - o.imageTintAmount) +
                              tint * o.imageTintAmount) *
                             255),
                      255);
      };
      row[x] = qRgba(channel(r, o.imageTint.redF()),
                     channel(g, o.imageTint.greenF()),
                     channel(b, o.imageTint.blueF()), qAlpha(pixel));
    }
  }
  cache.insert(key, new QImage(image),
               qMax(1, int(image.sizeInBytes() / 1024)));
  return image;
}
// Bilinear sampling reads four source pixels, so a picture drawn smaller than
// its pixels skips some of them and fine detail turns to grain. A picture is
// brought down to the size it has on the device by area averaging instead and
// drawn one to one: by halves first, which are kept for every size, then the
// remaining step of under 2x.
QImage reduced(const QImage &from, const QSize &size, const QString &key) {
  static thread_local QCache<QString, QImage> cache(
      192 * 1024); // KiB, as for the adjusted pictures above.
  if (auto *found = cache.object(key))
    return *found;
  const auto image =
      from.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  cache.insert(key, new QImage(image),
               qMax(1, int(image.sizeInBytes() / 1024)));
  return image;
}
void drawReduced(QPainter &painter, const QRectF &target, const QImage &full,
                 const QRectF &source) {
  const auto device = painter.deviceTransform();
  const qreal across = target.width() * std::hypot(device.m11(), device.m12()),
              down = target.height() * std::hypot(device.m21(), device.m22());
  // Only for pixels: a PDF page keeps the whole picture for zoom and print.
  if (painter.paintEngine()->type() != QPaintEngine::Raster || across < 1 ||
      down < 1 || (source.width() < across + 1 && source.height() < down + 1)) {
    painter.drawImage(target, full, source);
    return;
  }
  auto image = full;
  const auto id = QString::number(full.cacheKey());
  for (qreal ratio = qMin(source.width() / across, source.height() / down);
       ratio >= 2 && image.width() > 1 && image.height() > 1; ratio /= 2) {
    const QSize half((image.width() + 1) / 2, (image.height() + 1) / 2);
    image = reduced(image, half,
                    id + QStringLiteral("/%1x%2").arg(half.width()).arg(
                             half.height()));
  }
  // The whole picture at the scale that puts its shown part on the device.
  const QSize size(
      qBound(1, qRound(full.width() * across / source.width()), image.width()),
      qBound(1, qRound(full.height() * down / source.height()),
             image.height()));
  if (size != image.size())
    image = reduced(image, size,
                    id + QStringLiteral("/%1x%2").arg(size.width()).arg(
                             size.height()));
  const qreal sx = qreal(image.width()) / full.width(),
              sy = qreal(image.height()) / full.height();
  painter.drawImage(target, image,
                    QRectF(source.x() * sx, source.y() * sy,
                           source.width() * sx, source.height() * sy));
}
QImage sRGB(QImage image) {
  if (image.colorSpace().isValid() &&
      image.colorSpace() != QColorSpace(QColorSpace::SRgb))
    image = image.convertedToColorSpace(QColorSpace::SRgb);
  image.setColorSpace(QColorSpace::SRgb);
  return image;
}
bool valid(const QSize &size) {
  return !size.isEmpty() &&
         qint64(size.width()) * size.height() <= ImageAsset::maxPixels;
}
} // namespace
QImage ImageAsset::displayImage(const SceneObject &object) { return adjusted(object); }
QString ImageAsset::identity(const QByteArray &bytes) {
  return QString::fromLatin1(
      QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
bool ImageAsset::decode(SceneObject &o, const QByteArray &bytes,
                        QString *error) {
  if (bytes.isEmpty() || bytes.size() > maxBytes)
    return fail(error,
                QStringLiteral("Image files must be smaller than 64 MB."));
  if (bytes.trimmed().startsWith("<") || bytes.startsWith("\xef\xbb\xbf"))
    return SvgAsset::decode(o, bytes, error);
  QBuffer buffer;
  buffer.setData(bytes);
  buffer.open(QIODevice::ReadOnly);
  QImageReader reader(&buffer);
  reader.setAutoTransform(true);
  const auto format = reader.format().toLower();
  if (format != "png" && format != "jpeg" && format != "jpg" &&
      format != "webp")
    return fail(error, QStringLiteral("Choose a PNG, JPEG or WebP image."));
  if (!valid(reader.size()))
    return fail(error, QStringLiteral(
                           "Images must contain at most 32 million pixels."));
  const auto image = reader.read();
  if (image.isNull())
    return fail(error, reader.errorString());
  if (!valid(image.size()))
    return fail(error, QStringLiteral("The decoded image is too large."));
  o.imageFormat = format=="jpeg" || format=="jpg" ? "jpg" : QString::fromLatin1(format);
  o.vectorPicture = QPicture();
  o.vectorSize = {};
  o.image = sRGB(image);
  o.imageData = bytes;
  o.imageId = identity(bytes);
  o.type = ObjectType::Image;
  return true;
}
bool ImageAsset::fromImage(SceneObject &o, const QImage &image,
                           QString *error) {
  if (!valid(image.size()))
    return fail(error, QStringLiteral(
                           "Images must contain at most 32 million pixels."));
  const auto pixels = sRGB(image);
  QByteArray bytes;
  QBuffer buffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  if (!pixels.save(&buffer, "PNG") || bytes.size() > maxBytes)
    return fail(error, QStringLiteral("The image could not be stored."));
  o.imageFormat = "png";
  o.vectorPicture = QPicture();
  o.vectorSize = {};
  o.image = pixels;
  o.imageData = bytes;
  o.imageId = identity(bytes);
  o.type = ObjectType::Image;
  return true;
}
bool ImageAsset::fromFile(SceneObject &o, const QString &path, QString *error) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return fail(error, file.errorString());
  if (file.size() > maxBytes)
    return fail(error,
                QStringLiteral("Image files must be smaller than 64 MB."));
  SceneObject decoded;
  if (!decode(decoded, file.readAll(), error))
    return false;
  if (decoded.imageFormat == "svg") {
    copyData(o, decoded);
    o.type = ObjectType::Image;
    return true;
  }
  // Embed orientation-correct pixels in a portable PNG; no external file link.
  return fromImage(o, decoded.image, error);
}
QRectF ImageAsset::sourceRect(const SceneObject &o) {
  QRectF source(o.imageCrop.x() * size(o).width(),
                o.imageCrop.y() * size(o).height(),
                o.imageCrop.width() * size(o).width(),
                o.imageCrop.height() * size(o).height());
  if (o.imageMode == 1 && !source.isEmpty() && !o.rect.isEmpty()) {
    const qreal ratio = o.rect.width() / o.rect.height();
    if (source.width() / source.height() > ratio) {
      const auto w = source.height() * ratio;
      source.setLeft(source.x() + (source.width() - w) * o.imageFocalX);
      source.setWidth(w);
    } else {
      const auto h = source.width() / ratio;
      source.setTop(source.y() + (source.height() - h) * o.imageFocalY);
      source.setHeight(h);
    }
  }
  return source;
}
void ImageAsset::paint(QPainter &painter, const SceneObject &o) {
  if ((o.image.isNull() && o.vectorPicture.isNull()) || o.rect.isEmpty())
    return;
  const auto source = sourceRect(o);
  if (source.isEmpty())
    return;
  auto target = o.rect;
  if (o.imageMode == 0) {
    auto size = source.size();
    size.scale(o.rect.size(), Qt::KeepAspectRatio);
    target = QRectF(
        o.rect.center() - QPointF(size.width() / 2, size.height() / 2), size);
  }
  painter.save();
  painter.setClipRect(o.rect, Qt::IntersectClip);
  if (o.imageMask > 0) {
    SceneObject mask;
    mask.rect = o.rect;
    mask.shapeKind = o.imageMask == 1   ? 1
                     : o.imageMask == 2 ? 0
                     : o.imageMask == 3 ? 5
                                        : 7;
    mask.cornerRadius = o.cornerRadius > 0
                            ? o.cornerRadius
                            : qMin(o.rect.width(), o.rect.height()) * .1;
    painter.setClipPath(Shape::path(mask), Qt::IntersectClip);
  }
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  if (o.imageFormat == "svg") {
    painter.translate(target.topLeft());
    painter.scale(target.width() / source.width(),
                  target.height() / source.height());
    painter.translate(-source.topLeft());
    // QPicture replays in the target device's DPI. Our scene uses document
    // units, so cancel that implicit scale (PDF printers use a different DPI).
    painter.scale(
        qreal(o.vectorPicture.logicalDpiX()) / painter.device()->logicalDpiX(),
        qreal(o.vectorPicture.logicalDpiY()) / painter.device()->logicalDpiY());
    painter.drawPicture(QPointF(), o.vectorPicture);
  } else
    drawReduced(painter, target, adjusted(o), source);
  painter.restore();
}

QSizeF ImageAsset::size(const SceneObject &o) {
  return o.imageFormat == "svg" ? o.vectorSize : QSizeF(o.image.size());
}
void ImageAsset::copyData(SceneObject &target, const SceneObject &source) {
  target.image = source.image;
  target.imageData = source.imageData;
  target.imageId = source.imageId;
  target.imageFormat = source.imageFormat;
  target.vectorSize = source.vectorSize;
  target.vectorPicture = source.vectorPicture;
}
