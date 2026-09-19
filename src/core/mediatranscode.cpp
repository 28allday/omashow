#include "core/mediatranscode.h"
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <cmath>

MediaAsset::Result
MediaTranscode::preview(const SceneObject &source, int preset,
                        const std::shared_ptr<MediaAsset::Job> &job) {
  MediaAsset::Result result;
  const auto fail = [&](const QString &message) {
    result.error = message;
    return result;
  };
  if (preset < 0 || preset > 1 || source.type != ObjectType::Media)
    return fail("Choose a media compression preset.");
  if (!source.mediaPath.isEmpty() &&
      MediaAsset::linkState(source, source.mediaReadAllowed) !=
          QStringLiteral("Linked · keep source file"))
    return fail("Approve or relink the source file before optimising it.");
  QTemporaryDir temporary;
  if (!temporary.isValid())
    return fail("Could not create a media workspace.");
  QString input = source.mediaPath;
  if (input.isEmpty()) {
    input = temporary.filePath("source.media");
    QFile file(input);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(source.mediaData) != source.mediaData.size())
      return fail("Could not prepare the source media.");
  }
  const QString output = temporary.filePath("preview.mp4");
  QStringList args = {"-v",
                      "error",
                      "-nostdin",
                      "-y",
                      "-protocol_whitelist",
                      "file,pipe",
                      "-format_whitelist",
                      "mov,matroska,webm,wav,mp3,flac,ogg",
                      "-i",
                      input,
                      "-map",
                      "0:v:0?",
                      "-map",
                      "0:a:0?",
                      "-sn",
                      "-dn",
                      "-map_metadata",
                      "-1",
                      "-map_chapters",
                      "-1"};
  if (source.mediaVideo) {
    const QString width = preset == 0 ? "1920" : "1280",
                  height = preset == 0 ? "1080" : "720";
    args << "-vf"
         << QString("scale=w='min(%1,iw)':h='min(%2,ih)':force_original_aspect_"
                    "ratio=decrease:force_divisible_by=2")
                .arg(width, height)
         << "-c:v" << "libx264" << "-preset" << "veryfast" << "-crf"
         << (preset == 0 ? "23" : "28") << "-pix_fmt" << "yuv420p";
  } else
    args << "-vn";
  args << "-c:a" << "aac" << "-b:a" << (preset == 0 ? "128k" : "96k")
       << "-threads" << "2" << "-movflags" << "+faststart" << "-progress"
       << "pipe:1" << "-f" << "mp4" << output;
  QProcess process;
  process.start("ffmpeg", args);
  if (!process.waitForStarted(5000))
    return fail("FFmpeg is unavailable. Install ffmpeg to compress media.");
  QElapsedTimer idle;
  idle.start();
  QByteArray pending, error;
  while (process.state() != QProcess::NotRunning) {
    process.waitForFinished(100);
    const auto bytes = process.readAllStandardOutput();
    if (!bytes.isEmpty())
      idle.restart();
    pending += bytes;
    while (pending.contains('\n')) {
      const int end = pending.indexOf('\n');
      const auto line = pending.left(end);
      pending.remove(0, end + 1);
      if (line.startsWith("out_time_us="))
        job->progress = qBound(0,
                               int(line.mid(12).toDouble() / 1000000.0 /
                                   source.mediaDuration * 90),
                               90);
    }
    error += process.readAllStandardError();
    if (error.size() > 8192)
      error = error.right(8192);
    const bool tooLarge = QFileInfo(output).size() > MediaAsset::embedLimit;
    if (job->canceled || tooLarge || idle.elapsed() > 120000) {
      process.kill();
      process.waitForFinished(5000);
      return fail(job->canceled ? "Media compression canceled."
                  : tooLarge ? "The compressed clip exceeds 64 MiB. Choose a "
                               "smaller source or stronger compression."
                             : "The encoder stopped responding.");
    }
  }
  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    return fail("Compression failed: " + QString::fromUtf8(error).trimmed());
  result = MediaAsset::fromFile(output, true, job,
                                QFileInfo(source.mediaName).completeBaseName() +
                                    QStringLiteral("-compressed.mp4"));
  if (!result.ok())
    return result;
  if (std::abs(result.object.mediaDuration - source.mediaDuration) > .15)
    return fail(
        "The encoded duration changed unexpectedly. The preview was rejected.");
  result.object.mediaName = QFileInfo(source.mediaName).completeBaseName() +
                            QStringLiteral("-compressed.mp4");
  return result;
}
