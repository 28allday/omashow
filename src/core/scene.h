#pragma once

// Authored content and shared design definitions. The design resolver applies
// inheritance before the deterministic animation evaluator produces a frame.

#include <QColor>
#include <QImage>
#include <QPicture>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVector>
#include <QVariantList>
#include <memory>

#include "anim/build.h"
#include "core/tabledata.h"
#include "core/chartdata.h"
#include "core/datasource.h"

enum class ObjectType { Rect, Text, Image, Media, Table, Chart };

// A stretch of text that does not look like the rest of its box. Only what
// differs is stored: an empty family, a zero size or weight and an invalid
// colour all mean "as the box says", so changing the box still carries.
struct TextRun {
    int start = 0, length = 0;
    int weight = 0;              // 0 inherits
    int italic = 0, underline = 0, strike = 0;   // 0 inherits, 1 on, 2 off
    int baseline = 0;            // 0 normal, 1 raised, 2 lowered
    qreal fontSize = 0;          // 0 inherits
    QString fontFamily;
    QColor color;                // invalid inherits
    bool operator==(const TextRun &other) const {
        return start == other.start && length == other.length && weight == other.weight &&
               italic == other.italic && underline == other.underline &&
               strike == other.strike && baseline == other.baseline &&
               qFuzzyCompare(fontSize + 1, other.fontSize + 1) &&
               fontFamily == other.fontFamily && color == other.color;
    }
};

// One thing on a slide. This doubles as the *resolved state* of that thing at a
// given time: the evaluator returns a copy with the animated fields changed, so
// there is exactly one shape flowing from document to renderer.
struct SceneObject {
    QString id;                       // stable across slides — this is what Morph matches on
    ObjectType type = ObjectType::Rect;
    QRectF rect;                      // in document coordinates
    qreal rotation = 0.0;             // degrees
    qreal opacity = 1.0;
    QColor fill = QColor(255, 255, 255);
    qreal cornerRadius = 0.0;

    int linkKind = 0;
    QString linkTarget;
    bool connector = false;
    QString connectorFrom, connectorTo;
    QPointF connectorStart, connectorEnd;
    int connectorFromSide = 0, connectorToSide = 0, connectorRoute = 1;
    bool connectorArrowStart = false, connectorArrowEnd = true;
    int shapeKind = 0; // Shape::names(), or 99 for editable paths
    QVariantList pathData;
    bool pathWinding = false;
    int fillStyle = 0; // solid, linear, radial, pattern, picture, none
    QColor fillSecondary = QColor(255,255,255);
    qreal fillAngle = 0;
    int patternStyle = 9; // Qt patterns from Dense1 through DiagCross
    QColor strokeColor = QColor(0,0,0);
    qreal strokeWidth = 0;
    int strokeStyle = 0, strokeJoin = 1, strokeCap = 1;
    bool shadowEnabled = false;
    QColor shadowColor = QColor(0,0,0,100);
    qreal shadowX = 8, shadowY = 8;

    // Embedded raster data is implicitly shared between snapshots and duplicates.
    std::shared_ptr<const SceneObject> imageOriginal;
    QImage image;
    QByteArray imageData;
    QString imageId;
    QString imageFormat = QStringLiteral("png");
    QPicture vectorPicture = QPicture();
    QSizeF vectorSize;
    QRectF imageCrop = QRectF(0,0,1,1); // normalized source region
    int imageMode = 0; // fit, fill, stretch
    qreal imageFocalX = .5, imageFocalY = .5;
    int imageMask = 0; // rectangle, ellipse, rounded, hexagon, heart
    qreal imageBrightness = 0, imageContrast = 1, imageSaturation = 1, imageTintAmount = 0;
    QColor imageTint = QColor(0,0,0);

    // Media bytes are shared by history snapshots. Local read permission and
    // playback position are derived session state, never written into a deck.
    std::shared_ptr<const SceneObject> mediaOriginal;
    QByteArray mediaData;
    QString mediaId, mediaPath, mediaName, mediaContainer, mediaCodec;
    qint64 mediaBytes = 0, mediaModified = 0;
    qreal mediaDuration = 0, mediaTrimStart = 0, mediaTrimEnd = 0;
    qreal mediaVolume = 1;
    int mediaLoops = 1;
    bool mediaVideo = false, mediaAudio = false;
    qreal mediaPosition = -1;
    bool mediaActive = false, mediaReadAllowed = false;

    TableData table;
    ChartData chart;
    DataSource dataSource;

