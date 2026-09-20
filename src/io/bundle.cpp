#include "core/design.h"
#include "core/imageasset.h"
#include "core/mediaasset.h"
#include "core/table.h"
#include "core/chart.h"
#include "core/link.h"
#include "core/review.h"
#include "core/textruns.h"
#include <QRegularExpression>
#include <cmath>
#include <QSet>
#include <functional>
#include "io/bundle.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include "io/zip.h"

namespace {

QString colorToString(const QColor &color) {
    return color.name(color.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb);
}

QColor colorFromString(const QString &value, const QColor &fallback) {
    const QColor color = QColor::fromString(value);
    return color.isValid() ? color : fallback;
}

QJsonObject objectToJson(const SceneObject &object) {
    return QJsonObject::fromVariantMap(Design::properties(object));
}
SceneObject objectFromJson(const QJsonObject &json) {
    SceneObject object;
    object.id = json.value("id").toString();
    const auto type = json.value("type").toString();
    object.type = type == "text" ? ObjectType::Text : type == "image" ? ObjectType::Image : type == "rect" ? ObjectType::Rect : type == "media" ? ObjectType::Media : type == "table" ? ObjectType::Table : type == "chart" ? ObjectType::Chart : ObjectType(-1);
    const auto known=Design::properties(object);
    const QSet<QString> structural={"id","type","placeholderId","groups","overrides","runs"};
    for (auto it = json.begin(); it != json.end(); ++it) {
        if(structural.contains(it.key())) continue;
        if(!known.contains(it.key()) || !Design::setProperty(object,it.key(),it.value().toVariant(),false)) object.type=ObjectType(-1);
    }
    // Parsing preserves authored geometry exactly; editing clamps new boxes.
    object.rect = QRectF(json.value("x").toDouble(), json.value("y").toDouble(),
                         json.value("w").toDouble(), json.value("h").toDouble());
    object.placeholderId = json.value("placeholderId").toString();
    for (const auto &group : json.value("groups").toArray()) object.groups.append(group.toString());
    for (const auto &key : json.value("overrides").toArray()) object.overrides.append(key.toString());
    if (json.contains("runs")) {
        QVector<TextRun> runs;
        if (!TextRuns::decode(json.value("runs").toVariant(), runs)) object.type = ObjectType(-1);
        else object.runs = TextRuns::tidy(runs, object.text.size());
    }
    return object;
}

QString effectToString(Effect effect) {
    switch (effect) {
    case Effect::Media: return QStringLiteral("media");
    case Effect::Fade: return QStringLiteral("fade");
    case Effect::Rise: return QStringLiteral("rise");
    case Effect::Move: return QStringLiteral("move");
    case Effect::Scale: return QStringLiteral("scale");
    case Effect::Spin: return QStringLiteral("spin");
    case Effect::Pulse: return QStringLiteral("pulse");
    case Effect::Reveal: return QStringLiteral("reveal");
    case Effect::Path: return QStringLiteral("path");
    case Effect::None: break;
    }
    return QStringLiteral("none");
}

Effect effectFromString(const QString &value) {
    if (value == QLatin1String("media")) return Effect::Media;
    if (value == QLatin1String("fade")) return Effect::Fade;
    if (value == QLatin1String("rise")) return Effect::Rise;
    if (value == QLatin1String("move")) return Effect::Move;
    if (value == QLatin1String("scale")) return Effect::Scale;
    if (value == QLatin1String("spin")) return Effect::Spin;
    if (value == QLatin1String("pulse")) return Effect::Pulse;
    if (value == QLatin1String("reveal")) return Effect::Reveal;
    if (value == QLatin1String("path")) return Effect::Path;
    return Effect::None;
}

QJsonObject stepToJson(const BuildStep &step) {
    QJsonObject json;
    json[QStringLiteral("target")] = step.targetId;
    json[QStringLiteral("effect")] = effectToString(step.effect);
    json[QStringLiteral("start")] = step.start;
    json["phase"] = int(step.phase);
    json["trigger"] = int(step.trigger);
    json["delay"] = step.delay;
    json[QStringLiteral("duration")] = step.duration;
    json[QStringLiteral("easing")] = int(step.easing);
    json["amountX"] = step.amountX;
    json["amountY"] = step.amountY;
    json["amount"] = step.amount;
    json["unit"] = step.unit;
    json["pathId"] = step.pathId;
    json["pathReverse"] = step.pathReverse;
    json["orient"] = step.orient;
    return json;
}

BuildStep stepFromJson(const QJsonObject &json) {
    BuildStep step;
    step.targetId = json.value(QStringLiteral("target")).toString();
    step.effect = effectFromString(json.value(QStringLiteral("effect")).toString());
    step.start = qMax(0.0, json.value(QStringLiteral("start")).toDouble());
    step.phase = BuildPhase(qBound(0, json.value("phase").toInt(), 1));
    step.trigger = BuildTrigger(qBound(0, json.value("trigger").toInt(), 3));
    step.delay = qMax(0.0, json.value("delay").toDouble());
    step.duration = json.value(QStringLiteral("duration")).toDouble(0.6);
    step.easing = QEasingCurve::Type(json.value(QStringLiteral("easing"))
                                         .toInt(int(QEasingCurve::OutCubic)));
    step.amountX = json.value("amountX").toDouble();
    step.amountY = json.value("amountY").toDouble();
    step.amount = json.value("amount").toDouble();
    step.unit = qBound(0, json.value("unit").toInt(), 2);
    step.pathId = json.value("pathId").toString();
    step.pathReverse = json.value("pathReverse").toBool();
    step.orient = json.value("orient").toBool();
    return step;
}

QByteArray slideToJson(const Slide &slide) {
    QJsonObject json;
    json[QStringLiteral("id")] = slide.id;
    json[QStringLiteral("background")] = colorToString(slide.background);
    json["layoutId"] = slide.layoutId;
    json["sectionId"] = slide.sectionId;
    json["notes"] = slide.notes;
    json["skipped"] = slide.skipped;
    json["backgroundOverride"] = slide.backgroundOverride;
    json["showMasterObjects"] = slide.showMasterObjects;
    json["showMasterFields"] = slide.showMasterFields;
    json["transition"] = slide.transition;
    json["transitionDirection"] = slide.transitionDirection;
    json["transitionSeconds"] = slide.transitionSeconds;
    json["advanceAfter"] = slide.advanceAfter;
    QJsonArray reading;
    for (const auto &id : slide.readingOrder) reading.append(id);
    json["readingOrder"] = reading;

    QJsonArray objects;
    for (const SceneObject &object : slide.objects)
        objects.append(objectToJson(object));
    json[QStringLiteral("objects")] = objects;

    QJsonArray builds;
    for (const BuildStep &step : slide.timeline.steps)
        builds.append(stepToJson(step));
    json[QStringLiteral("builds")] = builds;

    return QJsonDocument(json).toJson(QJsonDocument::Indented);
}

Slide slideFromJson(const QByteArray &raw, bool *ok) {
    Slide slide;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(raw, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (ok) *ok = false;
        return slide;
    }

    const QJsonObject json = document.object();
    if ((json.contains("showMasterObjects") && !json.value("showMasterObjects").isBool()) ||
        (json.contains("showMasterFields") && !json.value("showMasterFields").isBool()) ||
        (json.contains("readingOrder") && !json.value("readingOrder").isArray())) {
        if (ok) *ok = false;
        return slide;
    }
    for (const auto &key : {"transition", "transitionDirection", "transitionSeconds", "advanceAfter"})
        if (json.contains(key) && !json.value(key).isDouble()) { if (ok) *ok = false; return slide; }
    slide.transition = json.value("transition").toInt(-1);
    slide.transitionDirection = json.value("transitionDirection").toInt(0);
    slide.transitionSeconds = json.value("transitionSeconds").toDouble(-1);
    slide.advanceAfter = json.value("advanceAfter").toDouble(-1);
    if (slide.transition < -1 || slide.transition > 3 ||
        slide.transitionDirection < 0 || slide.transitionDirection > 3 ||
        !std::isfinite(slide.transitionSeconds) || slide.transitionSeconds < -1 ||
        slide.transitionSeconds > 10 || !std::isfinite(slide.advanceAfter) ||
        slide.advanceAfter < -1 || slide.advanceAfter > 3600) {
        if (ok) *ok = false;
        return slide;
    }
    for (const auto &value : json.value("readingOrder").toArray()) {
        const auto id = value.toString();
        if (id.isEmpty() || slide.readingOrder.contains(id)) { if (ok) *ok = false; return slide; }
        slide.readingOrder.append(id);
    }
    slide.id = json.value(QStringLiteral("id")).toString();
    slide.layoutId = json.value("layoutId").toString();
    slide.sectionId = json.value("sectionId").toString();
    slide.notes = json.value("notes").toString();
    slide.skipped = json.value("skipped").toBool();
    slide.backgroundOverride = json.value("backgroundOverride").toBool();
    slide.showMasterObjects = json.value("showMasterObjects").toBool(true);
    slide.showMasterFields = json.value("showMasterFields").toBool(true);
    slide.background = colorFromString(json.value(QStringLiteral("background")).toString(),
                                       QColor(11, 22, 38));

    const QJsonArray objects = json.value(QStringLiteral("objects")).toArray();
    for (const QJsonValue &value : objects)
        slide.objects.append(objectFromJson(value.toObject()));

    const QJsonArray builds = json.value(QStringLiteral("builds")).toArray();
    for (const QJsonValue &value : builds) {
        const auto object = value.toObject();
        for (const auto &key : {"amountX", "amountY", "amount", "unit"})
            if (object.contains(key) && !object.value(key).isDouble()) { if (ok) *ok = false; return slide; }
        for (const auto &key : {"pathReverse", "orient"})
            if (object.contains(key) && !object.value(key).isBool()) { if (ok) *ok = false; return slide; }
        const auto step = stepFromJson(object);
        if (!std::isfinite(step.amountX) || !std::isfinite(step.amountY) ||
            !std::isfinite(step.amount) || std::abs(step.amountX) > 100000 ||
            std::abs(step.amountY) > 100000 || std::abs(step.amount) > 100000) {
            if (ok) *ok = false;
            return slide;
        }
        slide.timeline.steps.append(step);
    }

    if (ok) *ok = true;
    return slide;
}

} // namespace

