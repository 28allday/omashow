#include "cli/describe.h"

#include "anim/presentation.h"
#include "core/chart.h"
#include "core/shape.h"
#include "core/table.h"
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
    case 4: return QStringLiteral("cover");
    case 5: return QStringLiteral("uncover");
    case 6: return QStringLiteral("fade-through-black");
    case 7: return QStringLiteral("zoom");
    case 8: return QStringLiteral("whirl");
    default: return QStringLiteral("morph");
    }
}

QString typeName(ObjectType type) {
    switch (type) {
    case ObjectType::Text: return QStringLiteral("text");
    case ObjectType::Image: return QStringLiteral("image");
    case ObjectType::Media: return QStringLiteral("media");
    case ObjectType::Table: return QStringLiteral("table");
    case ObjectType::Chart: return QStringLiteral("chart");
    case ObjectType::Rect: break;
    }
    return QStringLiteral("rect");
}

// What a brand-new object of this kind says about itself. A property still at
// that value tells a reader nothing, so the compact reading leaves it out.
const QVariantMap &blank(ObjectType type) {
    static QHash<int, QVariantMap> cache;
    auto found = cache.find(int(type));
    if (found == cache.end()) {
        SceneObject object;
        object.type = type;
        found = cache.insert(int(type), Design::properties(object));
    }
    return *found;
}

// Always given, whatever their value: what an agent aims with.
const QStringList &always() {
    static const QStringList keys{QStringLiteral("id"), QStringLiteral("type"),
                                  QStringLiteral("x"),  QStringLiteral("y"),
                                  QStringLiteral("w"),  QStringLiteral("h"),
                                  QStringLiteral("text"), QStringLiteral("placeholderId")};
    return keys;
}

// A table's or chart's words as rows of text, which is how they are written
// back (table.setCells). Covered cells of a merge read as empty.
QVariantList grid(const TableData &table) {
    QVariantList rows;
    for (int r = 0; r < table.rows.size(); ++r) {
        QVariantList row;
        for (int c = 0; c < table.columns.size(); ++c) {
            const int k = Table::anchor(table, r, c);
            const bool own = k >= 0 && k == r * int(table.columns.size()) + c;
            row.append(own ? table.cells.at(k).text : QString());
        }
        rows.append(QVariant(row));
    }
    return rows;
}

