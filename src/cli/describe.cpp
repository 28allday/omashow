#include "cli/describe.h"

#include "anim/presentation.h"
#include "core/design.h"
#include "core/review.h"
#include "core/spelling.h"
#include "io/bundle.h"
#include "render/textlayout.h"

namespace {

// Left out unless asked for: each can be pages long on its own.
const QStringList &heavy() {
    static const QStringList keys{QStringLiteral("pathData"), QStringLiteral("table"),
                                  QStringLiteral("chart"),    QStringLiteral("dataSource"),
                                  QStringLiteral("runs"),     QStringLiteral("imageOriginal"),
                                  QStringLiteral("mediaOriginal")};
    return keys;
}

QString effectName(Effect effect) {
    switch (effect) {
    case Effect::None: return QStringLiteral("none");
    case Effect::Fade: return QStringLiteral("fade");
    case Effect::Rise: return QStringLiteral("rise");
    case Effect::Media: return QStringLiteral("media");
    case Effect::Move: return QStringLiteral("move");
    case Effect::Scale: return QStringLiteral("scale");
    case Effect::Spin: return QStringLiteral("spin");
    case Effect::Pulse: return QStringLiteral("pulse");
    case Effect::Reveal: return QStringLiteral("reveal");
    case Effect::Path: return QStringLiteral("path");
    }
    return QStringLiteral("none");
}

QString triggerName(BuildTrigger trigger) {
    switch (trigger) {
    case BuildTrigger::Absolute: return QStringLiteral("absolute");
    case BuildTrigger::OnClick: return QStringLiteral("onClick");
    case BuildTrigger::WithPrevious: return QStringLiteral("withPrevious");
    case BuildTrigger::AfterPrevious: return QStringLiteral("afterPrevious");
    }
    return QStringLiteral("absolute");
}

QString transitionName(int kind) {
    switch (kind) {
    case 0: return QStringLiteral("cut");
    case 1: return QStringLiteral("fade");
    case 2: return QStringLiteral("push");
    default: return QStringLiteral("morph");
    }
}

QVariantMap describeObject(const Document &d, const SceneObject &authored,
                           const SceneObject &shown, bool full) {
    auto values = Design::properties(shown);
    if (!full)
        for (const auto &key : heavy()) values.remove(key);
    // What is the box's own, and what it is taking from a layout or a style.
    values[QStringLiteral("overrides")] = authored.overrides;
    values[QStringLiteral("fromPlaceholder")] = !authored.placeholderId.isEmpty();
    if (shown.type == ObjectType::Text) {
        const auto metrics = TextLayout::measure(shown);
        values[QStringLiteral("textOverflow")] = metrics.overflow;
        values[QStringLiteral("language")] = Review::language(d, &shown);
    }
    if (shown.type == ObjectType::Table)
        values[QStringLiteral("tableSize")] =
            QVariantMap{{QStringLiteral("rows"), shown.table.rows.size()},
                        {QStringLiteral("columns"), shown.table.columns.size()}};
    return values;
}

} // namespace

QVariantMap Cli::describeSlide(const Document &d, int index, bool full) {
    QVariantMap row;
    if (index < 0 || index >= d.slides.size()) return row;
    const auto &authored = d.slides.at(index);
    const auto shown = Design::resolve(d, index);
    QVariantList objects;
    for (const auto &object : shown.objects) {
        const auto *own = authored.find(object.id);
        if (!own) continue;   // master artwork and fields belong to the master
        objects.append(describeObject(d, *own, object, full));
    }
    QVariantList builds;
    for (const auto &step : authored.timeline.steps)
        builds.append(QVariantMap{{QStringLiteral("targetId"), step.targetId},
                                  {QStringLiteral("effect"), effectName(step.effect)},
                                  {QStringLiteral("phase"),
                                   step.phase == BuildPhase::In ? QStringLiteral("in")
                                                                : QStringLiteral("out")},
                                  {QStringLiteral("trigger"), triggerName(step.trigger)},
                                  {QStringLiteral("start"), step.start},
                                  {QStringLiteral("duration"), step.duration},
                                  {QStringLiteral("delay"), step.delay},
                                  {QStringLiteral("pathId"), step.pathId}});
    QString title;
    for (const auto &object : shown.objects)
        if (object.type == ObjectType::Text && !object.text.trimmed().isEmpty() &&
            !object.id.startsWith(QStringLiteral("@field/"))) {
            title = object.text.section('\n', 0, 0);
            break;
        }
    row = QVariantMap{
        {QStringLiteral("index"), index},
        {QStringLiteral("id"), authored.id},
        {QStringLiteral("title"), title},
        {QStringLiteral("layoutId"), authored.layoutId},
        {QStringLiteral("sectionId"), authored.sectionId},
        {QStringLiteral("skipped"), authored.skipped},
        {QStringLiteral("notes"), authored.notes},
        {QStringLiteral("transition"), transitionName(Presentation::transitionKind(d, index))},
        {QStringLiteral("transitionIsItsOwn"), authored.transition >= 0},
        {QStringLiteral("advanceAfter"), authored.advanceAfter},
        {QStringLiteral("objects"), objects},
        {QStringLiteral("builds"), builds}};
    return row;
}