QByteArray Bundle::toBytes(const Document &document, const QByteArray &recoveryMetadata) {
    QVector<Zip::Entry> entries;
    if (!recoveryMetadata.isEmpty()) entries.append({"recovery.json", recoveryMetadata, true});

    QJsonObject manifest;
    manifest[QStringLiteral("version")] = kFormatVersion;
    manifest[QStringLiteral("width")] = document.size.width();
    manifest[QStringLiteral("height")] = document.size.height();
    manifest[QStringLiteral("transitionDuration")] = document.transitionDuration;
    manifest[QStringLiteral("transition")] = document.transition;

    QJsonArray order;
    for (const Slide &slide : document.slides)
        order.append(slide.id);
    manifest[QStringLiteral("slides")] = order;
    QJsonArray sections;
    for (const auto &section : document.sections) sections.append(QJsonObject{{"id",section.id},{"name",section.name}});
    manifest["sections"] = sections;
    QJsonArray shows;
    for (const auto &show : document.shows) {
        QJsonArray slides;
        for (const auto &id : show.slideIds) slides.append(id);
        shows.append(QJsonObject{{"id",show.id},{"name",show.name},{"slides",slides}});
    }
    manifest["shows"] = shows;

    QJsonObject theme, colors, fonts;
    theme["name"] = document.theme.name;
    for (auto it = document.theme.colors.cbegin(); it != document.theme.colors.cend(); ++it)
        colors[it.key()] = colorToString(it.value());
    for (auto it = document.theme.fonts.cbegin(); it != document.theme.fonts.cend(); ++it)
        fonts[it.key()] = it.value();
    theme["colors"] = colors; theme["fonts"] = fonts;
    entries.append({"theme.json", QJsonDocument(theme).toJson(), true});
    QJsonArray masters, layouts;
    for (const auto &m : document.masters) {
        QJsonArray objects;
        for (const auto &o : m.objects) objects.append(objectToJson(o));
        masters.append(QJsonObject{{"id",m.id},{"name",m.name},{"background",colorToString(m.background)},
                                   {"backgroundToken",m.backgroundToken},{"objects",objects},
                                   {"fields",QJsonObject::fromVariantMap(Design::fieldProperties(m.fields))}});
    }
    for (const auto &l : document.layouts) {
        QJsonArray objects;
        for (const auto &o : l.placeholders) objects.append(objectToJson(o));
        layouts.append(QJsonObject{{"id",l.id},{"name",l.name},{"masterId",l.masterId},{"placeholders",objects}});
    }
    entries.append({"masters/design.json", QJsonDocument(QJsonObject{{"masters",masters},{"layouts",layouts}}).toJson(), true});

    entries.append({QStringLiteral("document.json"),
                    QJsonDocument(manifest).toJson(QJsonDocument::Indented), true});

    for (const Slide &slide : document.slides) {
        entries.append({QStringLiteral("slides/%1.json").arg(slide.id),
                        slideToJson(slide), true});
    }
    QJsonArray styles;
    for(const auto &style:document.objectStyles) styles.append(QJsonObject{{"id",style.id},{"name",style.name},{"appearance",objectToJson(style.appearance)}});
    entries.append({"styles.json",QJsonDocument(styles).toJson(),true});
    QJsonArray comments;
    for (const auto &comment : document.comments)
        comments.append(QJsonObject{{"id",comment.id},{"slideId",comment.slideId},
                                    {"objectId",comment.objectId},{"parentId",comment.parentId},
                                    {"author",comment.author},{"created",comment.created},
                                    {"text",comment.text},{"resolved",comment.resolved}});
    QJsonArray dismissed;
    for (const auto &key : document.dismissedIssues) dismissed.append(key);
    entries.append({"review.json",QJsonDocument(QJsonObject{{"comments",comments},
                                                           {"dismissed",dismissed}}).toJson(),true});
    QMap<QString,QByteArray> assets;
    std::function<void(const QVector<SceneObject>&)> collect;
    collect = [&assets,&collect](const QVector<SceneObject> &objects) {
        for (const auto &o : objects) {
            if(o.mediaOriginal) collect({*o.mediaOriginal});
            if(o.imageOriginal) collect({*o.imageOriginal});
            if (!o.imageId.isEmpty()) assets[o.imageId+"."+o.imageFormat] = o.imageData;
            if(o.type==ObjectType::Media && o.mediaPath.isEmpty()) assets[o.mediaId+".media"]=o.mediaData;
        }
    };
    for (const auto &style:document.objectStyles) collect({style.appearance});
    for (const auto &m : document.masters) collect(m.objects);
    for (const auto &l : document.layouts) collect(l.placeholders);
    for (const auto &slide : document.slides) collect(slide.objects);
    for (auto it=assets.cbegin();it!=assets.cend();++it)
        entries.append({"assets/"+it.key(),it.value(),false});
    return Zip::write(entries);
}

