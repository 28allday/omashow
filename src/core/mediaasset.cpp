#include "core/mediaasset.h"
#include "core/imageasset.h"
#include "core/hardwaredecode.h"
#include <QBuffer>
#include <QCache>
#include <QColorSpace>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <cmath>
#include <limits>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/display.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}
namespace {
const AVCodec *hardwareDecoder(AVCodecID id, AVHWDeviceType type) {
  // FFmpeg may prefer a software-only implementation (notably libdav1d for
  // AV1). Discover the implementation that actually exposes this hardware API.
  void *iterator = nullptr;
  while (const auto *candidate = av_codec_iterate(&iterator)) {
    if (candidate->id != id || !av_codec_is_decoder(candidate)) continue;
    for (int i = 0; const auto *config = avcodec_get_hw_config(candidate, i); ++i)
      if (config->device_type == type && (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) return candidate;
  }
  return nullptr;
}
class Decoder {
public:
  std::unique_ptr<QIODevice> input;
  AVFormatContext *format = nullptr;
  AVIOContext *io = nullptr;
  AVCodecContext *codec = nullptr;
  AVPacket *packet = nullptr;
  AVFrame *frame = nullptr, *download = nullptr;
  AVPixelFormat hardwareFormat = AV_PIX_FMT_NONE;
  QString hardwareDevice;
  QSet<QString> rejectedDevices;
  bool usedHardware = false;
  SwsContext *scaler = nullptr;
  std::shared_ptr<MediaAsset::Job> job;
  QElapsedTimer deadline;
  int video = -1, audio = -1;
  qreal duration = 0, currentTime = -1, nextTime = -1, origin = 0, rotation = 0;
  QImage current, next;
  bool drained = false, decodeFailed = false;
  QString error;
  ~Decoder() {
    sws_freeContext(scaler);
    av_frame_free(&frame);
    av_frame_free(&download);
    av_packet_free(&packet);
    avcodec_free_context(&codec);
    avformat_close_input(&format);
    if (io) {
      av_freep(&io->buffer);
      avio_context_free(&io);
    }
  }
  static int read(void *opaque, uint8_t *buffer, int size) {
    auto *self = static_cast<Decoder *>(opaque);
    if (interrupt(self))
      return AVERROR_EXIT;
    const auto count =
        self->input->read(reinterpret_cast<char *>(buffer), size);
    return count > 0 ? int(count) : count == 0 ? AVERROR_EOF : AVERROR(EIO);
  }
  static int64_t seek(void *opaque, int64_t offset, int whence) {
    auto *self = static_cast<Decoder *>(opaque);
    if (whence == AVSEEK_SIZE)
      return self->input->size();
    whence &= ~AVSEEK_FORCE;
    const auto base = whence == SEEK_SET   ? 0
                      : whence == SEEK_CUR ? self->input->pos()
                      : whence == SEEK_END ? self->input->size()
                                           : -1;
    if (base < 0 || offset < -base ||
        offset > std::numeric_limits<qint64>::max() - base)
      return AVERROR(EINVAL);
    const auto position = base + offset;
    return position <= self->input->size() && self->input->seek(position)
               ? position
               : AVERROR(EINVAL);
  }
  static int interrupt(void *opaque) {
    auto *self = static_cast<Decoder *>(opaque);
    return (self->job && self->job->canceled) ||
           self->deadline.elapsed() > 5000;
  }
  static int denyOpen(AVFormatContext *, AVIOContext **, const char *, int,
                      AVDictionary **) {
    return AVERROR(EACCES);
  }
  static AVPixelFormat chooseFormat(AVCodecContext *context, const AVPixelFormat *formats) {
    const auto *self = static_cast<Decoder *>(context->opaque);
    for (auto *format = formats; *format != AV_PIX_FMT_NONE; ++format)
      if (*format == self->hardwareFormat) return *format;
    // Do not silently accept software under a hardware label. A rejected
    // profile/format advances to the next adapter, then the explicit CPU path.
    return AV_PIX_FMT_NONE;
  }
  bool configureHardware(const AVCodec *implementation, const HardwareDecode::Candidate &candidate) {
    static thread_local QHash<QString, qint64> unavailableUntil;
    const auto key = candidate.key();
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (unavailableUntil.value(key) > now) return false;
    const auto type = av_hwdevice_find_type_by_name(candidate.backend.toLatin1().constData());
    for (int i = 0; const auto *config = avcodec_get_hw_config(implementation, i); ++i) {
      if (config->device_type != type || !(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) continue;
      AVBufferRef *device = nullptr;
      const auto name = candidate.device.toLocal8Bit();
      if (av_hwdevice_ctx_create(&device, type, name.isEmpty() ? nullptr : name.constData(), nullptr, 0) < 0) {
        // A failed adapter never disables the whole backend. Retry transient
        // driver/permission failures after 30 seconds, not on every frame.
        unavailableUntil.insert(key, now + 30000);
        return false;
      }
      hardwareFormat = config->pix_fmt;
      hardwareDevice = key;
      codec->hw_device_ctx = device;
      codec->opaque = this;
      codec->get_format = chooseFormat;
      return true;
    }
    return false;
  }
  bool open(const SceneObject &object,
            const std::shared_ptr<MediaAsset::Job> &task = {}, const HardwareDecode::Candidate *hardware = nullptr) {
    job = task;
    deadline.start();
    if (!object.mediaData.isEmpty()) {
      auto buffer = std::make_unique<QBuffer>();
      buffer->setData(object.mediaData);
      input = std::move(buffer);
    } else if (object.mediaReadAllowed && !object.mediaPath.isEmpty())
      input = std::make_unique<QFile>(object.mediaPath);
    else {
      error = "Linked media needs approval in Media preflight.";
      return false;
    }
    if (!input->open(QIODevice::ReadOnly)) {
      error = input->errorString();
      return false;
    }
    auto *buffer = static_cast<unsigned char *>(av_malloc(32768));
    if (!buffer) {
      error = "Not enough memory to read media.";
      return false;
    }
    io = avio_alloc_context(buffer, 32768, 0, this, read, nullptr, seek);
    if (!io) {
      av_free(buffer);
      error = "Could not create media reader.";
      return false;
    }
    format = avformat_alloc_context();
    if (!format) {
      error = "Could not create media decoder.";
      return false;
    }
    format->pb = io;
    format->flags |= AVFMT_FLAG_CUSTOM_IO;
    format->io_open = denyOpen;
    format->interrupt_callback = {interrupt, this};
    AVDictionary *options = nullptr;
    av_dict_set(&options, "format_whitelist",
                "mov,matroska,webm,wav,mp3,flac,ogg", 0);
    av_dict_set(&options, "protocol_whitelist", "", 0);
    av_dict_set(&options, "probesize", "8388608", 0);
    av_dict_set(&options, "analyzeduration", "3000000", 0);
    const int opened = avformat_open_input(&format, nullptr, nullptr, &options);
    av_dict_free(&options);
    if (opened < 0 || avformat_find_stream_info(format, nullptr) < 0) {
      error = "Unsupported, damaged or timed-out media. Use MP4/MOV, WebM/MKV, "
              "WAV, MP3, FLAC or Ogg.";
      return false;
    }
    video = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    audio = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (video >= 0 &&
        (format->streams[video]->disposition & AV_DISPOSITION_ATTACHED_PIC))
      video = -1;
    if (video < 0 && audio < 0) {
      error = "This file has no supported audio or video stream.";
      return false;
    }
    duration = format->duration == AV_NOPTS_VALUE
                   ? 0
                   : qreal(format->duration) / AV_TIME_BASE;
    if (!std::isfinite(duration) || duration <= 0 || duration > 86400) {
      error = "Media must have a known duration between zero and 24 hours.";
      return false;
    }
    if (video < 0)
      return true;
    auto *stream = format->streams[video];
    auto *parameters = stream->codecpar;
    if (parameters->width <= 0 || parameters->height <= 0 ||
        parameters->width > 4096 || parameters->height > 4096) {
      error = "Video dimensions must be at most 4096 × 4096.";
      return false;
    }
    const AVCodec *implementation = hardware
        ? hardwareDecoder(parameters->codec_id, av_hwdevice_find_type_by_name(hardware->backend.toLatin1().constData()))
        : avcodec_find_decoder(parameters->codec_id);
    if (!implementation) {
      error = "The video codec is unavailable.";
      return false;
    }
    codec = avcodec_alloc_context3(implementation);
    if (!codec || avcodec_parameters_to_context(codec, parameters) < 0) {
      error = "Could not configure video decoder.";
      return false;
    }
    if (hardware && !configureHardware(implementation, *hardware)) return false;
    codec->thread_count = 2;
    codec->max_pixels = 4096LL * 4096;
    if (avcodec_open2(codec, implementation, nullptr) < 0) {
      error = "The video codec could not be opened.";
      return false;
    }
    packet = av_packet_alloc();
    frame = av_frame_alloc();
    download = av_frame_alloc();
    if (!packet || !frame || !download) {
      error = "Not enough memory to decode video.";
      return false;
    }
    origin = stream->start_time == AV_NOPTS_VALUE
                 ? 0
                 : stream->start_time * av_q2d(stream->time_base);
    const auto *side = av_packet_side_data_get(parameters->coded_side_data,
                                               parameters->nb_coded_side_data,
                                               AV_PKT_DATA_DISPLAYMATRIX);
    if (side && side->size >= 9 * sizeof(int32_t))
      rotation = -av_display_rotation_get(
          reinterpret_cast<const int32_t *>(side->data));
    if (!std::isfinite(rotation))
      rotation = 0;
    return true;
  }
  bool decodeNext(QImage &image, qreal &seconds) {
    decodeFailed = true;
    for (int attempts = 0; attempts < 20000 && !interrupt(this); ++attempts) {
      int result = avcodec_receive_frame(codec, frame);
      if (result == 0) {
        AVFrame *pixels = frame;
        if (frame->format == hardwareFormat && hardwareFormat != AV_PIX_FMT_NONE) {
          av_frame_unref(download);
          if (av_hwframe_transfer_data(download, frame, 0) < 0 || av_frame_copy_props(download, frame) < 0) return false;
          pixels = download;
          usedHardware = true;
        }
        if (pixels->color_trc == AVCOL_TRC_SMPTE2084 ||
            pixels->color_trc == AVCOL_TRC_ARIB_STD_B67 ||
            pixels->color_primaries == AVCOL_PRI_BT2020) {
          error = "HDR and wide-gamut video need conversion to SDR Rec.709 "
                  "before insertion.";
          return false;
        }
        const int64_t pts = pixels->best_effort_timestamp;
        seconds =
            pts == AV_NOPTS_VALUE
                ? qMax(0.0, nextTime + .04)
                : pts * av_q2d(format->streams[video]->time_base) - origin;
        if (pixels->width <= 0 || pixels->height <= 0 || pixels->width > 4096 ||
            pixels->height > 4096)
          return false;
        scaler = sws_getCachedContext(
            scaler, pixels->width, pixels->height, AVPixelFormat(pixels->format),
            pixels->width, pixels->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr,
            nullptr, nullptr);
        if (!scaler)
          return false;
        const int space = pixels->colorspace == AVCOL_SPC_BT709 ? SWS_CS_ITU709
                          : pixels->colorspace == AVCOL_SPC_BT2020_NCL
                              ? SWS_CS_BT2020
                              : SWS_CS_ITU601;
        const auto *coefficients = sws_getCoefficients(space);
        sws_setColorspaceDetails(scaler, coefficients,
                                 pixels->color_range == AVCOL_RANGE_JPEG,
                                 coefficients, 1, 0, 1 << 16, 1 << 16);
        // 8K is the largest film a deck plays; a file claiming more is refused
        // rather than handed a gigabyte of pixels.
        if (pixels->width <= 0 || pixels->height <= 0 ||
            qint64(pixels->width) * pixels->height > 8192LL * 8192)
          return false;
        image = QImage(pixels->width, pixels->height, QImage::Format_RGBA8888);
        if (image.isNull())
          return false;
        uint8_t *planes[] = {image.bits(), nullptr, nullptr, nullptr};
        int strides[] = {int(image.bytesPerLine()), 0, 0, 0};
        if (sws_scale(scaler, pixels->data, pixels->linesize, 0, pixels->height,
                      planes, strides) <= 0)
          return false;
        const AVRational aspect =
            av_guess_sample_aspect_ratio(format, format->streams[video], pixels);
        if (aspect.num > 0 && aspect.den > 0 && aspect.num != aspect.den) {
          const int width = qRound(image.width() * av_q2d(aspect));
          if (width <= 0 || width > 4096)
            return false;
          image = image.scaled(width, image.height(), Qt::IgnoreAspectRatio,
                               Qt::SmoothTransformation);
        }
        if (!qFuzzyIsNull(rotation))
          image = image.transformed(QTransform().rotate(rotation),
                                    Qt::SmoothTransformation);
        image.setColorSpace(QColorSpace::SRgb);
        av_frame_unref(frame);
        av_frame_unref(download);
        decodeFailed = false;
        return true;
      }
      if (result == AVERROR_EOF) { decodeFailed = false; return false; }
      if (result != AVERROR(EAGAIN) || drained)
        return false;
      do {
        av_packet_unref(packet);
        result = av_read_frame(format, packet);
      } while (result >= 0 && packet->stream_index != video &&
               !interrupt(this));
      if (interrupt(this))
        return false;
      if (result < 0) {
        if (result != AVERROR_EOF)
          return false;
        drained = true;
        avcodec_send_packet(codec, nullptr);
      } else if (avcodec_send_packet(codec, packet) < 0)
        return false;
    }
    return false;
  }
  QImage at(qreal seconds) {
    if (video < 0 || !codec)
      return {};
    deadline.restart();
    seconds = qBound(0.0, seconds, qMax(0.0, duration - .000001));
    if (currentTime >= 0 &&
        (seconds + 1e-7 < currentTime || seconds - currentTime > 2)) {
      const auto *stream = format->streams[video];
      const int64_t timestamp =
          qRound64((seconds + origin) / av_q2d(stream->time_base));
      if (av_seek_frame(format, video, timestamp, AVSEEK_FLAG_BACKWARD) < 0)
        return {};
      avcodec_flush_buffers(codec);
      av_packet_unref(packet);
      current = {};
      next = {};
      currentTime = nextTime = -1;
      drained = false;
    }
    if (current.isNull()) {
      if (!decodeNext(current, currentTime))
        return {};
      decodeNext(next, nextTime);
    }
    while (!next.isNull() && nextTime <= seconds + 1e-7 && !interrupt(this)) {
      current = next;
      currentTime = nextTime;
      next = {};
      nextTime = -1;
      decodeNext(next, nextTime);
    }
    // A decoder/transfer failure after an earlier good frame must trigger the
    // next device (or software), rather than freeze on that earlier frame.
    return decodeFailed ? QImage() : current;
  }
};
QString hashFile(QFile &file, const std::shared_ptr<MediaAsset::Job> &job,
                 QByteArray *bytes) {
  QCryptographicHash hash(QCryptographicHash::Sha256);
  while (!file.atEnd()) {
    if (job && job->canceled)
      return {};
    auto chunk = file.read(1024 * 1024);
    if (chunk.isEmpty())
      return {};
    hash.addData(chunk);
    if (bytes)
      bytes->append(chunk);
    if (job)
      job->progress = qMax(job->progress.load(),
                           int(file.pos() * 70 / qMax(qint64(1), file.size())));
  }
  return QString::fromLatin1(hash.result().toHex());
}
QImage audioPoster(const QString &name) {
  QImage image(640, 160, QImage::Format_RGBA8888);
  image.fill(QColor("#172434"));
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(QPen(QColor("#82cbe8"), 5, Qt::SolidLine, Qt::RoundCap));
  for (int i = 0; i < 11; ++i) {
    const int h = 15 + int(25 * std::abs(std::sin(i * 1.7)));
    painter.drawLine(35 + i * 9, 80 - h, 35 + i * 9, 80 + h);
  }
  painter.setPen(Qt::white);
  QFont font("Inter");
  font.setPixelSize(25);
  painter.setFont(font);
  painter.drawText(QRect(160, 20, 450, 120),
                   Qt::AlignVCenter | Qt::TextWordWrap, name.left(80));
  return image;
}
} // namespace
MediaAsset::Result MediaAsset::fromFile(const QString &path, bool embed,
                                        const std::shared_ptr<Job> &job,
                                        const QString &displayName) {
  Result result;
  QFileInfo info(path);
  const auto fail = [&](const QString &error) {
    result.error = error;
    return result;
  };
  if (!info.isFile() || info.isSymLink() || info.size() <= 0 ||
      info.size() > 2LL * 1024 * 1024 * 1024)
    return fail("Choose a regular local media file up to 2 GiB.");
  if (embed && info.size() > embedLimit)
    return fail("Embedding supports clips up to 64 MiB. Choose Link for this "
                "file, or compress it first.");
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return fail(file.errorString());
  SceneObject object;
  object.type = ObjectType::Media;
  object.mediaName = displayName.isEmpty() ? info.fileName() : displayName;
  object.mediaBytes = info.size();
  object.mediaModified = info.lastModified().toMSecsSinceEpoch();
  object.mediaId = hashFile(file, job, embed ? &object.mediaData : nullptr);
  if (object.mediaId.isEmpty())
    return fail(job && job->canceled ? "Media import canceled."
                                     : "The media file could not be read.");
  if (!embed)
    object.mediaPath = info.canonicalFilePath();
  object.mediaReadAllowed = true;
  Decoder decoder;
  if (!decoder.open(object, job))
    return fail(decoder.error);
  object.mediaDuration = object.mediaTrimEnd = decoder.duration;
  object.mediaVideo = decoder.video >= 0;
  object.mediaAudio = decoder.audio >= 0;
  object.mediaContainer = QString::fromUtf8(decoder.format->iformat->name);
  QStringList codecs;
  for (const int index : {decoder.video, decoder.audio})
    if (index >= 0)
      codecs.append(QString::fromUtf8(avcodec_get_name(
          decoder.format->streams[index]->codecpar->codec_id)));
  object.mediaCodec = codecs.join(" / ");
  QImage poster =
      object.mediaVideo ? decoder.at(0) : audioPoster(object.mediaName);
  if (poster.isNull())
    return fail(decoder.error.isEmpty()
                    ? "The first video frame could not be decoded."
                    : decoder.error);
  SceneObject picture;
  QString error;
  if (!ImageAsset::fromImage(picture, poster, &error))
    return fail(error);
  ImageAsset::copyData(object, picture);
  info.refresh();
  if (info.size() != object.mediaBytes ||
      info.lastModified().toMSecsSinceEpoch() != object.mediaModified)
    return fail("The media file changed during import. Try again.");
  if (job && job->canceled)
    return fail("Media import canceled.");
  object.mediaReadAllowed = false;
  result.object = object;
  if (job)
    job->progress = 100;
  return result;
}
bool MediaAsset::validate(const SceneObject &o, QString *error) {
  const auto fail = [&](const QString &text) {
    if (error)
      *error = text;
    return false;
  };
  if (o.type != ObjectType::Media)
    return true;
  if (!QRegularExpression("^[0-9a-f]{64}$").match(o.mediaId).hasMatch())
    return fail("Invalid media asset reference.");
  if (!std::isfinite(o.mediaDuration) || o.mediaDuration <= 0 ||
      o.mediaDuration > 86400 || !std::isfinite(o.mediaTrimStart) ||
      !std::isfinite(o.mediaTrimEnd) || o.mediaTrimStart < 0 ||
      o.mediaTrimEnd <= o.mediaTrimStart || o.mediaTrimEnd > o.mediaDuration ||
      o.mediaLoops < 1 || o.mediaLoops > 100 || !std::isfinite(o.mediaVolume) ||
      o.mediaVolume < 0 || o.mediaVolume > 1 ||
      (!o.mediaVideo && !o.mediaAudio))
    return fail("Invalid media playback settings.");
  if (o.mediaBytes <= 0 || o.mediaBytes > 2LL * 1024 * 1024 * 1024)
    return fail("Invalid media file size.");
  if (o.mediaPath.isEmpty()) {
    if (o.mediaData.size() != o.mediaBytes || o.mediaData.size() > embedLimit ||
        ImageAsset::identity(o.mediaData) != o.mediaId)
      return fail("Embedded media is missing or damaged.");
  } else if (!o.mediaData.isEmpty() || !QDir::isAbsolutePath(o.mediaPath) ||
             o.mediaPath.contains(QChar(0)))
    return fail("Invalid linked media location.");
  if (o.imageId.isEmpty() || o.imageFormat != "png")
    return fail("Media has no poster image.");
  if (o.mediaPath.isEmpty()) {
    Decoder decoder;
    if (!decoder.open(o))
      return fail(decoder.error);
    if (std::abs(decoder.duration - o.mediaDuration) > .05 ||
        (decoder.video >= 0) != o.mediaVideo ||
        (decoder.audio >= 0) != o.mediaAudio)
      return fail("Media metadata does not match the embedded file.");
  }
  return true;
}
QString MediaAsset::linkState(const SceneObject &o, bool authorized) {
  if (o.mediaPath.isEmpty())
    return QStringLiteral("Embedded · portable");
  if (!authorized)
    return QStringLiteral("Approval required");
  const QFileInfo info(o.mediaPath);
  if (!info.isFile() || info.isSymLink())
    return QStringLiteral("Missing");
  if (info.size() != o.mediaBytes ||
      info.lastModified().toMSecsSinceEpoch() != o.mediaModified)
    return QStringLiteral("Changed · relink to review");
  return QStringLiteral("Linked · keep source file");
}
QImage MediaAsset::frameAt(const SceneObject &o, qreal seconds, const std::shared_ptr<Job> &job,
                          bool hardware, QString *decoderBackend) {
  if (decoderBackend) *decoderBackend = "poster";
  if (job && job->canceled) return o.image;
  if (!o.mediaVideo || !std::isfinite(seconds) || seconds < 0) return o.image;
  if (!o.mediaPath.isEmpty() && linkState(o, o.mediaReadAllowed) != QStringLiteral("Linked · keep source file")) return o.image;
  // Four decoders per thread; cached entries never confer file permission.
  thread_local QCache<QString, Decoder> decoders(4);
  const auto preference = hardware ? qEnvironmentVariable("OMASHOW_VIDEO_DECODER", "auto") : QStringLiteral("software");
  const auto device = hardware ? qEnvironmentVariable("OMASHOW_VIDEO_DEVICE") : QString();
  const auto key = o.mediaId + '/' + o.mediaPath + '/' + preference + '/' + device;
  QSet<QString> rejected;
  if (auto *decoder = decoders.object(key)) {
    decoder->job = job;
    const auto image = decoder->at(seconds);
    if (job && job->canceled) { decoders.remove(key); return o.image; }
    if (!image.isNull()) {
      if (decoderBackend) *decoderBackend = decoder->usedHardware ? decoder->hardwareDevice : QStringLiteral("software");
      return image;
    }
    rejected = decoder->rejectedDevices;
    if (!decoder->hardwareDevice.isEmpty()) rejected.insert(decoder->hardwareDevice);
    decoders.remove(key);
  }

  // Read the actual stream/codec before selecting adapters. Its metadata and
  // each successful device initialization are insufficient on their own: a
  // candidate must also decode this stream's profile and download a frame.
  auto software = std::make_unique<Decoder>();
  if (!software->open(o, job)) return o.image;
  QStringList supported;
  if (hardware && software->codec) {
    if (hardwareDecoder(software->codec->codec_id, AV_HWDEVICE_TYPE_VAAPI)) supported.append("vaapi");
    if (hardwareDecoder(software->codec->codec_id, AV_HWDEVICE_TYPE_CUDA)) supported.append("cuda");
  }
  const auto plan = HardwareDecode::candidates(supported, hardware ? HardwareDecode::renderNodes() : QStringList(),
                                              hardware ? HardwareDecode::cudaDeviceCount() : 0, preference, device);
  std::unique_ptr<Decoder> chosen;
  QImage image;
  HardwareDecode::firstWorking(plan, [&](const HardwareDecode::Candidate &candidate) {
    if ((job && job->canceled) || rejected.contains(candidate.key())) return false;
    if (candidate.backend == "software") {
      image = software->at(seconds);
      if (image.isNull()) return false;
      chosen = std::move(software);
      return true;
    }
    auto attempt = std::make_unique<Decoder>();
    if (attempt->open(o, job, &candidate)) image = attempt->at(seconds);
    if (!image.isNull() && attempt->usedHardware) { chosen = std::move(attempt); return true; }
    image = {};
    rejected.insert(candidate.key());
    return false;
  });
  if (!chosen || (job && job->canceled)) return o.image;
  chosen->rejectedDevices = rejected;
  const auto selected = chosen->usedHardware ? chosen->hardwareDevice : QStringLiteral("software");
  if (decoderBackend) *decoderBackend = selected;
  if (hardware && qEnvironmentVariableIsSet("OMASHOW_PERF_LOG")) qInfo().noquote() << "OmaShow video decoder:" << selected;
  decoders.insert(key, chosen.release());
  return image;
}

qreal MediaAsset::playbackDuration(const SceneObject &o) {
  return (o.mediaTrimEnd - o.mediaTrimStart) * o.mediaLoops;
}
void MediaAsset::evaluate(SceneObject &o, qreal elapsed, qreal cueDuration) {
  const qreal span = o.mediaTrimEnd - o.mediaTrimStart;
  o.mediaActive = elapsed >= 0 && elapsed < cueDuration && span > 0;
  if (elapsed < 0 || span <= 0) {
    o.mediaPosition = -1;
    return;
  }
  o.mediaPosition = elapsed >= cueDuration
                        ? qMax(o.mediaTrimStart, o.mediaTrimEnd - .000001)
                        : o.mediaTrimStart + std::fmod(elapsed, span);
}

void MediaAsset::copySource(SceneObject &target, const SceneObject &source) {
  target.type = ObjectType::Media;
  target.mediaData = source.mediaData;
  target.mediaId = source.mediaId;
  target.mediaPath = source.mediaPath;
  target.mediaName = source.mediaName;
  target.mediaContainer = source.mediaContainer;
  target.mediaCodec = source.mediaCodec;
  target.mediaBytes = source.mediaBytes;
  target.mediaModified = source.mediaModified;
  target.mediaDuration = source.mediaDuration;
  target.mediaVideo = source.mediaVideo;
  target.mediaAudio = source.mediaAudio;
  target.mediaReadAllowed = false;
  target.mediaPosition = -1;
  target.mediaActive = false;
  ImageAsset::copyData(target, source);
}

qint64 MediaAsset::embeddedBytes(const Document &document) {
  QHash<QString, qint64> sizes;
  const auto collectOne = [&sizes](const SceneObject &o) {
    if (!o.imageId.isEmpty())
      sizes[o.imageId] = o.imageData.size();
    if (!o.mediaId.isEmpty() && o.mediaPath.isEmpty())
      sizes[o.mediaId] = o.mediaData.size();
  };
  const auto collect = [&](const QVector<SceneObject> &objects) {
    for (const auto &o : objects) {
      collectOne(o);
      if (o.mediaOriginal)
        collectOne(*o.mediaOriginal);
      if (o.imageOriginal)
        collectOne(*o.imageOriginal);
    }
  };
  for (const auto &slide : document.slides)
    collect(slide.objects);
  for (const auto &master : document.masters)
    collect(master.objects);
  for (const auto &layout : document.layouts)
    collect(layout.placeholders);
  for (const auto &style : document.objectStyles)
    collect({style.appearance});
  qint64 total = 0;
  for (const auto size : sizes)
    total += size;
  return total;
}
