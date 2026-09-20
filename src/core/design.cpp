#include "core/design.h"
#include "core/imageasset.h"
#include "core/edit.h"
#include "core/table.h"
#include "core/chart.h"
#include "core/shape.h"
#include "core/textruns.h"
#include "core/connector.h"
#include <QSet>
#include <cmath>

const SlideLayout *Design::layout(const Document &d, const QString &id) {
    for (const auto &item : d.layouts) if (item.id == id) return &item;
    return nullptr;
}
const Master *Design::master(const Document &d, const QString &id) {
    for (const auto &item : d.masters) if (item.id == id) return &item;
    return nullptr;
}
const TextStyle *Design::textStyle(const Document &d, const QString &id) {
    if (id.isEmpty()) return nullptr;
    for (const auto &item : d.textStyles) if (item.id == id) return &item;
    return nullptr;
}
QStringList Design::textStyleKeys() {
    return {"fontFamily","fontSize","fontWeight","italic","underline","textColor","uppercase",
            "letterSpacing","textAlign","verticalAlign","lineHeight","paragraphSpacing",
            "textIndent","listStyle","listStart","textFit","tabStop","columns","columnGap",
            "direction"};
}
SceneObject Design::themed(const DeckTheme &theme, SceneObject o) {
    o.fill = theme.colors.value(o.fillToken, o.fill);
    o.textColor = theme.colors.value(o.textColorToken, o.textColor);
    o.fontFamily = theme.fonts.value(o.fontToken, o.fontFamily);
    if(o.type==ObjectType::Chart) o.chart.resolvedColors=Chart::palette(theme,o.chart.palette);
    o.table.headerFill = theme.colors.value(o.table.headerFillToken,o.table.headerFill);
    o.table.borderColor = theme.colors.value(o.table.borderColorToken,o.table.borderColor);
    return o;
}
QVariantMap Design::properties(const SceneObject &o) {
    return {{"id", o.id}, {"type", o.type == ObjectType::Text ? "text" : o.type == ObjectType::Image ? "image" : o.type == ObjectType::Media ? "media" : o.type == ObjectType::Table ? "table" : o.type == ObjectType::Chart ? "chart" : "rect"},
            {"dataSource",LinkedData::encode(o.dataSource)}, {"chart",Chart::encode(o.chart)}, {"table",Table::encode(o.table)}, {"x", o.rect.x()}, {"y", o.rect.y()}, {"w", o.rect.width()}, {"h", o.rect.height()},
            {"linkKind",o.linkKind},{"linkTarget",o.linkTarget},{"connector",o.connector},{"connectorFrom",o.connectorFrom},{"connectorTo",o.connectorTo},
            {"connectorStartX",o.connectorStart.x()},{"connectorStartY",o.connectorStart.y()},{"connectorEndX",o.connectorEnd.x()},{"connectorEndY",o.connectorEnd.y()},
            {"connectorFromSide",o.connectorFromSide},{"connectorToSide",o.connectorToSide},{"connectorRoute",o.connectorRoute},{"connectorArrowStart",o.connectorArrowStart},{"connectorArrowEnd",o.connectorArrowEnd},
            {"shapeKind",o.shapeKind},{"pathData",o.pathData},{"pathWinding",o.pathWinding},{"fillStyle",o.fillStyle},{"fillSecondary",o.fillSecondary.name(QColor::HexArgb)},
            {"fillAngle",o.fillAngle},{"patternStyle",o.patternStyle},{"strokeColor",o.strokeColor.name(QColor::HexArgb)},
            {"strokeWidth",o.strokeWidth},{"strokeStyle",o.strokeStyle},{"strokeJoin",o.strokeJoin},{"strokeCap",o.strokeCap},
            {"shadowEnabled",o.shadowEnabled},{"shadowColor",o.shadowColor.name(QColor::HexArgb)},{"shadowX",o.shadowX},{"shadowY",o.shadowY},
            {"imageOriginal",o.imageOriginal ? properties(*o.imageOriginal) : QVariantMap()}, {"imageFormat",o.imageFormat},{"imageId",o.imageId},{"imageMode",o.imageMode},{"imageMask",o.imageMask},{"imageFocalX",o.imageFocalX},{"imageFocalY",o.imageFocalY},
            {"imageBrightness",o.imageBrightness},{"imageContrast",o.imageContrast},{"imageSaturation",o.imageSaturation},{"imageTint",o.imageTint.name(QColor::HexArgb)},{"imageTintAmount",o.imageTintAmount},{"cropX",o.imageCrop.x()},{"cropY",o.imageCrop.y()},
            {"cropW",o.imageCrop.width()},{"cropH",o.imageCrop.height()},
            {"mediaOriginal",o.mediaOriginal ? properties(*o.mediaOriginal) : QVariantMap()}, {"mediaId",o.mediaId},{"mediaPath",o.mediaPath},{"mediaName",o.mediaName},{"mediaContainer",o.mediaContainer},{"mediaCodec",o.mediaCodec},
            {"mediaBytes",o.mediaBytes},{"mediaModified",o.mediaModified},{"mediaDuration",o.mediaDuration},{"mediaTrimStart",o.mediaTrimStart},{"mediaTrimEnd",o.mediaTrimEnd},{"mediaVolume",o.mediaVolume},{"mediaLoops",o.mediaLoops},{"mediaVideo",o.mediaVideo},{"mediaAudio",o.mediaAudio},
            {"rotation", o.rotation}, {"opacity", o.opacity}, {"cornerRadius", o.cornerRadius},
            {"fill", o.fill.name(QColor::HexArgb)}, {"textColor", o.textColor.name(QColor::HexArgb)},
            {"text", o.text}, {"fontSize", o.fontSize}, {"fontWeight", o.fontWeight},
            {"italic",o.italic},{"underline",o.underline},{"textAlign",o.textAlign},{"verticalAlign",o.verticalAlign},
            {"lineHeight",o.lineHeight},{"paragraphSpacing",o.paragraphSpacing},{"textIndent",o.textIndent},
            {"listStyle",o.listStyle},{"listStart",o.listStart},{"textFit",o.textFit},{"textKind",o.textKind},
            {"tabStop",o.tabStop},{"columns",o.columns},{"columnGap",o.columnGap},{"direction",o.direction},
            {"language",o.language},
            {"fontFamily", o.fontFamily}, {"uppercase", o.uppercase}, {"letterSpacing", o.letterSpacing},
            {"fillToken", o.fillToken}, {"textColorToken", o.textColorToken}, {"fontToken", o.fontToken},
            {"altTitle", o.altTitle}, {"altText", o.altText}, {"runs", TextRuns::encode(o.runs)},
            {"placeholderId", o.placeholderId}, {"textStyleId", o.textStyleId}, {"overrides", o.overrides}, {"groups", o.groups}, {"locked", o.locked}, {"hidden", o.hidden}};
}
bool Design::setProperty(SceneObject &o, const QString &key, const QVariant &v, bool constrainForEditing) {
    const QStringList numeric = {"textKind","tabStop","columns","columnGap","direction","mediaBytes","mediaModified","mediaDuration","mediaTrimStart","mediaTrimEnd","mediaVolume","mediaLoops","x", "y", "w", "h", "rotation", "opacity", "cornerRadius",
                                 "linkKind", "connectorStartX", "connectorStartY", "connectorEndX", "connectorEndY", "connectorFromSide", "connectorToSide", "connectorRoute", "shapeKind", "fillStyle", "fillAngle", "patternStyle", "strokeWidth", "strokeStyle", "strokeJoin", "strokeCap", "shadowX", "shadowY", "fontSize", "fontWeight", "letterSpacing", "textAlign", "verticalAlign", "lineHeight", "paragraphSpacing", "textIndent", "listStyle", "listStart", "textFit", "imageMask", "imageFocalX", "imageFocalY", "imageBrightness", "imageContrast", "imageSaturation", "imageTintAmount", "imageMode", "cropX", "cropY", "cropW", "cropH"};
    bool ok = false;
    const qreal n = v.toDouble(&ok);
    if (numeric.contains(key) && (!ok || !std::isfinite(n))) return false;
    if ((key == "fill" || key == "textColor" || key == "fillSecondary" || key == "strokeColor" || key == "shadowColor" || key == "imageTint") && !QColor(v.toString()).isValid()) return false;
    if (key == "dataSource") return LinkedData::decode(v,o.dataSource);
    if (key == "chart") return Chart::decode(v,o.chart);
    if (key == "table") { TableData data; if(!Table::decode(v,data) || (o.type!=ObjectType::Table && o.type!=ObjectType::Chart && !data.cells.isEmpty())) return false; o.table=data; return true; }
    if (key == "x") o.rect.moveLeft(n);
    else if (key == "y") o.rect.moveTop(n);
    else if (key == "w") o.rect.setWidth(qMax(constrainForEditing ? 8.0 : 0.0, n));
    else if (key == "h") o.rect.setHeight(qMax(constrainForEditing ? 8.0 : 0.0, n));
    else if (key == "rotation") o.rotation = n;
    else if (key == "opacity") o.opacity = qBound(0.0, n, 1.0);
    else if (key == "cornerRadius") o.cornerRadius = qMax(0.0, n);
    else if (key == "linkKind") { if(n<0 || n>8) return false; o.linkKind=v.toInt(); }
    else if (key == "linkTarget") o.linkTarget=v.toString();
    else if (key == "connector") o.connector=v.toBool();
    else if (key == "connectorFrom") o.connectorFrom=v.toString();
    else if (key == "connectorTo") o.connectorTo=v.toString();
    else if (key == "connectorStartX") o.connectorStart.setX(n);
    else if (key == "connectorStartY") o.connectorStart.setY(n);
    else if (key == "connectorEndX") o.connectorEnd.setX(n);
    else if (key == "connectorEndY") o.connectorEnd.setY(n);
    else if (key == "connectorFromSide") o.connectorFromSide=qBound(0,v.toInt(),4);
    else if (key == "connectorToSide") o.connectorToSide=qBound(0,v.toInt(),4);
    else if (key == "connectorRoute") o.connectorRoute=qBound(0,v.toInt(),2);
    else if (key == "connectorArrowStart") o.connectorArrowStart=v.toBool();
    else if (key == "connectorArrowEnd") o.connectorArrowEnd=v.toBool();
    else if (key == "shapeKind") { const int k=v.toInt(); if(k!=Shape::custom && (k<0 || k>=Shape::names().size())) return false; o.shapeKind=k; }
    else if (key == "pathWinding") o.pathWinding=v.toBool();
    else if (key == "pathData") { if(!Shape::decode(v.toList())) return false; o.pathData=v.toList(); }
    else if (key == "fillStyle") o.fillStyle=qBound(0,v.toInt(),5);
    else if (key == "fillSecondary") o.fillSecondary=QColor(v.toString());
    else if (key == "fillAngle") o.fillAngle=n;
    else if (key == "patternStyle") o.patternStyle=qBound(0,v.toInt(),12);
    else if (key == "strokeWidth") o.strokeWidth=qMax(0.0,n);
    else if (key == "strokeColor") o.strokeColor=QColor(v.toString());
    else if (key == "strokeStyle") o.strokeStyle=qBound(0,v.toInt(),4);
    else if (key == "strokeJoin") o.strokeJoin=qBound(0,v.toInt(),2);
    else if (key == "strokeCap") o.strokeCap=qBound(0,v.toInt(),2);
    else if (key == "shadowEnabled") o.shadowEnabled=v.toBool();
    else if (key == "shadowColor") o.shadowColor=QColor(v.toString());
    else if (key == "shadowX") o.shadowX=n;
    else if (key == "shadowY") o.shadowY=n;
    else if (key == "fontSize") o.fontSize = qMax(constrainForEditing ? 4.0 : 0.0, n);
    else if (key == "fontWeight") o.fontWeight = qBound(100, v.toInt(), 900);
    else if (key == "letterSpacing") o.letterSpacing = n;
    else if (key == "imageMask") o.imageMask=qBound(0,v.toInt(),4);
    else if (key == "imageFocalX") o.imageFocalX=qBound(0.0,n,1.0);
    else if (key == "imageFocalY") o.imageFocalY=qBound(0.0,n,1.0);
    else if (key == "imageBrightness") o.imageBrightness=qBound(-1.0,n,1.0);
    else if (key == "imageContrast") o.imageContrast=qBound(0.0,n,3.0);
    else if (key == "imageSaturation") o.imageSaturation=qBound(0.0,n,3.0);
    else if (key == "imageTintAmount") o.imageTintAmount=qBound(0.0,n,1.0);
    else if (key == "imageTint") o.imageTint=QColor(v.toString());
    else if (key == "imageMode") o.imageMode = qBound(0,v.toInt(),2);
    else if (key == "imageFormat") { if(v.toString()!="png" && v.toString()!="jpg" && v.toString()!="webp" && v.toString()!="svg") return false; o.imageFormat=v.toString(); }
    else if (key == "imageId") o.imageId = v.toString();
    else if (key == "cropX") { o.imageCrop.moveLeft(qBound(0.0,n,.99)); o.imageCrop.setWidth(qMin(o.imageCrop.width(),1-o.imageCrop.x())); }
    else if (key == "cropY") { o.imageCrop.moveTop(qBound(0.0,n,.99)); o.imageCrop.setHeight(qMin(o.imageCrop.height(),1-o.imageCrop.y())); }
    else if (key == "cropW") o.imageCrop.setWidth(qBound(.01,n,1-o.imageCrop.x()));
    else if (key == "cropH") o.imageCrop.setHeight(qBound(.01,n,1-o.imageCrop.y()));
    else if (key == "imageOriginal") {
        if(v.metaType().id()!=QMetaType::QVariantMap) return false;
        const auto map=v.toMap();
        if(map.isEmpty()) { o.imageOriginal.reset(); return true; }
        if((o.type!=ObjectType::Image && o.type!=ObjectType::Rect) || map.value("type").toString()!="image" || !map.value("mediaOriginal").toMap().isEmpty() || !map.value("imageOriginal").toMap().isEmpty()) return false;
        SceneObject source; source.type=ObjectType::Image;
        const QSet<QString> structural={"id","type","groups","placeholderId","textStyleId","overrides"};
        for(auto it=map.cbegin();it!=map.cend();++it) if(!structural.contains(it.key())) if(!setProperty(source,it.key(),it.value(),false)) return false;
        source.id=map.value("id").toString(); source.groups=map.value("groups").toStringList();
        source.placeholderId=map.value("placeholderId").toString(); source.textStyleId=map.value("textStyleId").toString();
        source.overrides=map.value("overrides").toStringList();
        o.imageOriginal=std::make_shared<const SceneObject>(source);
    }
    else if (key == "mediaOriginal") {
        if(v.metaType().id()!=QMetaType::QVariantMap) return false;
        const auto map=v.toMap();
        if(map.isEmpty()) { o.mediaOriginal.reset(); return true; }
        if(o.type!=ObjectType::Media || map.value("type").toString()!="media" || (!map.value("mediaOriginal").toMap().isEmpty() || !map.value("imageOriginal").toMap().isEmpty())) return false;
        SceneObject source; source.type=ObjectType::Media;
        const QSet<QString> structural={"id","type","groups","placeholderId","textStyleId","overrides"};
        for(auto it=map.cbegin();it!=map.cend();++it) if(!structural.contains(it.key())) if(!setProperty(source,it.key(),it.value(),false)) return false;
        source.id=map.value("id").toString(); source.groups=map.value("groups").toStringList();
        source.placeholderId=map.value("placeholderId").toString(); source.textStyleId=map.value("textStyleId").toString();
        source.overrides=map.value("overrides").toStringList();
        o.mediaOriginal=std::make_shared<const SceneObject>(source);
    }
    else if (key == "mediaId") o.mediaId=v.toString();
    else if (key == "mediaPath") o.mediaPath=v.toString();
    else if (key == "mediaName") o.mediaName=v.toString().left(1024);
    else if (key == "mediaContainer") o.mediaContainer=v.toString().left(1024);
    else if (key == "mediaCodec") o.mediaCodec=v.toString().left(1024);
    else if (key == "mediaBytes") { if(n<0 || n>2LL*1024*1024*1024) return false; o.mediaBytes=v.toLongLong(); }
    else if (key == "mediaModified") { if(n<0) return false; o.mediaModified=v.toLongLong(); }
    else if (key == "mediaDuration") { if(n<0 || n>86400) return false; o.mediaDuration=n; }
    else if (key == "mediaTrimStart") { if(n<0 || n>86400) return false; o.mediaTrimStart=n; }
    else if (key == "mediaTrimEnd") { if(n<0 || n>86400) return false; o.mediaTrimEnd=n; }
    else if (key == "mediaVolume") { if(n<0 || n>1) return false; o.mediaVolume=n; }
    else if (key == "mediaLoops") { if(n<1 || n>100 || n!=int(n)) return false; o.mediaLoops=int(n); }
    else if (key == "mediaVideo") o.mediaVideo=v.toBool();
    else if (key == "mediaAudio") o.mediaAudio=v.toBool();
    else if (key == "italic") o.italic = v.toBool();
    else if (key == "underline") o.underline = v.toBool();
    else if (key == "textAlign") o.textAlign = qBound(0,v.toInt(),3);
    else if (key == "verticalAlign") o.verticalAlign = qBound(0,v.toInt(),2);
    else if (key == "lineHeight") o.lineHeight = qBound(50.0,n,300.0);
    else if (key == "paragraphSpacing") o.paragraphSpacing = constrainForEditing ? qBound(0.0,n,1000.0) : qMax(0.0,n);
    else if (key == "textIndent") o.textIndent = constrainForEditing ? qBound(0.0,n,1000.0) : qMax(0.0,n);
    else if (key == "listStyle") o.listStyle = qBound(0,v.toInt(),8);
    else if (key == "listStart") o.listStart = qBound(1,v.toInt(),9999);
    else if (key == "textFit") o.textFit = qBound(0,v.toInt(),1);
    else if (key == "textKind") o.textKind = qBound(0,v.toInt(),2);
    else if (key == "language") { if(v.toString().size()>32) return false; o.language = v.toString(); }
    else if (key == "tabStop") o.tabStop = qBound(0.0,n,4000.0);
    else if (key == "columns") o.columns = qBound(1,v.toInt(),6);
    else if (key == "columnGap") o.columnGap = qBound(0.0,n,4000.0);
    else if (key == "direction") o.direction = qBound(0,v.toInt(),2);
    else if (key == "locked") o.locked = v.toBool();
    else if (key == "hidden") o.hidden = v.toBool();
    else if (key == "uppercase") o.uppercase = v.toBool();
    else if (key == "text") o.text = v.toString();
    else if (key == "fontFamily") o.fontFamily = v.toString();
    else if (key == "fill") o.fill = QColor(v.toString());
    else if (key == "textColor") o.textColor = QColor(v.toString());
    else if (key == "fillToken") o.fillToken = v.toString();
    else if (key == "textColorToken") o.textColorToken = v.toString();
    else if (key == "fontToken") o.fontToken = v.toString();
    else if (key == "runs") {
        // Not clipped here: the text may not have been set yet, and clipping
        // an unset string would silently drop every stretch.
        QVector<TextRun> runs;
        if (!TextRuns::decode(v, runs)) return false;
        o.runs = runs;
    }
    else if (key == "altTitle" || key == "altText") {
        const auto text = v.toString();
        if (text.size() > (key == "altTitle" ? 200 : 2000)) return false;
        (key == "altTitle" ? o.altTitle : o.altText) = text;
    }
    else return false;
    return true;
}