Bundle::ReadResult Bundle::fromBytes(const QByteArray &raw) {
    ReadResult result;

    const Zip::Reader reader(raw);
    if (!reader.isValid()) {
        result.error = QStringLiteral("This is not an OmaShow deck (%1).").arg(reader.error());
        return result;
    }
    if (!reader.contains(QStringLiteral("document.json"))) {
        result.error = QStringLiteral("The deck has no document.json.");
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument manifestDocument =
        QJsonDocument::fromJson(reader.read(QStringLiteral("document.json")), &parseError);
    if (parseError.error != QJsonParseError::NoError || !manifestDocument.isObject()) {
        result.error = QStringLiteral("The deck's manifest is damaged.");
        return result;
    }

    const QJsonObject manifest = manifestDocument.object();
    const int version = manifest.value(QStringLiteral("version")).toInt();
    if (version > kFormatVersion) {
        // Refuse rather than half-read: a newer file may carry objects, builds
        // or properties this build would silently drop on the next save.
        result.error = QStringLiteral("This deck was written by a newer OmaShow "
                                      "(format %1, this build reads %2).")
                           .arg(version).arg(kFormatVersion);
        return result;
    }
    if (version < 1) {
        result.error = QStringLiteral("The deck has an invalid format version.");
        return result;
    }
    // Version 1 migrates as literal slides, with no layout or theme links.
    // Its original pixels are preserved.

    Document document;
    document.size = QSizeF(manifest.value(QStringLiteral("width")).toDouble(1920.0),
                           manifest.value(QStringLiteral("height")).toDouble(1080.0));
    document.transitionDuration =
        manifest.value(QStringLiteral("transitionDuration")).toDouble(0.9);
    document.transition = manifest.value(QStringLiteral("transition")).toInt(3);
    if (!std::isfinite(document.transitionDuration) || document.transitionDuration < 0 ||
        document.transitionDuration > 10 || document.transition < 0 || document.transition > 3) {
        result.error = QStringLiteral("The deck has an invalid transition."); return result;
    }

    if (!std::isfinite(document.size.width()) || !std::isfinite(document.size.height()) ||
        document.size.width()<=0 || document.size.height()<=0 ||
        document.size.width()>100000 || document.size.height()>100000) {
        result.error=QStringLiteral("The deck has invalid dimensions."); return result;
    }
    if (version >= 2) {
        auto readJson = [&reader](const QString &member, QJsonObject *out) {
            QJsonParseError error;
            const auto doc = QJsonDocument::fromJson(reader.read(member), &error);
            if (error.error != QJsonParseError::NoError || !doc.isObject()) return false;
            *out = doc.object(); return true;
        };
        QJsonObject theme, design;
        if (!readJson("theme.json", &theme) || !readJson("masters/design.json", &design)) {
            result.error = QStringLiteral("The deck's theme or masters are missing or damaged."); return result;
        }
        document.theme.name = theme.value("name").toString();
        const auto colors = theme.value("colors").toObject(), fonts = theme.value("fonts").toObject();
        for (auto it = colors.begin(); it != colors.end(); ++it) {
            const QColor color(it.value().toString());
            if (!color.isValid()) { result.error = QStringLiteral("Invalid theme colour."); return result; }
            document.theme.colors[it.key()] = color;
        }
        for (auto it = fonts.begin(); it != fonts.end(); ++it) document.theme.fonts[it.key()] = it.value().toString();
        QSet<QString> masterIds, layoutIds;
        for (const auto &value : design.value("masters").toArray()) {
            const auto json = value.toObject(); Master m;
            m.id = json.value("id").toString(); m.name = json.value("name").toString();
            m.background = colorFromString(json.value("background").toString(), m.background);
            m.backgroundToken = json.value("backgroundToken").toString();
            if (json.contains("fields") && !json.value("fields").isObject()) {
                result.error = QStringLiteral("Invalid master fields."); return result;
            }
            const auto fields = json.value("fields").toObject();
            for (auto it = fields.begin(); it != fields.end(); ++it)
                if (!Design::setFieldProperty(m.fields, it.key(), it.value().toVariant())) {
                    result.error = QStringLiteral("Invalid master field: %1.").arg(it.key()); return result;
                }
            if (m.id.isEmpty() || masterIds.contains(m.id)) {
                result.error = QStringLiteral("Invalid or duplicate master."); return result;
            }
            masterIds.insert(m.id);
            for (const auto &o : json.value("objects").toArray()) m.objects.append(objectFromJson(o.toObject()));
            document.masters.append(m);
        }
        for (const auto &value : design.value("layouts").toArray()) {
            const auto json = value.toObject(); SlideLayout l;
            l.id = json.value("id").toString(); l.name = json.value("name").toString(); l.masterId = json.value("masterId").toString();
            if (l.id.isEmpty() || layoutIds.contains(l.id) || !masterIds.contains(l.masterId)) {
                result.error = QStringLiteral("Invalid layout or missing master reference."); return result;
            }
            layoutIds.insert(l.id); QSet<QString> roles;
            for (const auto &value : json.value("placeholders").toArray()) {
                const auto o = objectFromJson(value.toObject());
                if (o.id.isEmpty() || roles.contains(o.id)) {
                    result.error = QStringLiteral("Duplicate or unnamed placeholder."); return result;
                }
                roles.insert(o.id); l.placeholders.append(o);
            }
            document.layouts.append(l);
        }
    }

    QSet<QString> sectionIds;
    for (const auto &value : manifest.value("sections").toArray()) {
        const auto json=value.toObject(); Section section{json.value("id").toString(),json.value("name").toString()};
        if (section.id.isEmpty() || sectionIds.contains(section.id)) {
            result.error=QStringLiteral("Invalid or duplicate section."); return result;
        }
        sectionIds.insert(section.id); document.sections.append(section);
    }
    if (manifest.contains("shows") && !manifest.value("shows").isArray()) {
        result.error = QStringLiteral("The deck's custom shows are damaged."); return result;
    }
    const QJsonArray order = manifest.value(QStringLiteral("slides")).toArray();
    for (const QJsonValue &value : order) {
        const QString id = value.toString();
        const QString member = QStringLiteral("slides/%1.json").arg(id);
        if (!reader.contains(member)) {
            result.error = QStringLiteral("The deck is missing slide %1.").arg(id);
            return result;
        }
        bool ok = false;
        const Slide slide = slideFromJson(reader.read(member), &ok);
        if (!ok) {
            result.error = QStringLiteral("Slide %1 is damaged.").arg(id);
            return result;
        }
        if (!slide.sectionId.isEmpty() && !sectionIds.contains(slide.sectionId)) {
            result.error=QStringLiteral("Slide %1 refers to a missing section.").arg(id); return result;
        }
        const auto *layout = Design::layout(document, slide.layoutId);
        if (!slide.layoutId.isEmpty() && !layout) {
            result.error = QStringLiteral("Slide %1 refers to a missing layout.").arg(id); return result;
        }
        for (const auto &o : slide.objects) {
            if (o.placeholderId.isEmpty()) continue;
            bool found = false;
            if (layout) for (const auto &p : layout->placeholders)
                if (p.id == o.placeholderId && p.type == o.type) found = true;
            if (!found) { result.error = QStringLiteral("Slide %1 has a missing placeholder.").arg(id); return result; }
        }
        for (const auto &object : slide.readingOrder)
            if (!slide.find(object)) {
                result.error = QStringLiteral("Slide %1 reads an object it does not have.").arg(id);
                return result;
            }
        document.slides.append(slide);
    }

    if (document.slides.isEmpty()) {
        result.error = QStringLiteral("The deck has no slides.");
        return result;
    }

    QSet<QString> showIds;
    for (const auto &value : manifest.value("shows").toArray()) {
        const auto json = value.toObject();
        CustomShow show{json.value("id").toString(), json.value("name").toString(), {}};
        if (show.id.isEmpty() || showIds.contains(show.id) || show.name.trimmed().isEmpty() ||
            show.name.size() > 120 || !json.value("slides").isArray()) {
            result.error = QStringLiteral("A custom show is damaged."); return result;
        }
        showIds.insert(show.id);
        for (const auto &slide : json.value("slides").toArray()) {
            const auto id = slide.toString();
            bool exists = false;
            for (const auto &known : document.slides) if (known.id == id) exists = true;
            if (!exists || show.slideIds.contains(id)) {
                result.error = QStringLiteral("A custom show names a slide twice, or one that "
                                              "is not in the deck.");
                return result;
            }
            show.slideIds.append(id);
        }
        document.shows.append(show);
    }

    // Review notes travel with the deck. One that points at a slide or object
    // that is not there is a damaged deck, not something to drop quietly.
    if (version >= 13 && reader.contains("review.json")) {
        QJsonParseError error;
        const auto parsed = QJsonDocument::fromJson(reader.read("review.json"), &error);
        if (error.error != QJsonParseError::NoError || !parsed.isObject()) {
            result.error = QStringLiteral("The deck's review notes are damaged."); return result;
        }
        const auto json = parsed.object();
        if (!json.value("comments").isArray() || !json.value("dismissed").isArray()) {
            result.error = QStringLiteral("The deck's review notes are damaged."); return result;
        }
        for (const auto &value : json.value("comments").toArray()) {
            const auto row = value.toObject();
            if (row.contains("resolved") && !row.value("resolved").isBool()) {
                result.error = QStringLiteral("A review comment is damaged."); return result;
            }
            Comment comment;
            comment.id = row.value("id").toString();
            comment.slideId = row.value("slideId").toString();
            comment.objectId = row.value("objectId").toString();
            comment.parentId = row.value("parentId").toString();
            comment.author = row.value("author").toString();
            comment.created = row.value("created").toString();
            comment.text = row.value("text").toString();
            comment.resolved = row.value("resolved").toBool();
            document.comments.append(comment);
        }
        const auto invalid = Review::validate(document);
        if (!invalid.isEmpty()) { result.error = invalid; return result; }
        for (const auto &value : json.value("dismissed").toArray()) {
            const auto key = value.toString();
            if (key.isEmpty() || document.dismissedIssues.contains(key)) {
                result.error = QStringLiteral("A dismissed review finding is damaged."); return result;
            }
            document.dismissedIssues.append(key);
        }
    }

    QMap<QString,SceneObject> decoded;
    qint64 decodedBytes=0;
    std::function<bool(QVector<SceneObject>&)> loadAssets;
    loadAssets = [&](QVector<SceneObject> &objects) {
        for (auto &o : objects) {
            if(o.imageOriginal) {
                QVector<SceneObject> originals{*o.imageOriginal};
                if(!loadAssets(originals)) return false;
                o.imageOriginal=std::make_shared<const SceneObject>(originals.first());
            }
            if(o.mediaOriginal) {
                QVector<SceneObject> originals{*o.mediaOriginal};
                if(!loadAssets(originals)) return false;
                o.mediaOriginal=std::make_shared<const SceneObject>(originals.first());
            }
            if (o.type!=ObjectType::Rect && o.type!=ObjectType::Text && o.type!=ObjectType::Image && o.type!=ObjectType::Media && o.type!=ObjectType::Table && o.type!=ObjectType::Chart) {
                result.error=QStringLiteral("The deck contains an unsupported object type."); return false;
            }
            if(!LinkedData::validate(o.dataSource) || (!o.dataSource.path.isEmpty() && o.type!=ObjectType::Table && o.type!=ObjectType::Chart)) { result.error="Invalid linked data source."; return false; }
            if(o.type==ObjectType::Chart && !Chart::validate(o,&result.error)) return false;
            if(o.type==ObjectType::Table && (o.table.rows.isEmpty() || !Table::validate(o.table))) { result.error="Invalid native table."; return false; }
            if(o.type!=ObjectType::Table && o.type!=ObjectType::Chart && !o.table.cells.isEmpty()) { result.error="Table cells belong to a table object."; return false; }
            if(o.type==ObjectType::Media) {
                if(o.mediaPath.isEmpty()) o.mediaData=reader.read("assets/"+o.mediaId+".media");
                if(!MediaAsset::validate(o,&result.error)) return false;
            }
            const auto linkError=Links::validate(o.linkKind,o.linkTarget,document,true);
            if(!linkError.isEmpty()) { result.error=linkError; return false; }
            if (o.type!=ObjectType::Image && o.imageId.isEmpty()) continue;
            if (!QRegularExpression("^[0-9a-f]{64}$").match(o.imageId).hasMatch()) {
                result.error=QStringLiteral("A picture has an invalid asset reference."); return false;
            }
            if (!decoded.contains(o.imageId)) {
                const auto bytes=reader.read("assets/"+o.imageId+"."+o.imageFormat);
                if (ImageAsset::identity(bytes)!=o.imageId) { result.error=QStringLiteral("A picture is missing or damaged."); return false; }
                SceneObject asset;
                if (!ImageAsset::decode(asset,bytes,&result.error)) return false;
                if(version<=7 && o.imageFormat=="png" && (asset.imageFormat=="jpg" || asset.imageFormat=="webp")) o.imageFormat=asset.imageFormat;
                if(asset.imageFormat!=o.imageFormat) { result.error=QStringLiteral("Image asset format does not match its reference."); return false; }
                decodedBytes += asset.image.sizeInBytes()+asset.vectorPicture.size();
                if (decodedBytes>512*1024*1024) { result.error=QStringLiteral("This deck contains too much decoded image data."); return false; }
                decoded[o.imageId]=asset;
            }
            const auto &asset=decoded[o.imageId];
            if(version<=7 && o.imageFormat=="png" && (asset.imageFormat=="jpg" || asset.imageFormat=="webp")) o.imageFormat=asset.imageFormat;
            if(asset.imageFormat!=o.imageFormat) { result.error=QStringLiteral("Inconsistent image asset format."); return false; }
            ImageAsset::copyData(o,asset);
        }
        return true;
    };
    if(reader.names().contains("styles.json")) {
        QJsonParseError error; const auto json=QJsonDocument::fromJson(reader.read("styles.json"),&error);
        if(error.error!=QJsonParseError::NoError || !json.isArray() || json.array().size()>1000) { result.error="Invalid object styles."; return result; }
        QSet<QString> ids;
        for(const auto &value:json.array()) { const auto row=value.toObject(); ObjectStyle style; style.id=row.value("id").toString(); style.name=row.value("name").toString(); style.appearance=objectFromJson(row.value("appearance").toObject());
            if(style.id.isEmpty() || style.name.trimmed().isEmpty() || ids.contains(style.id)) { result.error="An object style has an invalid identity."; return result; }
            QVector<SceneObject> objects{style.appearance}; if(!loadAssets(objects)) return result; style.appearance=objects.first(); ids.insert(style.id); document.objectStyles.append(style);
        }
    }
    for (auto &m : document.masters) if (!loadAssets(m.objects)) return result;
    for (auto &l : document.layouts) if (!loadAssets(l.placeholders)) return result;
    for (auto &slide : document.slides) if (!loadAssets(slide.objects)) return result;

    for(const auto &slide:document.slides) {
        QSet<QString> mediaCues;
        for(const auto &step:slide.timeline.steps) if(step.effect==Effect::Media) {
            const auto *o=slide.find(step.targetId);
            if(!o || o->type!=ObjectType::Media || mediaCues.contains(o->id) || step.phase!=BuildPhase::In || !std::isfinite(step.duration) || std::abs(step.duration-MediaAsset::playbackDuration(*o))>.0001) { result.error="Invalid or duplicate media cue."; return result; }
            mediaCues.insert(o->id);
        }
    }
    result.document = document;
    result.ok = true;
    return result;
}

bool Bundle::save(const Document &document, const QString &path, QString *error) {
    // QSaveFile is the atomic write: a temporary beside the target, then a
    // rename on commit. An interrupted save leaves the old deck untouched.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    const QByteArray raw = toBytes(document);
    if (file.write(raw) != raw.size()) {
        if (error) *error = file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

Bundle::ReadResult Bundle::load(const QString &path) {
    ReadResult result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = file.errorString();
        return result;
    }
    return fromBytes(file.readAll());
}
