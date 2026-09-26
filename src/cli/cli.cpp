#include "cli/cli.h"

#include "backend.h"
#include "cli/describe.h"
#include "cli/operations.h"
#include "core/design.h"
#include "core/mediaasset.h"
#include "io/bundle.h"
#include "io/exports.h"
#include "io/interchange.h"
#include "core/review.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace {

QTextStream &out() {
    static QTextStream stream(stdout);
    return stream;
}
QTextStream &err() {
    static QTextStream stream(stderr);
    return stream;
}

int say(const QVariantMap &payload) {
    auto body = payload;
    body[QStringLiteral("ok")] = true;
    out() << QString::fromUtf8(
                 QJsonDocument(QJsonObject::fromVariantMap(body)).toJson(QJsonDocument::Indented))
          << Qt::flush;
    return 0;
}
int refuse(const QString &message, const QVariantMap &extra = {}) {
    auto body = extra;
    body[QStringLiteral("ok")] = false;
    body[QStringLiteral("error")] = message;
    out() << QString::fromUtf8(
                 QJsonDocument(QJsonObject::fromVariantMap(body)).toJson(QJsonDocument::Indented))
          << Qt::flush;
    return 1;
}

// --name value, --flag, and whatever is left over. An unknown flag is a
// mistake worth stopping for rather than quietly ignoring.
struct Flags {
    QMap<QString, QString> values;
    QStringList switches;
    QStringList rest;
    QString complaint;

    static Flags read(const QStringList &arguments, const QStringList &taking,
                      const QStringList &alone) {
        Flags flags;
        for (int i = 0; i < arguments.size(); ++i) {
            const auto word = arguments.at(i);
            if (!word.startsWith(QStringLiteral("--"))) { flags.rest.append(word); continue; }
            auto name = word.mid(2);
            QString value;
            const int equals = name.indexOf('=');
            if (equals >= 0) { value = name.mid(equals + 1); name = name.left(equals); }
            if (alone.contains(name)) {
                flags.switches.append(name);
                continue;
            }
            if (!taking.contains(name)) {
                flags.complaint = QStringLiteral("there is no option called --%1.").arg(name);
                return flags;
            }
            if (equals < 0) {
                if (i + 1 >= arguments.size()) {
                    flags.complaint = QStringLiteral("--%1 needs a value.").arg(name);
                    return flags;
                }
                value = arguments.at(++i);
            }
            flags.values.insert(name, value);
        }
        return flags;
    }
    bool has(const QString &name) const { return switches.contains(name); }
    QString value(const QString &name, const QString &fallback = {}) const {
        return values.value(name, fallback);
    }
};

// A deck from PowerPoint or Keynote reads like any other; `source` says so,
// with what the conversion could not carry, so the answer can pass it on.
bool readDeck(Backend &backend, const QString &path, QString *error, QVariantMap *source = nullptr) {
    if (path.isEmpty()) { *error = QStringLiteral("name the deck to work on."); return false; }
    if (!QFileInfo::exists(path)) {
        *error = QStringLiteral("there is no file at %1.").arg(path);
        return false;
    }
    const auto read = Interchange::load(path);
    if (!read.ok) { *error = read.error; return false; }
    backend.setDocument(read.document);
    if (source && Interchange::isForeign(read.kind))
        *source = {{QStringLiteral("kind"), Interchange::kindName(read.kind)},
                   {QStringLiteral("file"), QFileInfo(path).absoluteFilePath()},
                   {QStringLiteral("warnings"), read.warnings}};
    return true;
}