Slide Design::resolve(const Document &d, int index) {
    if (index < 0 || index >= d.slides.size()) return {};
    Slide slide = d.slides.at(index);
    const auto *definition = layout(d, slide.layoutId);
    const auto *base = definition ? master(d, definition->masterId) : nullptr;
    slide.objects.clear();
    if (base) {
        if (!slide.backgroundOverride)
            slide.background = d.theme.colors.value(base->backgroundToken, base->background);
        if (slide.showMasterObjects)
            for (auto o : base->objects) {
                o.id = QStringLiteral("@master/") + base->id + '/' + o.id;
                slide.objects.append(themed(d.theme, o));
            }
        if (slide.showMasterFields) slide.objects += fields(d, index, *base);
    }
    for (const auto &local : d.slides.at(index).objects) {
        SceneObject o = local;
        if (definition && !local.placeholderId.isEmpty()) {
            for (const auto &p : definition->placeholders) {
                if (p.id != local.placeholderId) continue;
                o = p;
                o.id = local.id;
                o.placeholderId = local.placeholderId;
                o.textStyleId = local.textStyleId;
                o.overrides = local.overrides;
                o.groups = local.groups; o.locked = local.locked; o.hidden = local.hidden;
                const auto values = properties(local);
                for (const auto &key : local.overrides) {
                    setProperty(o, key, values.value(key), false);
                    if (key == "fill") o.fillToken = local.fillToken;
                    if (key == "textColor") o.textColorToken = local.textColorToken;
                    if (key == "fontFamily") o.fontToken = local.fontToken;
                    if (key == "imageId") ImageAsset::copyData(o,local);
                }
                break;
            }
        }
        if (const auto *style = textStyle(d, o.textStyleId)) {
            const auto values = properties(style->look);
            for (const auto &key : textStyleKeys())
                if (!o.overrides.contains(key)) setProperty(o, key, values.value(key), false);
            if (!o.overrides.contains(QStringLiteral("textColor")))
                o.textColorToken = style->look.textColorToken;
            if (!o.overrides.contains(QStringLiteral("fontFamily")))
                o.fontToken = style->look.fontToken;
        }
        slide.objects.append(themed(d.theme, o));
    }
    Connector::resolve(slide.objects);
    return slide;
}
DeckTheme Design::preset(int index) {
    DeckTheme theme;
    if (index == 1) {
        theme.name = QStringLiteral("Paper");
        theme.colors = {{"background", QColor(247,244,237)}, {"foreground", QColor(35,38,42)},
                        {"muted", QColor(93,99,107)}, {"accent", QColor(168,58,39)}};
        theme.fonts["heading"] = QStringLiteral("Noto Serif");
    } else if (index == 2) {
        theme.name = QStringLiteral("Grove");
        theme.colors = {{"background", QColor(17,38,33)}, {"foreground", QColor(240,243,223)},
                        {"muted", QColor(177,198,180)}, {"accent", QColor(216,231,144)}};
    }
    return theme;
}
void Design::ensureDefaults(Document &d) {
    if (!d.layouts.isEmpty()) return;
    if (d.masters.isEmpty()) {
        Master m;
        m.id = Edit::newId("master"); m.name = QStringLiteral("Standard");
        d.masters.append(m);
    }
    const qreal sx = d.size.width()/1920, sy = d.size.height()/1080;
    auto text = [sx,sy](QString role, QString prompt, QRectF r, qreal size) {
        SceneObject o;
        o.id = role; o.type = ObjectType::Text; o.text = prompt;
        o.rect = QRectF(r.x()*sx,r.y()*sy,r.width()*sx,r.height()*sy);
        o.fontSize = size*sy;
        o.fontWeight = role == "title" ? 600 : 400;
        o.textColorToken = role == "title" ? "foreground" : "muted";
        o.fontToken = role == "title" ? "heading" : "body";
        return o;
    };
    SlideLayout title;
    title.id = Edit::newId("layout"); title.name = QStringLiteral("Title");
    title.masterId = d.masters.first().id;
    title.placeholders = {text("title","Your title",QRectF(160,300,1600,210),100),
                          text("body","A few words to begin",QRectF(168,560,1500,150),44)};
    SlideLayout body = title;
    body.id = Edit::newId("layout"); body.name = QStringLiteral("Title and body");
    body.placeholders = {text("title","Your title",QRectF(160,110,1600,170),80),
                         text("body","Tell your story",QRectF(168,350,1580,540),48)};
    SlideLayout blank;
    blank.id = Edit::newId("layout"); blank.name = QStringLiteral("Blank"); blank.masterId = title.masterId;
    d.layouts = {title,body,blank};
}
SceneObject Design::detached(SceneObject o) {
    o.placeholderId.clear(); o.textStyleId.clear(); o.overrides.clear();
    o.fillToken.clear(); o.textColorToken.clear(); o.fontToken.clear();
    if(o.type==ObjectType::Table || o.type==ObjectType::Chart) {o.table.headerFillToken.clear();o.table.borderColorToken.clear();}
    if(o.type==ObjectType::Chart && !o.chart.resolvedColors.isEmpty()) {
        const int columns=o.table.columns.size();
        for(int i=1;i<columns;++i) {const auto id=o.table.cells[i].id;if(!o.chart.seriesColors.contains(id))o.chart.seriesColors[id]=o.chart.resolvedColors[(i-1)%o.chart.resolvedColors.size()];}
        for(int r=1;r<o.table.rows.size();++r) {const auto id=o.table.cells[r*columns].id;if(!o.chart.seriesColors.contains(id))o.chart.seriesColors[id]=o.chart.resolvedColors[(r-1)%o.chart.resolvedColors.size()];}
    }
    return o;
}
bool Design::applyLayout(Document &d, int index, const QString &id) {
    return applyLayout(d, index, id, {}, 0);
}
bool Design::applyLayout(Document &d, int index, const QString &id,
                         const QVariantMap &mapping, int geometry) {
    const auto *target = layout(d,id);
    if (!target || index < 0 || index >= d.slides.size() || geometry < 0 || geometry > 2) return false;
    const auto definition = *target;
    const Slide previous = resolve(d,index);
    Slide slide = d.slides[index];
    QSet<QString> used;
    for (auto &o : slide.objects) {
        if (o.placeholderId.isEmpty()) continue;
        const QString role = mapping.value(o.placeholderId, o.placeholderId).toString();
        const SceneObject *match = nullptr;
        for (const auto &p : definition.placeholders) {
            if (p.id == role && p.type == o.type) { match = &p; break; }
        }
        // Explicit invalid/duplicate mappings fail atomically, including earlier objects.
        if (!role.isEmpty() && mapping.contains(o.placeholderId) && !match) return false;
        if (match && used.contains(role)) {
            if (mapping.contains(o.placeholderId)) return false;
            match = nullptr;
        }
        const auto *old = previous.find(o.id);
        if (!match) {
            if (old) o = detached(*old);
            continue;
        }
        used.insert(role);
        if (old && role != o.placeholderId && o.type == ObjectType::Text) {
            o.text = old->text;
            markOverride(o, "text");
        }
        o.placeholderId = role;
        if (geometry == 1) reset(o, true);
        if (geometry == 2 && old) {
            o.rect = old->rect; o.rotation = old->rotation;
            for (const QString &key : {"x", "y", "w", "h", "rotation"}) markOverride(o, key);
        }
    }
    for (const auto &p : definition.placeholders) {
        if (used.contains(p.id)) continue;
        SceneObject o = p;
        o.id = Edit::newId("placeholder"); o.placeholderId = p.id; o.overrides.clear();
        slide.objects.append(o);
    }
    slide.layoutId = id;
    d.slides[index] = slide;
    return true;
}
void Design::markOverride(SceneObject &o, const QString &key) {
    if (!o.placeholderId.isEmpty() && !o.overrides.contains(key)) o.overrides.append(key);
    if (key == "fill") o.fillToken.clear();
    if (key == "textColor") o.textColorToken.clear();
    if (key == "fontFamily") o.fontToken.clear();
}
void Design::reset(SceneObject &o, bool geometry) {
    const QStringList position = {"x","y","w","h","rotation"};
    for (int i = o.overrides.size()-1; i >= 0; --i) {
        const auto key = o.overrides.at(i);
        if (key != "text" && position.contains(key) == geometry) o.overrides.removeAt(i);
    }
}