QVariantMap describeObject(const Document &d, const SceneObject &authored,
                           const SceneObject &shown, bool full) {
    auto values = Design::properties(shown);
    if (!full) {
        for (const auto &key : heavy()) values.remove(key);
        const auto &plain = blank(shown.type);
        for (auto it = values.begin(); it != values.end();) {
            if (!always().contains(it.key()) && plain.contains(it.key()) &&
                plain.value(it.key()) == it.value())
                it = values.erase(it);
            else
                ++it;
        }
        if (shown.type != ObjectType::Text && shown.text.isEmpty()) values.remove(QStringLiteral("text"));
        if (authored.placeholderId.isEmpty()) values.remove(QStringLiteral("placeholderId"));
        values.remove(QStringLiteral("groups"));
        if (!shown.groups.isEmpty()) values[QStringLiteral("groups")] = shown.groups;
    }
    if (shown.type == ObjectType::Table || shown.type == ObjectType::Chart)
        values[QStringLiteral("cells")] = grid(shown.table);
    if (shown.type == ObjectType::Chart) {
        values[QStringLiteral("chartKind")] = Chart::names().value(shown.chart.kind);
        values[QStringLiteral("chartTitle")] = shown.chart.title;
    }
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
    for (const auto &step : authored.timeline.steps) {
        QVariantMap build{{QStringLiteral("targetId"), step.targetId},
                                  {QStringLiteral("effect"), effectName(step.effect)},
                                  {QStringLiteral("phase"),
                                   step.phase == BuildPhase::In ? QStringLiteral("in")
                                                                : QStringLiteral("out")},
                                  {QStringLiteral("trigger"), triggerName(step.trigger)},
                                  {QStringLiteral("start"), step.start},
                                  {QStringLiteral("duration"), step.duration},
                                  {QStringLiteral("delay"), step.delay},
                                  {QStringLiteral("pathId"), step.pathId}};
        // What a Reveal counts as one step, always: it is the whole point.
        if (step.effect == Effect::Reveal)
            build[QStringLiteral("unit")] = QStringList{QStringLiteral("paragraphs"), QStringLiteral("words"),
                                                        QStringLiteral("characters")}.value(qBound(0, step.unit, 2));
        builds.append(build);
    }
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
        // A placeholder's id is its role ("title", "body", …); a slide's object
        // names the role it fills as its placeholderId.
        for (const auto &object : layout.placeholders)
            placeholders.append(QVariantMap{{QStringLiteral("id"), object.id},
                                            {QStringLiteral("type"), typeName(object.type)},
                                            {QStringLiteral("prompt"), object.text}});
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

QVariantMap Cli::vocabulary() {
    const auto indexed = [](const QStringList &names, int from = 0) {
        QVariantMap map;
        for (int i = 0; i < names.size(); ++i) map[QString::number(from + i)] = names.at(i);
        return map;
    };
    QVariantMap objectProperties;
    for (auto type : {ObjectType::Text, ObjectType::Rect, ObjectType::Image, ObjectType::Media,
                      ObjectType::Table, ObjectType::Chart}) {
        auto keys = blank(type).keys();
        for (const auto &skip : {QStringLiteral("id"), QStringLiteral("type"),
                                 QStringLiteral("overrides"), QStringLiteral("groups"),
                                 QStringLiteral("placeholderId"), QStringLiteral("textStyleId")})
            keys.removeAll(skip);
        // Film and sound are changed through their own operations.
        keys.erase(std::remove_if(keys.begin(), keys.end(),
                                  [](const QString &k) { return k.startsWith(QStringLiteral("media")); }),
                   keys.end());
        objectProperties[typeName(type)] = keys;
    }
    auto chartKeys = Chart::encode(ChartData{}).keys();
    chartKeys.removeAll(QStringLiteral("seriesColors"));
    QVariantMap effects;
    for (int e = int(Effect::Fade); e <= int(Effect::Path); ++e)
        if (e != int(Effect::Media)) effects[QString::number(e)] = effectName(Effect(e));
    return QVariantMap{
        {QStringLiteral("themes"), indexed({QStringLiteral("Midnight"), QStringLiteral("Paper"),
                                            QStringLiteral("Grove")})},
        {QStringLiteral("newDeckLayouts"),
         indexed({QStringLiteral("Title"), QStringLiteral("Title and body"), QStringLiteral("Blank")})},
        {QStringLiteral("objectProperties"), objectProperties},
        // The numbered properties setSelectedProperty takes, by name.
        {QStringLiteral("objectChoices"),
         QVariantMap{{QStringLiteral("for"), QStringLiteral("setSelectedProperty(key, value): the value by name or number")},
                     {QStringLiteral("textAlign"), indexed({QStringLiteral("left"), QStringLiteral("centre"),
                                                            QStringLiteral("right"), QStringLiteral("justify")})},
                     {QStringLiteral("verticalAlign"), indexed({QStringLiteral("top"), QStringLiteral("middle"),
                                                                QStringLiteral("bottom")})},
                     {QStringLiteral("listStyle"),
                      indexed({QStringLiteral("none"), QStringLiteral("bullets"), QStringLiteral("numbers"),
                               QStringLiteral("circles"), QStringLiteral("squares"), QStringLiteral("lower-letters"),
                               QStringLiteral("upper-letters"), QStringLiteral("lower-roman"),
                               QStringLiteral("upper-roman")})},
                     {QStringLiteral("textFit"), indexed({QStringLiteral("clip"), QStringLiteral("shrink")})},
                     {QStringLiteral("direction"), indexed({QStringLiteral("auto"), QStringLiteral("left-to-right"),
                                                            QStringLiteral("right-to-left")})},
                     {QStringLiteral("imageMode"), indexed({QStringLiteral("fit"), QStringLiteral("fill"),
                                                            QStringLiteral("stretch")})}}},
        {QStringLiteral("shapes"),
         QVariantMap{{QStringLiteral("for"), QStringLiteral("addShape(kind)")},
                     {QStringLiteral("kinds"), indexed(Shape::names())}}},
        {QStringLiteral("charts"),
         QVariantMap{{QStringLiteral("for"), QStringLiteral("addChart(kind), setChartProperty(\"kind\", n)")},
                     {QStringLiteral("kinds"), indexed(Chart::names())},
                     {QStringLiteral("properties"), chartKeys},
                     {QStringLiteral("data"), QStringLiteral("table.setCells with the chart selected: row 0 "
                                                             "the series names, column 0 the categories")}}},
        {QStringLiteral("tables"),
         QVariantMap{{QStringLiteral("cells"), QStringLiteral("table.setCells(rows, row, column)")},
                     {QStringLiteral("cellStyle"),
                      QVariantMap{{QStringLiteral("for"),
                                   QStringLiteral("table.selectCell(row, column[, extend]) then "
                                                  "table.formatCells(key, value)")},
                                  {QStringLiteral("keys"), Table::styleKeyNames()}}},
                     {QStringLiteral("options"),
                      QVariantMap{{QStringLiteral("for"), QStringLiteral("table.setTableOption(key, bool)")},
                                  {QStringLiteral("keys"), QStringList{QStringLiteral("headerRows"),
                                                                       QStringLiteral("headerColumns"),
                                                                       QStringLiteral("banded")}}}}}},
        {QStringLiteral("builds"),
         QVariantMap{{QStringLiteral("for"),
                      QStringLiteral("addBuild(targetId, phase, effect) returns the build's index; "
                                     "setBuildProperty(index, key, value)")},
                     {QStringLiteral("phases"), indexed({QStringLiteral("in"), QStringLiteral("out")})},
                     {QStringLiteral("effects"), effects},
                     {QStringLiteral("triggers"),
                      indexed({QStringLiteral("absolute"), QStringLiteral("onClick"),
                               QStringLiteral("withPrevious"), QStringLiteral("afterPrevious")})},
                     {QStringLiteral("easing"),
                      QVariantMap{{QString::number(int(QEasingCurve::Linear)), QStringLiteral("linear")},
                                  {QString::number(int(QEasingCurve::OutCubic)), QStringLiteral("ease out")},
                                  {QString::number(int(QEasingCurve::InOutCubic)), QStringLiteral("ease in and out")}}},
                     {QStringLiteral("revealUnits"),
                      indexed({QStringLiteral("paragraphs"), QStringLiteral("words"), QStringLiteral("characters")})},
                     {QStringLiteral("properties"),
                      QStringList{QStringLiteral("start"), QStringLiteral("duration"), QStringLiteral("delay"),
                                  QStringLiteral("trigger"), QStringLiteral("phase"), QStringLiteral("effect"),
                                  QStringLiteral("amountX"), QStringLiteral("amountY"), QStringLiteral("amount"),
                                  QStringLiteral("unit"), QStringLiteral("easing"), QStringLiteral("pathId"),
                                  QStringLiteral("pathReverse"), QStringLiteral("orient")}}}},
        {QStringLiteral("transitions"),
         QVariantMap{{QStringLiteral("for"),
                      QStringLiteral("setSlideTransition(key, value[, everySlide]); -1 means follow the deck")},
                     {QStringLiteral("kinds"),
                      indexed({QStringLiteral("cut"), QStringLiteral("fade"), QStringLiteral("push"),
                               QStringLiteral("morph"), QStringLiteral("cover"), QStringLiteral("uncover"),
                               QStringLiteral("fade-through-black"), QStringLiteral("zoom"),
                               QStringLiteral("whirl")})},
                     {QStringLiteral("directions"),
                      indexed({QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("up"),
                               QStringLiteral("down")})},
                     {QStringLiteral("keys"),
                      QStringList{QStringLiteral("kind"), QStringLiteral("direction"),
                                  QStringLiteral("seconds"), QStringLiteral("advanceAfter")}}}}};
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
