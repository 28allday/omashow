#include "io/interchange.h"

#include "io/bundle.h"
#include "io/keynote.h"
#include "io/pptx.h"

#include <QFile>
#include <QFileInfo>

namespace Interchange {

Kind kindOf(const QString &path) {
    const auto suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("omashow")) return Native;
    if (suffix == QStringLiteral("pptx")) return PowerPoint;
    if (suffix == QStringLiteral("key")) return Keynote;
    return Unknown;
}

QString kindName(Kind kind) {
    switch (kind) {
    case Native: return QStringLiteral("OmaShow");
    case PowerPoint: return QStringLiteral("PowerPoint");
    case Keynote: return QStringLiteral("Keynote");
    case Unknown: break;
    }
    return QString();
}

Result load(const QString &path) {
    Result result;
    result.kind = kindOf(path);
    if (result.kind == Unknown) {
        result.error = QStringLiteral("%1 is not a deck OmaShow can open: it opens .omashow, "
                                      ".pptx and .key files.")
                           .arg(QFileInfo(path).fileName());
        return result;
    }
    if (result.kind == Native) {
        const auto read = Bundle::load(path);
        result.ok = read.ok;
        result.error = read.error;
        result.document = read.document;
        return result;
    }
    if (result.kind == Keynote) {
        // Keynote packages can be folders; the reader takes the path itself.
        const auto read = Keynote::load(path);
        result.ok = read.ok;
        result.error = read.error;
        result.document = read.document;
        result.warnings = read.warnings;
        return result;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("%1 could not be read.").arg(QFileInfo(path).fileName());
        return result;
    }
    return fromBytes(file.readAll(), result.kind);
}

Result fromBytes(const QByteArray &raw, Kind kind) {
    Result result;
    result.kind = kind;
    if (kind == Native) {
        const auto read = Bundle::fromBytes(raw);
        result.ok = read.ok;
        result.error = read.error;
        result.document = read.document;
    } else if (kind == PowerPoint) {
        const auto read = Pptx::read(raw);
        result.ok = read.ok;
        result.error = read.error;
        result.document = read.document;
        result.warnings = read.warnings;
    } else if (kind == Keynote) {
        const auto read = Keynote::read(raw);
        result.ok = read.ok;
        result.error = read.error;
        result.document = read.document;
        result.warnings = read.warnings;
    } else {
        result.error = QStringLiteral("not a deck OmaShow can open.");
    }
    return result;
}

QStringList openPatterns() {
    return {QStringLiteral("*.omashow"), QStringLiteral("*.pptx"), QStringLiteral("*.key")};
}

QString suggestedName(const QString &path) {
    const auto base = QFileInfo(path).completeBaseName();
    return (base.isEmpty() ? QStringLiteral("Untitled") : base) + QStringLiteral(".omashow");
}

} // namespace Interchange