// Converts a deck from another application into an OmaShow one, beside it
// unless --out says otherwise, and never over a file that is already there
// unless --force says so.
int importDeck(Backend &backend, const Flags &flags) {
    const auto path = flags.rest.value(0);
    if (path.isEmpty()) return refuse(QStringLiteral("name the .pptx or .key file to import."));
    const auto kind = Interchange::kindOf(path);
    if (!Interchange::isForeign(kind))
        return refuse(QStringLiteral("import takes a .pptx or .key file; %1 is not one.").arg(QFileInfo(path).fileName()));
    QString trouble;
    QVariantMap source;
    if (!readDeck(backend, path, &trouble, &source)) return refuse(trouble);
    QString destination = flags.value(QStringLiteral("out"));
    if (destination.isEmpty())
        destination = QFileInfo(path).absoluteDir().filePath(Interchange::suggestedName(path));
    if (Interchange::kindOf(destination) != Interchange::Native)
        return refuse(QStringLiteral("--out names the OmaShow deck to write, so it ends in .omashow."));
    if (QFileInfo::exists(destination) && !flags.has(QStringLiteral("force")))
        return refuse(QStringLiteral("%1 already exists; --force replaces it, or --out names another file.").arg(destination));
    if (!backend.saveTo(destination))
        return refuse(QStringLiteral("%1 could not be written.").arg(destination));
    return say({{QStringLiteral("file"), QFileInfo(destination).absoluteFilePath()},
                {QStringLiteral("source"), source},
                {QStringLiteral("slides"), backend.document().slides.size()},
                {QStringLiteral("warnings"), source.value(QStringLiteral("warnings"))},
                {QStringLiteral("statistics"), Review::statistics(backend.document())}});
}

QVariant readJson(const QString &path, QString *error) {
    QByteArray text;
    if (path == QStringLiteral("-")) {
        QFile input;
        if (!input.open(stdin, QIODevice::ReadOnly)) {
            *error = QStringLiteral("standard input could not be read.");
            return {};
        }
        text = input.readAll();
    } else {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            *error = QStringLiteral("%1 could not be read.").arg(path);
            return {};
        }
        text = file.readAll();
    }
    QJsonParseError trouble;
    const auto parsed = QJsonDocument::fromJson(text, &trouble);
    if (trouble.error != QJsonParseError::NoError) {
        *error = QStringLiteral("%1 is not valid JSON: %2 at character %3")
                     .arg(path == QStringLiteral("-") ? QStringLiteral("standard input") : path,
                          trouble.errorString())
                     .arg(trouble.offset);
        return {};
    }
    return parsed.toVariant();
}

// 1920x1080, or the shapes people ask for by name.
bool readSize(const QString &given, QSizeF *size, QString *error) {
    const auto text = given.trimmed().toLower();
    if (text.isEmpty() || text == QStringLiteral("16:9")) { *size = QSizeF(1920, 1080); return true; }
    if (text == QStringLiteral("4:3")) { *size = QSizeF(1440, 1080); return true; }
    if (text == QStringLiteral("16:10")) { *size = QSizeF(1920, 1200); return true; }
    if (text == QStringLiteral("portrait")) { *size = QSizeF(1080, 1920); return true; }
    const auto parts = text.split('x');
    bool wide = false, tall = false;
    if (parts.size() == 2) {
        const qreal w = parts.at(0).toDouble(&wide), h = parts.at(1).toDouble(&tall);
        if (wide && tall) { *size = QSizeF(w, h); return true; }
    }
    *error = QStringLiteral("%1 is not a slide size; give 1920x1080, 16:9, 4:3, 16:10 or portrait.")
                 .arg(given);
    return false;
}