    // Text only
    QString text;
    qreal fontSize = 48.0;
    int fontWeight = 400;
    QColor textColor = QColor(255, 255, 255);
    bool uppercase = false;
    qreal letterSpacing = 0.0;
    bool italic = false, underline = false;
    int textAlign = 0;       // left, centre, right, justify
    int verticalAlign = 1;   // top, middle, bottom; middle preserves older decks
    qreal lineHeight = 100;  // percentage
    qreal paragraphSpacing = 0, textIndent = 0;
    int listStyle = 0;       // none, bullets, numbers; leading tabs nest items
    int listStart = 1;
    int textFit = 0;         // clip with overflow warning, shrink to fit
    qreal tabStop = 0;       // 0 uses four times the type size
    int columns = 1;         // text flows down one column, then into the next
    qreal columnGap = 0;     // 0 uses one line of space between columns
    int direction = 0;       // 0 follows the words, 1 left to right, 2 right to left
    QString fontFamily = QStringLiteral("Inter");
    QString fillToken, textColorToken, fontToken;
    // Stretches of this text that differ from the box, in authored order.
    QVector<TextRun> runs;
    // What a screen reader would be told this is. Never rendered.
    QString altTitle, altText;
    QString placeholderId;
    QString textStyleId;      // the named look this box follows, if any
    QStringList overrides;
    QStringList groups; // outermost to innermost
    bool locked = false;
    bool hidden = false;
};

struct Slide {
    QString id;
    QColor background = QColor(12, 16, 24);
    QVector<SceneObject> objects;
    Timeline timeline;
    QString layoutId;
    QString sectionId;
    QString notes;
    // Object ids in the order they should be read. Anything not listed keeps
    // its place in the stacking order, after those that are.
    QStringList readingOrder;
    // How the show arrives at this slide. -1 follows the deck's own choice.
    int transition = -1;
    int transitionDirection = 0;    // push: 0 left, 1 right, 2 up, 3 down
    qreal transitionSeconds = -1;
    // Seconds to hold after the last build before moving on by itself.
    // -1 waits for the speaker.
    qreal advanceAfter = -1;
    bool backgroundOverride = false;
    bool showMasterObjects = true;
    bool showMasterFields = true;
    bool skipped = false;

    const SceneObject *find(const QString &id) const;
    SceneObject *find(const QString &id);

    // Topmost object containing the point, or empty. Later objects draw over
    // earlier ones, so hit testing walks backwards.
    QString objectAt(const QPointF &point) const;
};

struct DeckTheme {
    QString name = QStringLiteral("Midnight");
    QMap<QString, QColor> colors = {
        {QStringLiteral("background"), QColor(12, 16, 24)},
        {QStringLiteral("foreground"), QColor(231, 237, 247)},
        {QStringLiteral("muted"), QColor(167, 177, 193)},
        {QStringLiteral("accent"), QColor(39, 194, 255)}
    };
    QMap<QString, QString> fonts = {
        {QStringLiteral("heading"), QStringLiteral("Inter")},
        {QStringLiteral("body"), QStringLiteral("Inter")}
    };
};

struct MasterFields {
    bool showNumber = false, showDate = false, showFooter = false, hideOnFirst = false;
    int firstNumber = 1;
    QString date, footer;
};

struct Master {
    QString id, name;
    QColor background = QColor(12, 16, 24);
    QString backgroundToken = QStringLiteral("background");
    QVector<SceneObject> objects;
    MasterFields fields;
};

struct SlideLayout {
    QString id, name, masterId;
    // Stable ids identify placeholder roles across layouts.
    QVector<SceneObject> placeholders;
};

struct Section { QString id, name; };

// A named order of slides that already exist — never a copy of them.
struct CustomShow { QString id, name; QStringList slideIds; };

// A note on a slide, or on one object, or a reply to either. Replies are one
// level deep: a thread is a comment and the replies under it.
struct Comment {
    QString id, slideId, objectId, parentId;
    QString author, created;   // ISO-8601, exactly as it was written
    QString text;
    bool resolved = false;
};

struct ObjectStyle { QString id, name; SceneObject appearance; };

// A named way for text to look, that boxes can follow. Changing the style
// changes every box that follows it, except where a box says otherwise.
struct TextStyle { QString id, name; SceneObject look; };

struct Document {
    QVector<ObjectStyle> objectStyles;
    QVector<TextStyle> textStyles;
    QSizeF size = QSizeF(1920, 1080);
    QVector<Slide> slides;
    QVector<Section> sections;
    DeckTheme theme;
    QVector<Master> masters;
    QVector<SlideLayout> layouts;
    QVector<Comment> comments;
    QVector<CustomShow> shows;
    // Which slides are being shown right now, in order. Session state: it is
    // never written to a file, and an empty list means the whole deck.
    QStringList activeShow;
    // Review findings the author has looked at and does not want raised again.
    QStringList dismissedIssues;

    // What a slide does unless it says otherwise: the kind of transition into
    // it, and how long that takes.
    int transition = 3;   // Presentation::Morph
    qreal transitionDuration = 0.9;
};