QVariantMap Cli::describeDeck(const Document &d, bool full) {
    QVariantMap colours, fonts;
    for (auto it = d.theme.colors.cbegin(); it != d.theme.colors.cend(); ++it)
        colours[it.key()] = it.value().name(QColor::HexArgb);
    for (auto it = d.theme.fonts.cbegin(); it != d.theme.fonts.cend(); ++it)
        fonts[it.key()] = it.value();
    QVariantList masters, layouts, sections, shows, styles;
    for (const auto &master : d.masters)
        masters.append(QVariantMap{{QStringLiteral("id"), master.id},
                                   {QStringLiteral("name"), master.name}});
    for (const auto &layout : d.layouts) {
        QVariantList placeholders;
        for (const auto &object : layout.placeholders)
            if (!object.placeholderId.isEmpty())
                placeholders.append(object.placeholderId);
        layouts.append(QVariantMap{{QStringLiteral("id"), layout.id},
                                   {QStringLiteral("name"), layout.name},
                                   {QStringLiteral("masterId"), layout.masterId},
                                   {QStringLiteral("placeholders"), placeholders}});
    }
    for (const auto &section : d.sections)
        sections.append(QVariantMap{{QStringLiteral("id"), section.id},
                                    {QStringLiteral("name"), section.name}});
    for (const auto &show : d.shows)
        shows.append(QVariantMap{{QStringLiteral("id"), show.id},
                                 {QStringLiteral("name"), show.name},
                                 {QStringLiteral("slides"), show.slideIds}});
    for (const auto &style : d.textStyles)
        styles.append(QVariantMap{{QStringLiteral("id"), style.id},
                                  {QStringLiteral("name"), style.name}});
    QVariantList slides;
    for (int i = 0; i < d.slides.size(); ++i) slides.append(describeSlide(d, i, full));
    return QVariantMap{
        {QStringLiteral("format"), Bundle::kFormatVersion},
        {QStringLiteral("size"), QVariantMap{{QStringLiteral("width"), d.size.width()},
                                             {QStringLiteral("height"), d.size.height()}}},
        {QStringLiteral("theme"), QVariantMap{{QStringLiteral("name"), d.theme.name},
                                              {QStringLiteral("colors"), colours},
                                              {QStringLiteral("fonts"), fonts}}},
        {QStringLiteral("language"), Review::language(d)},
        {QStringLiteral("smartPunctuation"), d.smartPunctuation},
        {QStringLiteral("knownWords"), d.knownWords},
        {QStringLiteral("transition"), transitionName(d.transition)},
        {QStringLiteral("transitionSeconds"), d.transitionDuration},
        {QStringLiteral("masters"), masters},
        {QStringLiteral("layouts"), layouts},
        {QStringLiteral("sections"), sections},
        {QStringLiteral("shows"), shows},
        {QStringLiteral("textStyles"), styles},
        {QStringLiteral("slides"), slides},
        {QStringLiteral("statistics"), Review::statistics(d)}};
}

QVariantMap Cli::describeReview(const Document &d, bool includeDismissed) {
    QVariantList findings;
    for (const auto &row : Review::issues(d)) {
        const auto issue = row.toMap();
        if (!includeDismissed && issue.value(QStringLiteral("dismissed")).toBool()) continue;
        findings.append(issue);
    }
    const auto language = Review::language(d);
    return QVariantMap{
        {QStringLiteral("findings"), findings},
        {QStringLiteral("statistics"), Review::statistics(d)},
        {QStringLiteral("language"), language},
        {QStringLiteral("spellingAvailable"), Spelling::available(language)},
        {QStringLiteral("dictionaries"), Spelling::installed()}};
}