// The deck's own record of a linked film is not proof: a deck from someone
// else can name any file and any hash. Approve a path only when the file there
// opens as media and its bytes hash to what the deck says, as Media preflight
// does in the window.
QHash<QString, QString> everythingLinked(const Document &document, QStringList *log) {
    QHash<QString, QString> approved;
    for (const auto &slide : document.slides)
        for (const auto &object : slide.objects) {
            if (object.type != ObjectType::Media || object.mediaPath.isEmpty() ||
                approved.contains(object.mediaPath))
                continue;
            // The same test as Media preflight in the window (Backend's
            // approveOnly): the bytes hash to the deck's id, and the size and
            // time the deck recorded still hold.
            const auto probed = MediaAsset::fromFile(object.mediaPath, false);
            if (probed.ok() && probed.object.mediaId == object.mediaId &&
                probed.object.mediaBytes == object.mediaBytes &&
                probed.object.mediaModified == object.mediaModified) {
                approved.insert(object.mediaPath, object.mediaId);
                log->append(QStringLiteral("Approved linked media %1").arg(object.mediaPath));
            } else {
                log->append(QStringLiteral("Not approved: %1 — %2")
                                .arg(object.mediaPath,
                                     !probed.ok() ? probed.error
                                     : probed.object.mediaId != object.mediaId
                                         ? QStringLiteral("the file is not the one the deck linked")
                                         : QStringLiteral("the file's size or date changed; relink it in the app")));
            }
        }
    return approved;
}

const QStringList &verbs() {
    static const QStringList list{QStringLiteral("new"),    QStringLiteral("inspect"),
                                  QStringLiteral("apply"),  QStringLiteral("export"),
                                  QStringLiteral("review"), QStringLiteral("ops"),
                                  QStringLiteral("import"), QStringLiteral("skill"),
                                  QStringLiteral("help")};
    return list;
}

