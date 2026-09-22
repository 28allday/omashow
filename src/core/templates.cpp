#include "core/templates.h"
#include "core/design.h"
#include "core/starter.h"
#include "io/bundle.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QFileInfo>
#include <QFontDatabase>
#include <QStandardPaths>

namespace {

constexpr qint64 kLargest = 128LL * 1024 * 1024;

QString aspectOf(const QSizeF &size) {
    if (size.isEmpty()) return QStringLiteral("—");
    const qreal ratio = size.width() / size.height();
    if (qAbs(ratio - 16.0 / 9) < .02) return QStringLiteral("16:9");
    if (qAbs(ratio - 4.0 / 3) < .02) return QStringLiteral("4:3");
    if (qAbs(ratio - 1) < .02) return QStringLiteral("Square");
    if (ratio < 1) return QStringLiteral("Portrait");
    return QStringLiteral("%1:%2").arg(size.width()).arg(size.height());
}

QVariantMap describeDocument(const Document &document, const QString &id, const QString &name,
                             const QString &source) {
    QStringList fonts, missing;
    for (auto it = document.theme.fonts.cbegin(); it != document.theme.fonts.cend(); ++it)
        if (!fonts.contains(it.value())) fonts.append(it.value());
    for (const auto &slide : document.slides)
        for (const auto &object : slide.objects)
            if (object.type == ObjectType::Text && !object.fontFamily.isEmpty() &&
                !fonts.contains(object.fontFamily))
                fonts.append(object.fontFamily);
    fonts.sort();
    for (const auto &family : fonts)
        if (!QFontDatabase::hasFamily(family)) missing.append(family);
    QStringList colours;
    for (const auto &token : {"background", "foreground", "muted", "accent"})
        colours.append(document.theme.colors.value(QString::fromLatin1(token)).name(QColor::HexRgb));
    QStringList layouts;
    for (const auto &layout : document.layouts) layouts.append(layout.name);
    return QVariantMap{{"id", id},
                       {"name", name},
                       {"source", source},
                       {"theme", document.theme.name},
                       {"slides", document.slides.size()},
                       {"masters", document.masters.size()},
                       {"layouts", layouts},
                       {"fonts", fonts},
                       {"missingFonts", missing},
                       {"colors", colours},
                       {"aspect", aspectOf(document.size)},
                       {"size", QStringLiteral("%1 × %2").arg(document.size.width())
                                    .arg(document.size.height())}};
}

QString builtInName(int theme, int layout) {
    static const QStringList themes{QStringLiteral("Midnight"), QStringLiteral("Paper"),
                                    QStringLiteral("Grove")};
    static const QStringList openings{QStringLiteral("title slide"),
                                      QStringLiteral("title and body"),
                                      QStringLiteral("blank slide")};
    return QStringLiteral("%1 · %2").arg(themes.value(theme), openings.value(layout));
}

} // namespace

QString Templates::folder() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
           QStringLiteral("/templates");
}

QStringList Templates::exampleFolders() {
    QStringList folders;
    // Set, it is the only place examples come from — as OMASHOW_DICTIONARIES is
    // for spelling — so what is installed on this computer cannot leak in.
    if (qEnvironmentVariableIsSet("OMASHOW_EXAMPLES")) {
        const auto only = QString::fromLocal8Bit(qgetenv("OMASHOW_EXAMPLES"));
        return QDir(only).exists() ? QStringList{only} : QStringList{};
    }
    for (const auto &data : QStandardPaths::standardLocations(QStandardPaths::AppDataLocation))
        folders.append(data + QStringLiteral("/examples"));
    folders.append(QStringLiteral("/usr/share/omashow/examples"));
    QStringList kept;
    for (const auto &path : folders)
        if (!kept.contains(path) && QDir(path).exists()) kept.append(path);
    return kept;
}

QVariantList Templates::all() {
    QVariantList rows;
    for (int theme = 0; theme < 3; ++theme)
        for (int layout = 0; layout < 3; ++layout) {
            const auto document = Starter::create(theme, QSizeF(1920, 1080), layout);
            rows.append(describeDocument(document,
                                         QStringLiteral("builtin:%1:%2").arg(theme).arg(layout),
                                         builtInName(theme, layout),
                                         QStringLiteral("built-in")));
        }
    QDir directory(folder());
    const auto files = directory.entryInfoList({QStringLiteral("*.omashow")}, QDir::Files,
                                               QDir::Name);
    for (const auto &info : files) {
        if (info.size() > kLargest) continue;
        const auto read = Bundle::load(info.absoluteFilePath());
        if (!read.ok) {
            rows.append(QVariantMap{{"id", info.absoluteFilePath()},
                                    {"name", info.completeBaseName()},
                                    {"source", QStringLiteral("installed")},
                                    {"broken", true},
                                    {"error", read.error},
                                    {"aspect", QStringLiteral("—")},
                                    {"fonts", QStringList()},
                                    {"missingFonts", QStringList()},
                                    {"layouts", QStringList()},
                                    {"colors", QStringList()}});
            continue;
        }
        rows.append(describeDocument(read.document, info.absoluteFilePath(),
                                     info.completeBaseName(), QStringLiteral("installed")));
    }
    // Examples ship with the application and are opened the same way: as a new
    // deck, so the copy on disk is never written over.
    for (const auto &directory : exampleFolders())
        for (const auto &info : QDir(directory).entryInfoList({QStringLiteral("*.omashow")},
                                                              QDir::Files, QDir::Name)) {
            if (info.size() > kLargest) continue;
            const auto read = Bundle::load(info.absoluteFilePath());
            if (!read.ok) continue;
            rows.append(describeDocument(read.document, info.absoluteFilePath(),
                                         info.completeBaseName(), QStringLiteral("example")));
        }
    return rows;
}

