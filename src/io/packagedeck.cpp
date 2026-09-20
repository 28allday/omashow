#include "io/packagedeck.h"
#include "core/design.h"
#include "core/mediaasset.h"
#include "io/bundle.h"
#include "io/zip.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QSet>

namespace {

QString describeBytes(qint64 bytes) {
    if (bytes >= 1024 * 1024) return QStringLiteral("%1 MB").arg(bytes / double(1024 * 1024), 0, 'f', 1);
    if (bytes >= 1024) return QStringLiteral("%1 kB").arg(bytes / 1024);
    return QStringLiteral("%1 bytes").arg(bytes);
}

QString uniqueName(const QString &name, QSet<QString> &taken) {
    QFileInfo info(name.isEmpty() ? QStringLiteral("file") : name);
    QString candidate = info.fileName();
    for (int n = 2; taken.contains(candidate); ++n)
        candidate = info.completeBaseName() + QStringLiteral("-%1").arg(n) +
                    (info.suffix().isEmpty() ? QString() : '.' + info.suffix());
    taken.insert(candidate);
    return candidate;
}

} // namespace

Package::Report Package::write(const Document &document, const QString &path,
                               const QHash<QString, QString> &approved,
                               const std::shared_ptr<Workers::Job> &job) {
    Report report;
    const auto fail = [&report](const QString &message) {
        report.error = message;
        return report;
    };
    if (document.slides.isEmpty()) return fail(QStringLiteral("The deck has no slides."));
    if (path.isEmpty()) return fail(QStringLiteral("Choose where to write the package."));

    Document packaged = document;
    QVector<Zip::Entry> entries;
    QStringList copied, embedded, left, fonts;
    QSet<QString> names;
    qint64 assetBytes = 0;

    for (int i = 0; i < packaged.slides.size(); ++i) {
        for (auto &object : packaged.slides[i].objects) {
            if (job && job->canceled) return fail(QStringLiteral("Packaging canceled."));
            const QString linked = object.type == ObjectType::Media ? object.mediaPath
                                                                    : object.dataSource.path;
            if (linked.isEmpty()) continue;
            const QFileInfo info(linked);
            const bool readable = info.isReadable() && info.isFile();
            const bool allowed = object.type != ObjectType::Media ||
                                 approved.value(linked) == object.mediaId;
            if (!readable) {
                left.append(QStringLiteral("%1 — the file is not there any more")
                                .arg(info.fileName()));
                continue;
            }
            if (!allowed) {
                left.append(QStringLiteral("%1 — approve it in Media preflight to include it")
                                .arg(info.fileName()));
                continue;
            }
            QFile source(linked);
            if (!source.open(QIODevice::ReadOnly)) {
                left.append(QStringLiteral("%1 — %2").arg(info.fileName(), source.errorString()));
                continue;
            }
            const QByteArray bytes = source.readAll();
            assetBytes += bytes.size();
            const QString name = uniqueName(info.fileName(), names);
            entries.append({QStringLiteral("assets/") + name, bytes, false});
            copied.append(QStringLiteral("assets/%1 · %2 · %3")
                              .arg(name, describeBytes(bytes.size()),
                                   QString::fromUtf8(QCryptographicHash::hash(
                                                         bytes, QCryptographicHash::Sha256)
                                                         .toHex()
                                                         .left(16))));
            // Film small enough to live inside the deck travels inside it, so
            // the package opens complete on another computer.
            if (object.type == ObjectType::Media && bytes.size() <= MediaAsset::embedLimit) {
                const auto result = MediaAsset::fromFile(linked, true,
                                                         std::make_shared<MediaAsset::Job>(),
                                                         object.mediaName);
                if (result.ok()) {
                    auto replacement = result.object;
                    replacement.id = object.id;
                    replacement.rect = object.rect;
                    replacement.rotation = object.rotation;
                    replacement.opacity = object.opacity;
                    replacement.groups = object.groups;
                    replacement.placeholderId = object.placeholderId;
                    replacement.overrides = object.overrides;
                    replacement.mediaTrimStart = object.mediaTrimStart;
                    replacement.mediaTrimEnd = qMin(object.mediaTrimEnd, result.object.mediaDuration);
                    replacement.mediaVolume = object.mediaVolume;
                    replacement.mediaLoops = object.mediaLoops;
                    replacement.altTitle = object.altTitle;
                    replacement.altText = object.altText;
                    object = replacement;
                    embedded.append(info.fileName());
                }
            }
        }
    }

    for (int i = 0; i < packaged.slides.size(); ++i)
        for (const auto &object : Design::resolve(packaged, i).objects)
            if (object.type == ObjectType::Text || object.type == ObjectType::Table ||
                object.type == ObjectType::Chart) {
                const auto family = object.fontFamily;
                if (family.isEmpty() || fonts.contains(family)) continue;
                fonts.append(family);
            }
    fonts.sort();

    const QByteArray deck = Bundle::toBytes(packaged);
    entries.prepend({QStringLiteral("deck.omashow"), deck, false});

    QStringList manifest;
    manifest << QStringLiteral("OmaShow package")
             << QStringLiteral("Deck: deck.omashow · %1 slides · format %2")
                    .arg(packaged.slides.size()).arg(Bundle::kFormatVersion)
             << QStringLiteral("Deck size: %1").arg(describeBytes(deck.size()))
             << QString();
    manifest << QStringLiteral("Typefaces the deck asks for:");
    for (const auto &family : fonts)
        manifest << QStringLiteral("  %1%2").arg(family,
                                                 QFontDatabase::hasFamily(family)
                                                     ? QString()
                                                     : QStringLiteral(" — not installed here"));
    manifest << QStringLiteral("  Fonts are not copied: they are licensed to the computer "
                               "they are installed on, not to the deck.")
             << QString();
    if (!embedded.isEmpty()) {
        manifest << QStringLiteral("Brought inside the deck:");
        for (const auto &line : embedded) manifest << QStringLiteral("  ") + line;
        manifest << QString();
    }
    manifest << (copied.isEmpty() ? QStringLiteral("No linked files to copy.")
                                  : QStringLiteral("Copies of what the deck links to:"));
    for (const auto &line : copied) manifest << QStringLiteral("  ") + line;
    if (!left.isEmpty()) {
        manifest << QString() << QStringLiteral("Left out:");
        for (const auto &line : left) manifest << QStringLiteral("  ") + line;
    }
    manifest << QString()
             << QStringLiteral("Nothing on the original computer was changed by packaging.");
    entries.append({QStringLiteral("manifest.txt"),
                    (manifest.join('\n') + '\n').toUtf8(), true});

    if (job && job->canceled) return fail(QStringLiteral("Packaging canceled."));
    const QByteArray archive = Zip::write(entries);
    QString error;
    if (!Workers::write(path, archive, &error, job))
        return fail(error.isEmpty() ? QStringLiteral("The package could not be written.") : error);
    report.ok = true;
    report.bytes = archive.size();
    report.lines = manifest;
    return report;
}