// Where the agent skill ended up: beside a build, or in the package's share.
QString skillFolder() {
    const QStringList places{
        QCoreApplication::applicationDirPath() + QStringLiteral("/../skills/omashow"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/skills/omashow"),
        QStringLiteral("/usr/share/omashow/skills/omashow"),
        QStringLiteral("/usr/local/share/omashow/skills/omashow")};
    for (const auto &place : places)
        if (QFileInfo::exists(place + QStringLiteral("/SKILL.md")))
            return QFileInfo(place).canonicalFilePath();
    return {};
}

void usage() {
    out() << QStringLiteral(R"(OmaShow — presentations for Omarchy, without a window.

  omashow new <file> [--theme 0-2] [--size 16:9|1920x1080] [--layout 0-2] [--slides N] [--force]
  omashow import <file.pptx|file.key> [--out <file.omashow>] [--force]
  omashow inspect <file> [--slide N] [--full]
  omashow apply <file> [ops.json|-] [--out <file>] [--dry-run] [--keep-going] [--force]
  omashow export <file> --kind pdf|images|video|package|print|pptx --out <path>
                        [--from N] [--to N] [--layout slides|notes|outline|handout]
                        [--per-page N] [--width N] [--format png|jpeg] [--fps N]
                        [--quality 0|1] [--transparent] [--stages] [--include-skipped]
                        [--printer NAME] [--copies N] [--approve-media] [--force]
  omashow review <file> [--include-dismissed]
  omashow ops [--filter <text>]
  omashow skill [--link] [--force]

Everything answers with JSON on standard output and exits 0 or 1.

An operations file is a list of what to do, in order:

  {"ops": [
     {"op": "setCurrentSlide", "args": [0]},
     {"op": "addText"},
     {"op": "setSelectedProperty", "args": ["text", "Good morning"]},
     {"op": "setSelectedProperty", "args": {"name": "fontSize", "value": 72}},
     {"op": "setSelectedProperty", "select": "text-2", "args": ["textAlign", 1]}
  ]}

`omashow ops` lists every operation there is, with its arguments: they are the
same ones the interface calls, so anything the app can do is in that list.
)") << Qt::flush;
}

// Where the agents on this computer keep their skills. Omarchy's own skills are
// one directory under /usr/share linked into each of these, so OmaShow's is put
// there the same way rather than inventing a second arrangement.
QVector<QPair<QString, bool>> skillPlaces() {
    const auto home = QDir::homePath();
    QVector<QPair<QString, bool>> places{
        {home + QStringLiteral("/.agents/skills"), true},   // whichever agent reads it
        {home + QStringLiteral("/.claude/skills"), true}};
    const QVector<QPair<QString, QString>> others{
        {home + QStringLiteral("/.codex"), home + QStringLiteral("/.codex/skills")},
        {home + QStringLiteral("/.pi"), home + QStringLiteral("/.pi/agent/skills")},
        {home + QStringLiteral("/.hermes"), home + QStringLiteral("/.hermes/skills")}};
    // Only for the agents that are actually here: a directory for a tool
    // somebody does not use is litter.
    for (const auto &other : others)
        if (QFileInfo::exists(other.first)) places.append({other.second, false});
    const auto profiles = home + QStringLiteral("/.hermes/profiles");
    if (QFileInfo::exists(profiles))
        for (const auto &profile :
             QDir(profiles).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            places.append({profiles + '/' + profile + QStringLiteral("/skills"), false});
    return places;
}

// The skill is a file in the app; putting it where the agents look is a link,
// made as the person (installing does it for them), never as root.
int offerSkill(const Flags &flags) {
    const auto folder = skillFolder();
    if (folder.isEmpty())
        return refuse(QStringLiteral("the skill is not beside this copy of OmaShow; it lives in "
                                     "skills/omashow in the source."));
    const auto canonical = QFileInfo(folder).canonicalFilePath();
    const bool link = flags.has(QStringLiteral("link"));
    QVariantList rows;
    QStringList trouble;
    int linkedNow = 0, alreadyThere = 0;
    for (const auto &place : skillPlaces()) {
        const auto target = place.first + QStringLiteral("/omashow");
        const QFileInfo already(target);
        const bool matches =
            already.exists() && already.canonicalFilePath() == canonical;
        QVariantMap row{{QStringLiteral("place"), target}, {QStringLiteral("linked"), matches}};
        if (matches) ++alreadyThere;
        if (!link || matches) {
            rows.append(row);
            continue;
        }
        // A link to nothing — left by an old copy, or carried over from another
        // computer — is nobody's work, so it is replaced without asking.
        const bool dangling = already.isSymLink() && !already.exists();
        if (already.exists() || already.isSymLink()) {
            if (!dangling && !flags.has(QStringLiteral("force"))) {
                row[QStringLiteral("note")] =
                    QStringLiteral("something else is there; --force replaces it");
                trouble.append(QStringLiteral("%1 is already something else").arg(target));
                rows.append(row);
                continue;
            }
            const bool cleared = already.isSymLink() ? QFile::remove(target)
                                                     : QDir(target).removeRecursively();
            if (!cleared) {
                trouble.append(QStringLiteral("%1 could not be replaced").arg(target));
                rows.append(row);
                continue;
            }
        }
        if (!QDir().mkpath(place.first) || !QFile::link(folder, target)) {
            trouble.append(QStringLiteral("%1 could not be linked").arg(target));
            rows.append(row);
            continue;
        }
        row[QStringLiteral("linked")] = true;
        ++linkedNow;
        rows.append(row);
    }
    QVariantMap payload{{QStringLiteral("skill"), folder + QStringLiteral("/SKILL.md")},
                        {QStringLiteral("places"), rows},
                        {QStringLiteral("linked"), linkedNow + alreadyThere}};
    if (!trouble.isEmpty()) {
        auto said = trouble.join(QStringLiteral("; ")) + '.';
        if (said.contains(QStringLiteral("already something else")))
            said += QStringLiteral(" Use --force to replace what is there.");
        return refuse(said, payload);
    }
    payload[QStringLiteral("advice")] =
        link ? QStringLiteral("The agents on this computer will find it in a new session.")
             : (alreadyThere > 0 && linkedNow == 0 && alreadyThere == rows.size()
                    ? QStringLiteral("Every agent here already has it.")
                    : QStringLiteral("Run `omashow skill --link` to put it where the agents look."));
    return say(payload);
}

int describeOps(const Flags &flags) {
    const auto filter = flags.value(QStringLiteral("filter"));
    QVariantMap payload{{QStringLiteral("operations"), Cli::describeOperations(filter)}};
    if (filter.isEmpty()) payload[QStringLiteral("vocabulary")] = Cli::vocabulary();
    return say(payload);
}

int makeDeck(Backend &backend, const Flags &flags) {
    const auto path = flags.rest.value(0);
    if (path.isEmpty()) return refuse(QStringLiteral("name the file to write."));
    QSizeF size;
    QString trouble;
    if (!readSize(flags.value(QStringLiteral("size")), &size, &trouble)) return refuse(trouble);
    const int theme = flags.value(QStringLiteral("theme"), QStringLiteral("0")).toInt();
    const int layout = flags.value(QStringLiteral("layout"), QStringLiteral("0")).toInt();
    const int slides = qMax(1, flags.value(QStringLiteral("slides"), QStringLiteral("1")).toInt());
    if (!backend.createDeck(theme, size.width(), size.height(), layout))
        return refuse(QStringLiteral("a theme is 0, 1 or 2, a layout 0, 1 or 2, and a slide is "
                                     "between 240 and 10,000 across."));
    for (int i = 1; i < slides; ++i) backend.addSlide();
    if (QFileInfo::exists(path) && !flags.has(QStringLiteral("force")))
        return refuse(QStringLiteral("%1 already exists; --force replaces it.").arg(path));
    if (!backend.saveTo(path))
        return refuse(QStringLiteral("%1 could not be written.").arg(path));
    return say({{QStringLiteral("file"), QFileInfo(path).absoluteFilePath()},
                {QStringLiteral("slides"), backend.document().slides.size()},
                {QStringLiteral("theme"), backend.document().theme.name},
                {QStringLiteral("size"),
                 QVariantMap{{QStringLiteral("width"), size.width()},
                             {QStringLiteral("height"), size.height()}}}});
}

int inspectDeck(Backend &backend, const Flags &flags) {
    QString trouble;
    QVariantMap source;
    if (!readDeck(backend, flags.rest.value(0), &trouble, &source)) return refuse(trouble);
    const bool full = flags.has(QStringLiteral("full"));
    const auto &document = backend.document();
    if (flags.values.contains(QStringLiteral("slide"))) {
        const int index = flags.value(QStringLiteral("slide")).toInt();
        if (index < 0 || index >= document.slides.size())
            return refuse(QStringLiteral("the deck has %1 slides, numbered from 0.")
                              .arg(document.slides.size()));
        QVariantMap payload{{QStringLiteral("file"), flags.rest.value(0)},
                            {QStringLiteral("slide"), Cli::describeSlide(document, index, full)}};
        if (!source.isEmpty()) payload[QStringLiteral("source")] = source;
        return say(payload);
    }
    auto payload = Cli::describeDeck(document, full);
    payload[QStringLiteral("file")] = flags.rest.value(0);
    if (!source.isEmpty()) payload[QStringLiteral("source")] = source;
    return say(payload);
}

int reviewDeck(Backend &backend, const Flags &flags) {
    QString trouble;
    QVariantMap source;
    if (!readDeck(backend, flags.rest.value(0), &trouble, &source)) return refuse(trouble);
    auto payload = Cli::describeReview(backend.document(),
                                       flags.has(QStringLiteral("include-dismissed")));
    payload[QStringLiteral("file")] = flags.rest.value(0);
    if (!source.isEmpty()) payload[QStringLiteral("source")] = source;
    return say(payload);
}

int applyOps(Backend &backend, const Flags &flags) {
    QString trouble;
    QVariantMap source;
    if (!readDeck(backend, flags.rest.value(0), &trouble, &source)) return refuse(trouble);
    if (!source.isEmpty() && !flags.has(QStringLiteral("dry-run"))) {
        const auto out = flags.value(QStringLiteral("out"));
        if (out.isEmpty())
            return refuse(QStringLiteral("%1 is a %2 deck, which OmaShow never writes back to; --out names "
                                         "the .omashow file to write instead.")
                              .arg(QFileInfo(flags.rest.value(0)).fileName(), source.value(QStringLiteral("kind")).toString()));
        if (Interchange::kindOf(out) != Interchange::Native)
            return refuse(QStringLiteral("--out names the OmaShow deck to write, so it ends in .omashow."));
    }
    const auto opsPath = flags.values.contains(QStringLiteral("ops"))
                            ? flags.value(QStringLiteral("ops"))
                            : flags.rest.value(1, QStringLiteral("-"));
    const auto given = readJson(opsPath, &trouble);
    if (!trouble.isEmpty()) return refuse(trouble);
    QVariantList operations;
    if (given.metaType().id() == QMetaType::QVariantList) operations = given.toList();
    else if (given.metaType().id() == QMetaType::QVariantMap)
        operations = given.toMap().value(QStringLiteral("ops")).toList();
    if (operations.isEmpty())
        return refuse(QStringLiteral("give a list of operations, or {\"ops\": [ ... ]}."));

    const bool keepGoing = flags.has(QStringLiteral("keep-going"));
    QVariantList results;
    int done = 0;
    bool everything = true;
    for (int i = 0; i < operations.size(); ++i) {
        const auto operation = operations.at(i).toMap();
        const auto result = Cli::runOperation(backend, operation);
        QVariantMap row{{QStringLiteral("at"), i},
                        {QStringLiteral("op"), operation.value(QStringLiteral("op"))},
                        {QStringLiteral("ok"), result.ok}};
        row[QStringLiteral("changed")] = result.changed;
        if (!result.error.isEmpty()) row[QStringLiteral("error")] = result.error;
        if (!result.warning.isEmpty()) row[QStringLiteral("warning")] = result.warning;
        if (result.value.isValid()) row[QStringLiteral("returned")] = result.value;
        results.append(row);
        if (result.ok) { ++done; continue; }
        everything = false;
        if (!keepGoing) break;
    }

    QVariantMap payload{{QStringLiteral("applied"), done},
                        {QStringLiteral("of"), operations.size()},
                        {QStringLiteral("results"), results}};
    if (!source.isEmpty()) payload[QStringLiteral("source")] = source;
    // Warnings gathered where they cannot be missed.
    QVariantList warnings;
    for (const auto &row : results)
        if (row.toMap().contains(QStringLiteral("warning")))
            warnings.append(QVariantMap{{QStringLiteral("at"), row.toMap().value(QStringLiteral("at"))},
                                        {QStringLiteral("warning"), row.toMap().value(QStringLiteral("warning"))}});
    if (!warnings.isEmpty()) payload[QStringLiteral("warnings")] = warnings;
    if (!everything && !keepGoing) {
        payload[QStringLiteral("written")] = false;
        return refuse(QStringLiteral("operation %1 did not run; nothing was written. Use "
                                     "--keep-going to carry on past a refusal.")
                          .arg(results.size() - 1),
                      payload);
    }
    if (flags.has(QStringLiteral("dry-run"))) {
        payload[QStringLiteral("written")] = false;
        payload[QStringLiteral("statistics")] = Review::statistics(backend.document());
        return everything ? say(payload) : refuse(QStringLiteral("some operations were refused."),
                                                  payload);
    }
    const auto destination = flags.value(QStringLiteral("out"), flags.rest.value(0));
    // Saving over the deck that was read is the point of apply; saving over
    // some other file that is already there needs saying.
    const QFileInfo target(destination);
    if (target.exists() && !flags.has(QStringLiteral("force")) &&
        target.canonicalFilePath() != QFileInfo(flags.rest.value(0)).canonicalFilePath()) {
        payload[QStringLiteral("written")] = false;
        return refuse(QStringLiteral("%1 already exists; --force replaces it.").arg(destination),
                      payload);
    }
    if (!backend.saveTo(destination)) {
        payload[QStringLiteral("written")] = false;
        return refuse(QStringLiteral("%1 could not be written.").arg(destination), payload);
    }
    payload[QStringLiteral("written")] = true;
    payload[QStringLiteral("file")] = QFileInfo(destination).absoluteFilePath();
    payload[QStringLiteral("statistics")] = Review::statistics(backend.document());
    return everything ? say(payload)
                      : refuse(QStringLiteral("some operations were refused; the rest were "
                                              "written."),
                               payload);
}

int exportDeck(Backend &backend, const Flags &flags) {
    QString trouble;
    QVariantMap source;
    if (!readDeck(backend, flags.rest.value(0), &trouble, &source)) return refuse(trouble);
    const auto kinds = QStringList{QStringLiteral("pdf"), QStringLiteral("images"),
                                   QStringLiteral("video"), QStringLiteral("package"),
                                   QStringLiteral("print"), QStringLiteral("pptx")};
    const auto kind = flags.value(QStringLiteral("kind")).toLower();
    if (!kinds.contains(kind))
        return refuse(QStringLiteral("--kind is one of %1.").arg(kinds.join(", ")));
    Exports::Request request;
    request.kind = kinds.indexOf(kind);
    request.path = flags.value(QStringLiteral("out"));
    if (request.path.isEmpty() && request.kind != Exports::Print)
        return refuse(QStringLiteral("--out says where to write it."));
    const auto layouts = QStringList{QStringLiteral("slides"), QStringLiteral("notes"),
                                     QStringLiteral("outline"), QStringLiteral("handout")};
    if (flags.values.contains(QStringLiteral("layout"))) {
        const auto layout = flags.value(QStringLiteral("layout")).toLower();
        if (!layouts.contains(layout))
            return refuse(QStringLiteral("--layout is one of %1.").arg(layouts.join(", ")));
        request.layout = layouts.indexOf(layout);
    }
    request.stages = flags.has(QStringLiteral("stages"));
    request.includeSkipped = flags.has(QStringLiteral("include-skipped"));
    request.transparent = flags.has(QStringLiteral("transparent"));
    if (flags.values.contains(QStringLiteral("from")))
        request.from = flags.value(QStringLiteral("from")).toInt();
    if (flags.values.contains(QStringLiteral("to")))
        request.to = flags.value(QStringLiteral("to")).toInt();
    if (flags.values.contains(QStringLiteral("per-page")))
        request.perPage = flags.value(QStringLiteral("per-page")).toInt();
    if (flags.values.contains(QStringLiteral("width")))
        request.width = flags.value(QStringLiteral("width")).toInt();
    if (flags.values.contains(QStringLiteral("fps")))
        request.fps = flags.value(QStringLiteral("fps")).toInt();
    if (flags.values.contains(QStringLiteral("quality")))
        request.quality = flags.value(QStringLiteral("quality")).toInt();
    if (flags.values.contains(QStringLiteral("copies")))
        request.copies = flags.value(QStringLiteral("copies")).toInt();
    if (flags.values.contains(QStringLiteral("format"))) {
        const auto format = flags.value(QStringLiteral("format")).toLower();
        if (format != QStringLiteral("png") && format != QStringLiteral("jpeg") &&
            format != QStringLiteral("jpg"))
            return refuse(QStringLiteral("--format is png or jpeg."));
        request.format = format.startsWith(QStringLiteral("png")) ? 0 : 1;
    }
    request.printer = flags.value(QStringLiteral("printer"));
    // --out names a file; a folder there would give pictures without a name.
    if (!request.path.isEmpty() && (request.path.endsWith(QLatin1Char('/')) || QFileInfo(request.path).isDir()))
        return refuse(QStringLiteral("--out names a file, and %1 is a folder; give a file name inside it.").arg(request.path));
    // Every file the export would write, not just the name given: pictures
    // of several slides are numbered, and the suffix follows the format.
    if (!flags.has(QStringLiteral("force")))
        for (const auto &file : Exports::targets(backend.document(), request))
            if (QFileInfo::exists(file))
                return refuse(QStringLiteral("%1 already exists; --force replaces it.").arg(file));
    // The interface is given a folder that exists because somebody chose it;
    // here the path is typed, so make the folder it names.
    if (!request.path.isEmpty()) {
        const auto folder = QFileInfo(request.path).absolutePath();
        if (!folder.isEmpty() && !QDir().mkpath(folder))
            return refuse(QStringLiteral("%1 could not be made.").arg(folder));
    }
    if (request.kind == Exports::Video && !Exports::encoderAvailable())
        return refuse(QStringLiteral("film needs ffmpeg, which is not on this computer."));

    // Reading a file the deck links to is the author's decision, here as in the
    // interface — say so on the command line and it is taken.
    // Pictures, film and PDF draw a linked film's frames once it may be read;
    // a package copies its bytes. Approval marks each approved film as readable on the
    // copy being exported, as the window does for its export queue.
    QStringList approvals;
    const auto approved = flags.has(QStringLiteral("approve-media"))
                              ? everythingLinked(backend.document(), &approvals)
                              : QHash<QString, QString>();
    Document exported = backend.document();
    for (auto &slide : exported.slides)
        for (auto &object : slide.objects)
            if (object.type == ObjectType::Media)
                object.mediaReadAllowed = approved.value(object.mediaPath) == object.mediaId;
    QElapsedTimer timer;
    timer.start();
    const auto outcome = Exports::run(exported, request,
                                      std::make_shared<Workers::Job>(),
                                      [](int) {}, approved);
    QVariantMap payload{{QStringLiteral("kind"), kind},
                        {QStringLiteral("files"), outcome.files},
                        {QStringLiteral("log"), approvals + outcome.log},
                        {QStringLiteral("describes"), request.describe()},
                        {QStringLiteral("milliseconds"), timer.elapsed()}};
    return outcome.ok ? say(payload) : refuse(outcome.error, payload);
}

} // namespace

bool Cli::isVerb(const QString &word) { return verbs().contains(word); }

int Cli::run(Backend &backend, const QStringList &arguments) {
    const auto verb = arguments.value(0);
    const auto rest = arguments.mid(1);
    if (verb == QStringLiteral("help") || rest.contains(QStringLiteral("--help"))) {
        usage();
        return 0;
    }
    static const QStringList taking{
        QStringLiteral("theme"),   QStringLiteral("size"),     QStringLiteral("layout"),
        QStringLiteral("slides"),  QStringLiteral("slide"),    QStringLiteral("ops"),
        QStringLiteral("out"),     QStringLiteral("kind"),     QStringLiteral("from"),
        QStringLiteral("to"),      QStringLiteral("per-page"), QStringLiteral("width"),
        QStringLiteral("format"),  QStringLiteral("fps"),      QStringLiteral("quality"),
        QStringLiteral("printer"), QStringLiteral("copies"),   QStringLiteral("filter")};
    static const QStringList alone{
        QStringLiteral("full"),           QStringLiteral("dry-run"),
        QStringLiteral("keep-going"),     QStringLiteral("stages"),
        QStringLiteral("include-skipped"), QStringLiteral("transparent"),
        QStringLiteral("include-dismissed"), QStringLiteral("approve-media"),
        QStringLiteral("link"), QStringLiteral("force")};
    const auto flags = Flags::read(rest, taking, alone);
    if (!flags.complaint.isEmpty()) return refuse(flags.complaint);

    if (verb == QStringLiteral("skill")) return offerSkill(flags);
    if (verb == QStringLiteral("ops")) return describeOps(flags);
    if (verb == QStringLiteral("new")) return makeDeck(backend, flags);
    if (verb == QStringLiteral("import")) return importDeck(backend, flags);
    if (verb == QStringLiteral("inspect")) return inspectDeck(backend, flags);
    if (verb == QStringLiteral("review")) return reviewDeck(backend, flags);
    if (verb == QStringLiteral("apply")) return applyOps(backend, flags);
    if (verb == QStringLiteral("export")) return exportDeck(backend, flags);
    usage();
    return 1;
}