bool Templates::describe(const QString &id, QVariantMap *row) {
    for (const auto &value : all()) {
        if (value.toMap().value("id").toString() != id) continue;
        if (row) *row = value.toMap();
        return true;
    }
    return false;
}

bool Templates::open(const QString &id, Document *document, QString *error) {
    if (!document) return false;
    if (id.startsWith(QStringLiteral("builtin:"))) {
        const int theme = id.section(':', 1, 1).toInt();
        const int layout = id.section(':', 2, 2).toInt();
        if (theme < 0 || theme > 2 || layout < 0 || layout > 2) {
            if (error) *error = QStringLiteral("There is no such built-in template.");
            return false;
        }
        *document = Starter::create(theme, QSizeF(1920, 1080), layout);
        return true;
    }
    const QFileInfo info(id);
    if (!info.isFile() || info.size() > kLargest) {
        if (error) *error = QStringLiteral("That template is not there any more.");
        return false;
    }
    const auto read = Bundle::load(id);
    if (!read.ok) {
        if (error) *error = read.error;
        return false;
    }
    *document = read.document;
    return true;
}

namespace {

QStringList travelWarnings(const Document &document) {
    QStringList warnings;
    int linked = 0, missingMedia = 0;
    for (const auto &slide : document.slides)
        for (const auto &object : slide.objects) {
            const QString path = object.type == ObjectType::Media ? object.mediaPath
                                                                  : object.dataSource.path;
            if (path.isEmpty()) continue;
            ++linked;
            if (!QFileInfo(path).isReadable()) ++missingMedia;
        }
    if (linked > 0)
        warnings.append(QStringLiteral("%1 linked files stay linked: a template does not carry "
                                       "them.").arg(linked));
    if (missingMedia > 0)
        warnings.append(QStringLiteral("%1 of those are already missing here.").arg(missingMedia));
    QStringList missingFonts;
    for (const auto &family : document.theme.fonts)
        if (!QFontDatabase::hasFamily(family) && !missingFonts.contains(family))
            missingFonts.append(family);
    if (!missingFonts.isEmpty())
        warnings.append(QStringLiteral("Typefaces not installed here: %1.")
                            .arg(missingFonts.join(QStringLiteral(", "))));
    return warnings;
}

QString freeFile(const QString &name) {
    QDir().mkpath(Templates::folder());
    QString base = name.trimmed();
    base.replace(QRegularExpression(QStringLiteral("[^\\w \\-.]")), QString());
    base = base.trimmed();
    if (base.isEmpty()) base = QStringLiteral("Template");
    QString candidate = Templates::folder() + '/' + base + QStringLiteral(".omashow");
    for (int n = 2; QFileInfo::exists(candidate); ++n)
        candidate = Templates::folder() + '/' + base + QStringLiteral(" %1.omashow").arg(n);
    return candidate;
}

} // namespace

QString Templates::install(const QString &path, QString *error, QStringList *warnings) {
    const QFileInfo info(path);
    if (!info.isFile() || !info.isReadable()) {
        if (error) *error = QStringLiteral("That file cannot be read.");
        return {};
    }
    if (info.size() > kLargest) {
        if (error) *error = QStringLiteral("That deck is too large to keep as a template.");
        return {};
    }
    const auto read = Bundle::load(path);
    if (!read.ok) {
        if (error) *error = read.error;
        return {};
    }
    if (warnings) *warnings = travelWarnings(read.document);
    const auto target = freeFile(info.completeBaseName());
    if (!QFile::copy(path, target)) {
        if (error) *error = QStringLiteral("The template could not be copied in.");
        return {};
    }
    return target;
}

QString Templates::save(const Document &document, const QString &name, QString *error,
                        QStringList *warnings) {
    if (document.slides.isEmpty()) {
        if (error) *error = QStringLiteral("There is nothing to keep.");
        return {};
    }
    if (warnings) *warnings = travelWarnings(document);
    const auto target = freeFile(name);
    if (!Bundle::save(document, target, error)) return {};
    return target;
}

bool Templates::remove(const QString &id, QString *error) {
    if (id.startsWith(QStringLiteral("builtin:"))) {
        if (error) *error = QStringLiteral("Built-in templates are part of the application.");
        return false;
    }
    const QFileInfo info(id);
    if (info.absolutePath() != QFileInfo(folder()).absoluteFilePath()) {
        if (error) *error = QStringLiteral("That is not an installed template.");
        return false;
    }
    if (!QFile::remove(id)) {
        if (error) *error = QStringLiteral("The template could not be removed.");
        return false;
    }
    return true;
}
