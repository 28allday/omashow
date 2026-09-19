#include "core/imageoptimisation.h"
#include "core/imageasset.h"
#include <QBuffer>
#include <QImageWriter>
MediaAsset::Result
ImageOptimisation::preview(const SceneObject &source, int preset,
                           const std::shared_ptr<MediaAsset::Job> &job) {
  MediaAsset::Result result;
  const auto fail = [&](const QString &message) {
    result.error = message;
    return result;
  };
  if (preset < 0 || preset > 2 || source.image.isNull() ||
      source.imageFormat == "svg")
    return fail("Choose a raster picture and a compression preset.");
  if (job->canceled)
    return fail("Picture optimisation canceled.");
  QImage image = source.image;
  const int longest = preset == 1 ? 1280 : 1920;
  if (image.width() > longest || image.height() > longest)
    image = image.scaled(longest, longest, Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
  job->progress = 40;
  if (job->canceled)
    return fail("Picture optimisation canceled.");
  if (preset < 2 && image.hasAlphaChannel()) {
    image = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
      const auto *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
      for (int x = 0; x < image.width(); ++x)
        if (qAlpha(row[x]) != 255)
          return fail(
              "This picture has transparency. Choose PNG to preserve it.");
    }
  }
  QByteArray data;
  QBuffer buffer(&data);
  buffer.open(QIODevice::WriteOnly);
  QImageWriter writer(&buffer, preset == 2 ? "png" : "jpeg");
  if (preset < 2)
    writer.setQuality(preset == 0 ? 85 : 70);
  if (!writer.write(image))
    return fail(writer.errorString());
  job->progress = 85;
  if (job->canceled)
    return fail("Picture optimisation canceled.");
  if (!ImageAsset::decode(result.object, data, &result.error))
    return result;
  job->progress = 100;
  return result;
}
