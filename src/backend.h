#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QQuickTextDocument>

#include <QVariantMap>
#include <QHash>
#include <functional>
#include "core/mediaasset.h"

#include "core/history.h"
#include "tablemodel.h"
#include "core/scene.h"
#include "anim/presentationcache.h"
#include "core/workers.h"
#include "io/exports.h"
#include <atomic>

class PortalFileChooser;
class MediaPlayback;
class TableModel;

// The one object QML talks to. Everything that touches the filesystem, a
// subprocess or D-Bus lives behind here; QML only reads properties, calls
// invokables and reacts to signals.
//
// It also owns the deck. One document authority — QML never holds a parallel
// copy of a slide, it asks for the time and lets the evaluator answer.
class Backend : public QObject {
    Q_OBJECT
    friend class SlideThumbnailProvider;
    Q_PROPERTY(QVariantMap diagramPreview READ diagramPreview NOTIFY diagramPreviewChanged)
    Q_PROPERTY(QStringList chartNames READ chartNames CONSTANT)
    Q_PROPERTY(TableModel *tableModel READ tableModel CONSTANT)
    Q_PROPERTY(QVariantMap mediaOptimisation READ mediaOptimisation NOTIFY mediaOptimisationChanged)
    Q_PROPERTY(QString mediaJobLabel READ mediaJobLabel NOTIFY mediaJobChanged)
    Q_PROPERTY(int mediaProgress READ mediaProgress NOTIFY mediaJobChanged)
    Q_PROPERTY(QVariantList mediaPreflight READ mediaPreflight NOTIFY mediaJobChanged)
    Q_PROPERTY(QVariantList connectorTargets READ connectorTargets NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList objectStyles READ objectStyles NOTIFY documentChanged)
    Q_PROPERTY(QVariantList textStyles READ textStyles NOTIFY documentChanged)
    Q_PROPERTY(QStringList shapeNames READ shapeNames CONSTANT)
    Q_PROPERTY(QUrl fileUrl READ fileUrl NOTIFY fileUrlChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileUrlChanged)
    Q_PROPERTY(QVariantMap fileState READ fileState NOTIFY fileStateChanged)
    // What a deck opened from PowerPoint or Keynote could not bring across.
    Q_PROPERTY(QVariantMap importReport READ importReport NOTIFY importReportChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString operation READ operation NOTIFY operationChanged)

    Q_PROPERTY(bool includeSkipped READ includeSkipped WRITE setIncludeSkipped NOTIFY deckChanged)
    Q_PROPERTY(qreal time READ time WRITE setTime NOTIFY timeChanged)
    Q_PROPERTY(qreal duration READ duration NOTIFY deckChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(int slideCount READ slideCount NOTIFY deckChanged)
    Q_PROPERTY(int slideIndex READ slideIndex NOTIFY timeChanged)
    Q_PROPERTY(bool inTransition READ inTransition NOTIFY timeChanged)
    Q_PROPERTY(QString timecode READ timecode NOTIFY timeChanged)

    Q_PROPERTY(bool canPaste READ canPaste NOTIFY clipboardChanged)
    Q_PROPERTY(QVariantMap clipboardKinds READ clipboardKinds NOTIFY clipboardChanged)
    Q_PROPERTY(bool startVisible READ startVisible NOTIFY startChanged)
    Q_PROPERTY(bool hasDocument READ hasDocument NOTIFY startChanged)
    Q_PROPERTY(QVariantList recentFiles READ recentFiles NOTIFY recentsChanged)
    Q_PROPERTY(QVariantList templates READ templates NOTIFY templatesChanged)
    // --- editing ---
    Q_PROPERTY(int currentSlide READ currentSlide WRITE setCurrentSlide NOTIFY currentSlideChanged)
    Q_PROPERTY(QString selectedId READ selectedId NOTIFY selectionChanged)
    Q_PROPERTY(QStringList selectedIds READ selectedIds NOTIFY selectionChanged)
    Q_PROPERTY(int selectionCount READ selectionCount NOTIFY selectionChanged)
    Q_PROPERTY(int groupDepth READ groupDepth NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selection READ selection NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap textSelection READ textSelection NOTIFY textSelectionChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
    Q_PROPERTY(int revision READ revision NOTIFY documentChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY documentChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY documentChanged)
    Q_PROPERTY(QString undoLabel READ undoLabel NOTIFY documentChanged)
    Q_PROPERTY(QSizeF slideSize READ slideSize NOTIFY documentChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY documentChanged)
    Q_PROPERTY(bool snapEnabled READ snapEnabled WRITE setSnapEnabled NOTIFY snapEnabledChanged)
    // The alignments the current drag is sitting on, for the canvas to draw.
    Q_PROPERTY(QVariantList guides READ guides NOTIFY guidesChanged)
    Q_PROPERTY(QString slideNotes READ slideNotes NOTIFY selectionChanged)
    Q_PROPERTY(QStringList selectedSlides READ selectedSlides NOTIFY slideSelectionChanged)
    Q_PROPERTY(QVariantMap slideSelectionState READ slideSelectionState NOTIFY slideSelectionChanged)
    Q_PROPERTY(QVariantList navigator READ navigator NOTIFY documentChanged)
    Q_PROPERTY(QVariantList customShows READ customShows NOTIFY documentChanged)
    Q_PROPERTY(QString activeShow READ activeShow WRITE setActiveShow NOTIFY documentChanged)
    Q_PROPERTY(QVariantList browserSlides READ browserSlides NOTIFY browserChanged)
    Q_PROPERTY(QStringList collapsedSections READ collapsedSections NOTIFY browserChanged)
    Q_PROPERTY(QVariantList builds READ builds NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList slideObjects READ slideObjects NOTIFY selectionChanged)
    Q_PROPERTY(qreal slideStart READ slideStart NOTIFY selectionChanged)
    Q_PROPERTY(qreal slideDuration READ slideDuration NOTIFY selectionChanged)
    Q_PROPERTY(qreal localTime READ localTime NOTIFY timeChanged)
    Q_PROPERTY(qreal playbackRate READ playbackRate WRITE setPlaybackRate NOTIFY playbackRateChanged)
    Q_PROPERTY(QVariantMap design READ design NOTIFY documentChanged)
    Q_PROPERTY(QVariantMap layoutPreview READ layoutPreview NOTIFY layoutPreviewChanged)
    Q_PROPERTY(QVariantMap slideDesign READ slideDesign NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap deckImport READ deckImport NOTIFY deckImportChanged)
    Q_PROPERTY(QVariantMap designAudit READ designAudit NOTIFY documentChanged)
    Q_PROPERTY(QVariantList comments READ comments NOTIFY documentChanged)
    Q_PROPERTY(QVariantMap statistics READ statistics NOTIFY documentChanged)
    Q_PROPERTY(QVariantList reviewIssues READ reviewIssues NOTIFY documentChanged)
    Q_PROPERTY(QVariantList readingOrder READ readingOrder NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList outline READ outline NOTIFY documentChanged)
    Q_PROPERTY(QVariantMap findState READ findState NOTIFY findChanged)
    Q_PROPERTY(QVariantMap panelState READ panelState CONSTANT)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY viewPreferencesChanged)
    Q_PROPERTY(bool highContrast READ highContrast WRITE setHighContrast NOTIFY viewPreferencesChanged)
    Q_PROPERTY(QVariantMap slideTransition READ slideTransition NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList exportQueue READ exportQueue NOTIFY exportQueueChanged)
    Q_PROPERTY(bool encoderAvailable READ encoderAvailable CONSTANT)
    Q_PROPERTY(QVariantList printers READ printers NOTIFY printersChanged)
    Q_PROPERTY(bool printersKnown READ printersKnown NOTIFY printersChanged)
    Q_PROPERTY(QString reviewAuthor READ reviewAuthor WRITE setReviewAuthor NOTIFY reviewAuthorChanged)

public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;

    QUrl fileUrl() const { return m_fileUrl; }
    QString fileName() const;
    QVariantMap fileState() const;
    QVariantMap importReport() const;
    Q_INVOKABLE void dismissImportReport();
    // Typefaces the deck names that are not installed here, each with a
    // suggested stand-in; and the rewrite, as one undo step.
    Q_INVOKABLE QVariantList missingFonts() const;
    Q_INVOKABLE int substituteFonts(const QVariantMap &replacements);
    Q_INVOKABLE void keepMyVersion();
    Q_INVOKABLE void reloadFromDisk();
    QString status() const { return m_status; }
    bool busy() const { return m_busy; }
    QString operation() const { return m_operation.isEmpty() && m_imageImportRunning ? tr("Importing pictures…") : m_operation; }
    Q_INVOKABLE void cancelOperation();
    Q_INVOKABLE void openAsync(const QUrl &url, bool recovery = false);
    Q_INVOKABLE void saveAsync(const QString &path);
    Q_INVOKABLE void exportPdfAsync(const QString &path, bool stages = false, bool skipped = false);
    Q_INVOKABLE bool insertImageAsync(const QUrl &url);
    Q_INVOKABLE void copyAsync(bool cut = false);
    Q_INVOKABLE void pasteAsync();
    Q_INVOKABLE void previewLayoutAsync(const QString &id, int scope, const QVariantMap &mapping, int geometry);

    const Document &document() const { return m_document; }
    const PresentationCache &presentation(bool includeSkipped) const;
    void setDocument(const Document &document);

    bool includeSkipped() const { return m_includeSkipped; }
    void setIncludeSkipped(bool value);
    qreal time() const { return m_time; }
    qreal duration() const;
    bool playing() const { return m_playing; }
    // Called by a SlideView as its window starts each frame, so the time a
    // frame shows is read at the display's own pace rather than whenever the
    // ticker last fired.
    void frameTick();
    int slideCount() const { return m_document.slides.size(); }
    int slideIndex() const;
    bool inTransition() const;
    QString timecode() const;

    // Pure helpers live here as statics so the tests can reach them without a
    // window, an engine or a desktop.
    static QString displayNameFor(const QUrl &url);

    QString mediaJobLabel() const { return m_mediaJobLabel; }
    QVariantMap mediaOptimisation() const;
    Q_INVOKABLE void optimiseSelectedMedia() { if(selectionCount()==1 && selectedObject() && (selectedObject()->type==ObjectType::Media || (!selectedObject()->image.isNull() && selectedObject()->imageFormat!="svg"))) emit mediaOptimisationRequested(); }
    Q_INVOKABLE bool previewMediaOptimisation(int preset);
    Q_INVOKABLE bool applyMediaOptimisation(bool keepOriginal = true);
    Q_INVOKABLE void discardMediaOptimisation();
    Q_INVOKABLE void restoreOriginalMedia();
    Q_INVOKABLE void discardOriginalMedia();
    Q_INVOKABLE void playMediaComparison(bool compressed);
    Q_INVOKABLE void seekMediaComparison(qreal seconds);
    QImage mediaComparisonFrame(bool compressed,qreal seconds) const;
    QVariantList mediaTransport() const;
    void setMediaSuppressed(bool value);
    int mediaProgress() const { return m_mediaJob ? m_mediaJob->progress.load() : 0; }
    QVariantList mediaPreflight() const;
    Q_INVOKABLE void refreshMediaPreflight();
    Q_INVOKABLE void insertMediaDialog(bool embed = true);
    Q_INVOKABLE bool insertMedia(const QUrl &url, bool embed = true);
    Q_INVOKABLE void relinkMediaDialog();
    Q_INVOKABLE bool replaceMedia(const QUrl &url, bool embed = true);
    Q_INVOKABLE void approveMedia(const QString &slideId, const QString &objectId);
    Q_INVOKABLE void embedSelectedMedia();
    Q_INVOKABLE void cancelMediaJob();
    Q_INVOKABLE void setMediaPlayback(qreal start, qreal end, int loops, qreal volume, bool onClick);
    Q_INVOKABLE void previewMedia();
    Q_INVOKABLE void stopMedia();
    QVector<SceneObject> statesAt(qreal time, bool includeSkipped) const;
    Q_INVOKABLE void openDialog();
    Q_INVOKABLE void open(const QUrl &url);

    // --- the document on disk ----------------------------------------------
    bool startVisible() const { return m_startVisible; }
    bool hasDocument() const { return m_hasDocument; }
    Q_INVOKABLE void showStart();
    Q_INVOKABLE void resumeDeck();
    Q_INVOKABLE bool createDeck(int theme, qreal width, qreal height, int layout = 0);
    QVariantList templates() const;
    Q_INVOKABLE void refreshTemplates();
    Q_INVOKABLE bool createFromTemplate(const QString &id);
    Q_INVOKABLE void installTemplateDialog();
    Q_INVOKABLE bool installTemplate(const QUrl &url);
    Q_INVOKABLE bool saveAsTemplate(const QString &name);
    Q_INVOKABLE bool removeTemplate(const QString &id);
    QVariantList recentFiles() const;
    Q_INVOKABLE void pinRecent(const QString &path, bool pinned);
    Q_INVOKABLE void removeRecent(const QString &path);
    Q_INVOKABLE void revealRecent(const QString &path);
    Q_INVOKABLE void newDeck();
    Q_INVOKABLE bool openInNewWindow(const QUrl &url = QUrl());
    Q_INVOKABLE void save();          // straight to the current file, or asks
    Q_INVOKABLE void saveAsDialog();
    Q_INVOKABLE bool saveTo(const QString &path);

    // --- autosave and recovery ---------------------------------------------
    // Journals left by processes that are gone. Empty on a clean launch, which
    // is why nothing is shown unless there is genuinely something to offer.
    Q_INVOKABLE QVariantList recoveryCandidates() const;
    Q_INVOKABLE void recoverFrom(const QString &journalPath);
    Q_INVOKABLE void discardRecovery(const QString &journalPath);

    // --- export --------------------------------------------------------------
    Q_INVOKABLE void exportPdfDialog(bool pagePerBuildStage = false, bool includeSkipped = false);
    Q_INVOKABLE bool exportPdf(const QString &path, bool pagePerBuildStage = false, bool includeSkipped = false);

    Q_INVOKABLE void setTime(qreal time);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void restart();
    Q_INVOKABLE void step(qreal seconds);

    // The export path, and what bin/shot drives. Same evaluator, same renderer
    // as the live view — that identity is the whole architectural bet, and
    // tst_omashow::exportMatchesTheLiveView holds it to it.
    Q_INVOKABLE bool renderFrame(qreal time, const QString &path, int width = 1920, bool includeSkipped = false) const;

    // --- editing -----------------------------------------------------------
    int currentSlide() const { return m_currentSlide; }
    void setCurrentSlide(int index);
    QString selectedId() const { return m_selectedId; }
    QVariantMap selection() const;
    bool hasSelection() const;
    int revision() const { return m_revision; }
    bool canUndo() const { return m_history.canUndo(); }
    bool canRedo() const { return m_history.canRedo(); }
    QString undoLabel() const { return m_history.undoLabel(); }
    QSizeF slideSize() const { return m_document.size; }
    bool modified() const { return m_modified; }
    bool snapEnabled() const { return m_snapEnabled; }
    void setSnapEnabled(bool enabled);
    QVariantList guides() const { return m_guides; }

    // Settled time for a slide — what the Edit canvas shows, so an object with
    // a build in is visible while you are arranging it rather than invisible.
    Q_INVOKABLE qreal settledTime(int slideIndex) const;

    QStringList selectedSlides() const;
    QVariantMap slideSelectionState() const;
    Q_INVOKABLE void selectSlide(int index, bool toggle = false, bool range = false);
    Q_INVOKABLE void selectAllSlides();
    Q_INVOKABLE void duplicateSelectedSlides();
    Q_INVOKABLE void deleteSelectedSlides();
    Q_INVOKABLE void moveSelectedSlides(int target, bool after = false);
    Q_INVOKABLE void nudgeSelectedSlides(int direction);
    Q_INVOKABLE void setSlidesSkipped(bool skipped);
    QVariantList customShows() const;
    QString activeShow() const { return m_activeShow; }
    void setActiveShow(const QString &id);
    Q_INVOKABLE QString addCustomShow(const QString &name);
    Q_INVOKABLE bool renameCustomShow(const QString &id, const QString &name);
    Q_INVOKABLE bool removeCustomShow(const QString &id);
    Q_INVOKABLE bool setCustomShowSlides(const QString &id, const QStringList &slideIds);
    Q_INVOKABLE bool moveCustomShowSlide(const QString &id, int from, int to);
    Q_INVOKABLE bool resizeDeck(qreal width, qreal height, bool scaleContent = true);
    Q_INVOKABLE QRectF selectionVisualBounds() const;
    Q_INVOKABLE void rotateSelection(qreal degrees, bool snap = false);
    Q_INVOKABLE void resizeSelectionHandle(qreal hx, qreal hy, qreal dx, qreal dy);
    Q_INVOKABLE void addSlide();
    Q_INVOKABLE void duplicateSlide();
    Q_INVOKABLE void deleteSlide();
    Q_INVOKABLE void moveSlide(int from, int to);

    Q_INVOKABLE bool selectAt(qreal x, qreal y, bool extend = false, bool behind = false);
    Q_INVOKABLE void select(const QString &id);
    Q_INVOKABLE void clearSelection();

    bool canPaste() const;
    Q_INVOKABLE bool copySelected();
    Q_INVOKABLE void cutSelected();
    Q_INVOKABLE void paste();
    QVariantMap clipboardKinds() const;
    Q_INVOKABLE void pasteSpecial(int mode);
    Q_INVOKABLE void duplicateSelected();
    Q_INVOKABLE void openRecent(const QString &path) { openAsync(QUrl::fromLocalFile(path)); }
    Q_INVOKABLE void fitSelectedTextBox();
    Q_INVOKABLE void commitTextDocument(const QString &slideId, const QString &id, QQuickTextDocument *editor);
    QVariantMap textSelection() const;
    Q_INVOKABLE void setTextSelection(int start, int end);
    Q_INVOKABLE bool formatSelection(const QString &key, const QVariant &value);
    Q_INVOKABLE int pasteEditorText(QQuickTextDocument *editor, int start, int end);
    Q_INVOKABLE void resetImageCrop();
    Q_INVOKABLE void insertImageDialog();
    Q_INVOKABLE void replaceImageDialog();
    Q_INVOKABLE bool insertImage(const QUrl &url);
    Q_INVOKABLE bool replaceImage(const QUrl &url);
    Q_INVOKABLE void resetImageAdjustments();
    Q_INVOKABLE void editSelectedImageCrop() { if(selectionCount()==1 && selection().value("type").toString()=="image") emit imageCropEditorRequested(); }
    Q_INVOKABLE void resizeImageCrop(int handle,qreal x,qreal y,bool lockAspect=false);
    QStringList chartNames() const;
    QVariantMap diagramPreview() const { return m_diagramPreview; }
    QVector<SceneObject> diagramObjects() const;
    Q_INVOKABLE void previewDiagram(int kind,const QString &outline,bool vertical);
    Q_INVOKABLE bool insertDiagram();
    Q_INVOKABLE void clearDiagramPreview();
    Q_INVOKABLE bool addChart(int kind=0);
    Q_INVOKABLE bool setChartProperty(const QString &key,const QVariant &value);
    Q_INVOKABLE bool setChartThemeColor(int slot,const QString &color);
    Q_INVOKABLE bool setChartSeriesColor(const QString &id,const QString &color);
    TableModel *tableModel();
    Q_INVOKABLE bool addTable(int rows=4,int columns=3);
    Q_INVOKABLE void editSelectedTable();
    bool applyTable(const QString &slideId,const QString &objectId,const TableData &table,const QString &label,const DataSource *source=nullptr);
    Q_INVOKABLE void addText();
    Q_INVOKABLE void addEquation();
    Q_INVOKABLE bool putTextOnShape();
    // What the deck is written in, what it has been taught, and whether it
    // smartens the punctuation as words are committed.
    Q_PROPERTY(QString deckLanguage READ deckLanguage NOTIFY documentChanged)
    Q_PROPERTY(QStringList knownWords READ knownWords NOTIFY documentChanged)
    Q_PROPERTY(bool smartPunctuation READ smartPunctuation NOTIFY documentChanged)
    Q_PROPERTY(bool spellingAvailable READ spellingAvailable NOTIFY documentChanged)
    QString deckLanguage() const;
    QStringList knownWords() const;
    bool smartPunctuation() const;
    bool spellingAvailable() const;
    Q_INVOKABLE QStringList spellingLanguages() const;
    Q_INVOKABLE QStringList spellingSuggestions(const QString &word) const;
    Q_INVOKABLE bool setDeckLanguage(const QString &language);
    Q_INVOKABLE bool setSmartPunctuation(bool on);
    Q_INVOKABLE bool teachWord(const QString &word);
    Q_INVOKABLE bool forgetWord(const QString &word);
    Q_INVOKABLE bool takeTextOffPath();
    Q_INVOKABLE void addRect();
    Q_INVOKABLE QString setObjectLink(int kind, const QString &target);
    Q_INVOKABLE void editSelectedLink() { if(hasSelection()) emit linkEditorRequested(); }
    QVariantList connectorTargets() const;
    Q_INVOKABLE bool connectSelected(int route = 1);
    Q_INVOKABLE void attachConnector(bool start, const QString &target, int side = 0);
    QStringList shapeNames() const;
    QVariantList objectStyles() const;
    Q_INVOKABLE void saveObjectStyle(const QString &name, const QString &existingId = QString());
    Q_INVOKABLE void applyObjectStyle(const QString &id);
    Q_INVOKABLE void removeObjectStyle(const QString &id);
    QVariantList textStyles() const;
    Q_INVOKABLE QString addTextStyle(const QString &name);
    Q_INVOKABLE bool applyTextStyle(const QString &id);
    Q_INVOKABLE bool updateTextStyleFromSelection();
    Q_INVOKABLE bool renameTextStyle(const QString &id, const QString &name);
    Q_INVOKABLE bool removeTextStyle(const QString &id);
    Q_INVOKABLE void addShape(int kind);
    QVector<SceneObject> combinedShapes(int operation) const;
    Q_INVOKABLE bool combineShapes(int operation);
    Q_INVOKABLE void setShapeImageDialog();
    Q_INVOKABLE bool addPath(const QVariantList &points, bool closed, bool smooth);
    Q_INVOKABLE void convertToPath();
    Q_INVOKABLE void setPathClosed(bool closed);
    Q_INVOKABLE void movePathNode(int command, int coordinate, qreal x, qreal y);
    Q_INVOKABLE void removePathNode(int command);
    Q_INVOKABLE void deleteSelected();
    Q_INVOKABLE void raiseSelected(int delta);

    // One gesture, one undo step: a drag calls begin once, moves as often as it
    // likes, then ends. The intermediate positions never reach the stack.
    Q_INVOKABLE void beginEdit(const QString &label);
    Q_INVOKABLE void endEdit();
    // `tolerance` is in document units and comes from the canvas, which knows
    // the zoom — so snapping feels like the same distance at any magnification.
    Q_INVOKABLE void setSelectedRect(qreal x, qreal y, qreal w, qreal h,
                                     qreal tolerance = 0.0, bool keepSize = true);
    Q_INVOKABLE void nudgeSelected(qreal dx, qreal dy);
    Q_INVOKABLE void setSelectedProperty(const QString &name, const QVariant &value);

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    QStringList selectedIds() const;
    int selectionCount() const { return selectedIds().size(); }
    int groupDepth() const { return m_groupScope.size(); }
    Q_INVOKABLE void selectIds(const QStringList &ids);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void selectRegion(qreal x, qreal y, qreal w, qreal h, bool extend = false);
    Q_INVOKABLE void groupSelected();
    Q_INVOKABLE void ungroupSelected();
    Q_INVOKABLE void enterGroup();
    Q_INVOKABLE void leaveGroup();
    Q_INVOKABLE void alignSelected(const QString &mode, int reference = 0);
    Q_INVOKABLE void distributeSelected(bool horizontal, int reference = 0);
    Q_INVOKABLE void showAllObjects();
    Q_INVOKABLE void unlockAllObjects();
    Q_INVOKABLE void cancelEdit();
    QVariantList navigator() const;
    QVariantList browserSlides() const;
    QStringList collapsedSections() const;
    Q_INVOKABLE QVariantMap sectionInfo(const QString &id) const;
    Q_INVOKABLE void setSectionCollapsed(const QString &id, bool collapsed);
    Q_INVOKABLE void collapseAllSections(bool collapsed);
    Q_INVOKABLE void selectSection(const QString &id, bool toggle = false, bool range = false);
    Q_INVOKABLE void moveSection(const QString &id, int direction);
    Q_INVOKABLE void startSection(const QString &name);
    Q_INVOKABLE void renameSection(const QString &id, const QString &name);
    Q_INVOKABLE void removeSection(const QString &id);
    QVariantList builds() const;
    QVariantList slideObjects() const;
    qreal slideStart() const;
    qreal slideDuration() const;
    qreal localTime() const;
    qreal playbackRate() const { return m_playbackRate; }
    void setPlaybackRate(qreal rate);
    Q_INVOKABLE void setLocalTime(qreal time);
    Q_INVOKABLE void previewSlide();
    void playUntil(qreal end);
    QString slideNotes() const;
    Q_INVOKABLE void setSlideNotes(const QString &notes);
    Q_INVOKABLE void previewBuild(int index);
    Q_INVOKABLE int addBuild(const QString &targetId, int phase = 0, int effect = 1);
    Q_INVOKABLE int addBuildForSelection(int phase = 0, int effect = 1);
    Q_INVOKABLE void removeBuild(int index);
    Q_INVOKABLE void moveBuild(int from, int to);
    Q_INVOKABLE void setBuildProperty(int index, const QString &key, const QVariant &value);
    QVariantMap design() const;
    QVariantMap slideDesign() const;
    Q_INVOKABLE void setupDesign();
    Q_INVOKABLE void applyTheme(int index);
    Q_INVOKABLE void setThemeToken(const QString &name, const QString &value, bool font = false);
    Q_INVOKABLE void applyLayout(const QString &id);
    QVariantMap layoutPreview() const;
    const Document &layoutPreviewDocument() const { return m_layoutPreviewDocument; }
    Q_INVOKABLE void previewLayout(const QString &id, int scope, const QVariantMap &mapping, int geometry);
    Q_INVOKABLE bool applyLayoutPreview();
    Q_INVOKABLE void clearLayoutPreview();
    Q_INVOKABLE void setMasterArtworkVisible(bool visible);
    Q_INVOKABLE void setMasterFieldsVisible(bool visible);
    Q_INVOKABLE void setMasterField(const QString &masterId, const QString &key, const QVariant &value);
    Q_INVOKABLE void setSlideBackground(const QString &color, bool reset = false);
    Q_INVOKABLE void resetPlaceholder(bool geometry);
    Q_INVOKABLE QString addMaster(const QString &copyId = QString());
    Q_INVOKABLE void setMasterProperty(const QString &id, const QString &key, const QString &value);
    Q_INVOKABLE void moveMaster(const QString &id, int delta);
    Q_INVOKABLE bool deleteMaster(const QString &id, const QString &replacement = QString());
    Q_INVOKABLE QString addLayout(const QString &masterId, const QString &copyId = QString());
    Q_INVOKABLE void setLayoutProperty(const QString &id, const QString &key, const QString &value);
    Q_INVOKABLE bool deleteLayout(const QString &id, const QString &replacement = QString());
    Q_INVOKABLE void setPlaceholderProperty(const QString &id, const QString &role, const QString &key, const QVariant &value);
    Q_INVOKABLE void addPlaceholder(const QString &id, bool shape = false);
    Q_INVOKABLE void movePlaceholder(const QString &id, const QString &role, int delta);
    Q_INVOKABLE void deletePlaceholder(const QString &id, const QString &role);

    // --- design and slides from another deck, and what this one no longer uses
    Q_INVOKABLE void importDeckDialog();
    Q_INVOKABLE void importFromDeck(const QUrl &url);
    QVariantMap deckImport() const;
    const Document &importPreviewDocument() const { return m_importDocument; }
    const Document &importSourceDocument() const { return m_importSource; }
    Q_INVOKABLE void setImportOption(const QString &key, const QVariant &value);
    Q_INVOKABLE void setImportSlideSelected(int index, bool selected);
    Q_INVOKABLE void setImportSlidesSelected(bool selected);
    Q_INVOKABLE bool applyImport();
    Q_INVOKABLE void clearImport();
    QVariantMap designAudit() const;
    Q_INVOKABLE bool removeUnusedDesign(const QStringList &ids);

    // --- review -------------------------------------------------------------
    QString reviewAuthor() const;
    void setReviewAuthor(const QString &author);
    QVariantList comments() const;
    QVariantMap statistics() const;
    QVariantList reviewIssues() const;
    QVariantList readingOrder() const;
    QVariantList outline() const;
    QVariantMap findState() const;
    QVariantMap panelState() const;
    Q_INVOKABLE void setPanelState(const QString &key, const QVariant &value);
    bool reducedMotion() const;
    void setReducedMotion(bool reduced);
    bool highContrast() const;
    void setHighContrast(bool high);
    Q_INVOKABLE bool setOutlineText(const QString &slideId, const QString &objectId, const QString &text);
    Q_INVOKABLE QString addComment(const QString &text, bool onSelection = true);
    Q_INVOKABLE QString replyToComment(const QString &parentId, const QString &text);
    Q_INVOKABLE bool setCommentText(const QString &id, const QString &text);
    Q_INVOKABLE bool resolveComment(const QString &id, bool resolved);
    Q_INVOKABLE bool removeComment(const QString &id);
    Q_INVOKABLE void goToComment(const QString &id);
    Q_INVOKABLE bool moveReadingOrder(const QString &objectId, int delta);
    Q_INVOKABLE bool resetReadingOrder();
    Q_INVOKABLE bool describeObject(const QString &objectId, const QString &title, const QString &text);
    Q_INVOKABLE bool dismissIssue(const QString &key, bool dismissed);
    Q_INVOKABLE void goToIssue(const QString &key);
    Q_INVOKABLE void exportReviewDialog();
    Q_INVOKABLE bool exportReview(const QString &path, bool includeDismissed = false);
    Q_INVOKABLE void findText(const QString &needle, const QVariantMap &options);
    Q_INVOKABLE void goToMatch(int index);
    Q_INVOKABLE bool replaceMatch(int index, const QString &replacement);
    Q_INVOKABLE int replaceAllMatches(const QString &replacement);
    Q_INVOKABLE void clearFind();
    QVariantMap slideTransition() const;
    Q_INVOKABLE bool applyRehearsedTimings(const QVariantList &timings);
    QVariantList exportQueue() const;
    bool encoderAvailable() const;
    QVariantList printers() const;
    bool printersKnown() const;
    Q_INVOKABLE void refreshPrinters();
    Q_INVOKABLE void exportDialog(const QVariantMap &options);
    Q_INVOKABLE int queueExport(const QVariantMap &options);
    Q_INVOKABLE void cancelExport(int id);
    Q_INVOKABLE bool retryExport(int id);
    Q_INVOKABLE void clearFinishedExports();
    Q_INVOKABLE bool setSlideTransition(const QString &key, const QVariant &value,
                                        bool everySlide = false);

signals:
    void opened();
    void importReportChanged();
    void importReady();
    void deckImportChanged();
    void findChanged();
    void reviewAuthorChanged();
    void exportQueueChanged();
    void printersChanged();
    void viewPreferencesChanged();
    void operationChanged();
    void layoutPreviewChanged();
    void diagramPreviewChanged();
    void tableEditorRequested();
    void mediaOptimisationRequested();
    void mediaOptimisationChanged();
    void mediaPreviewRequested();
    void mediaJobChanged();
    void mediaImportFinished(bool success);
    void linkEditorRequested();
    void imageCropEditorRequested();
    void browserChanged();
    void slideSelectionChanged();
    void clipboardChanged();
    void startChanged();
    void recentsChanged();
    void templatesChanged();
    void saved();
    void saveCanceled();
    void playbackRateChanged();
    void fileUrlChanged();
    void fileStateChanged();
    void statusChanged();
    void busyChanged();
    void failed(const QString &message);
    void timeChanged();
    void deckChanged();
    void playingChanged();
    void currentSlideChanged();
    void selectionChanged();
    void textSelectionChanged();
    void textFormattingChanged();
    void documentChanged();
    void snapEnabledChanged();
    void guidesChanged();

private:
    int m_clipboardVersion = 0;
    bool m_copyPending = false, m_pasteAfterCopy = false;
    struct ImageRequest {
        QUrl url;
        bool replace;
        QString slide, target;
        QStringList groups;
        int generation;
    };
    QList<ImageRequest> m_imageQueue;
    bool m_imageImportRunning = false, m_cancelImages = false;
    void startImageImport();
    void commitImageImport(const ImageRequest &request, const SceneObject &image, const QString &error);
    quint64 m_layoutRequest = 0;
    bool m_layoutRunning = false;
    std::function<void()> m_nextLayoutPreview;
    QString m_operation;
    std::shared_ptr<Workers::Job> m_operationJob, m_journalJob;
    bool m_journalRunning = false, m_journalAgain = false;
    void cancelJournal();
    bool beginOperation(const QString &label);
    void endOperation();
    void acceptOpen(const Document &document, const QUrl &url);
    bool loadImageAsync(const QUrl &url, bool replace, int index, const QString &target);
    bool applyImage(SceneObject image, bool replace, int index, const QString &target, const QStringList &groups);
    TableModel *m_tableModel = nullptr;
    QString m_mediaJobLabel;
    SceneObject m_mediaPreviewSource, m_mediaPreview;
    QString m_mediaPreviewSlide;
    int m_mediaPreviewGeneration = -1;
    bool m_mediaOptimising = false, m_imageOptimisation = false;
    bool m_mediaPreviewReady = false, m_comparisonPlaying = false, m_comparisonCompressed = true;
    qreal m_comparisonTime = 0, m_comparisonFrom = 0;
    QElapsedTimer m_comparisonClock;
    QTimer m_comparisonTicker;
    MediaPlayback *m_comparisonAudio = nullptr;
    void syncMediaComparison();
    MediaPlayback *m_mediaPlayback = nullptr;
    bool m_mediaSuppressed = false;
    void syncMedia();
    std::shared_ptr<MediaAsset::Job> m_mediaJob;
    QTimer m_mediaProgressTimer;
    QHash<QString,QString> m_mediaPermissions;
    int m_documentGeneration = 0, m_mediaPickerGeneration = 0;
    bool m_mediaPickerEmbed = true;
    QString m_mediaPickerSlide, m_mediaPickerTarget;
    bool loadMedia(const QUrl &url, bool embed, const QString &slideId, const QString &target = {}, bool approveOnly = false);
    void resetMediaSession();
    bool m_includeSkipped = true, m_pendingPdfSkipped = false;
    bool m_startVisible = true;
    bool m_hasDocument = false;
    void activateDocument();
    void rememberRecent(const QString &path);
    void setStatus(const QString &status);
    void setBusy(bool busy);
    void setFileUrl(const QUrl &url);
    void tick();
    void scheduleAutosave();
    void writeJournal();

    PortalFileChooser *m_chooser;
    // The portal answers `selected` for every dialog, so the intent behind the
    // one in flight has to be remembered — otherwise a save would open the file
    // it was about to write, or a PDF export would overwrite the deck.
    enum class Pending { None, SaveDeck, ExportPdf, InsertImage, ReplaceImage, InsertMedia, ReplaceMedia, ImportDeck, ExportReview, ExportFile, InstallTemplate };
    QString m_imageTargetId, m_imageSlideId;
    bool loadImage(const QUrl &url, bool replace, int index, const QString &target);
    Pending m_pending = Pending::None;
    bool m_pendingPdfStages = false;
    QUrl m_fileUrl;
    // A deck converted from another application: where it came from, and
    // what was said about the conversion. Cleared by the next open.
    QString m_importedFrom;
    QStringList m_importWarnings;
    void acceptImported(const Document &document, const QString &sourcePath, const QString &kind, const QStringList &warnings);
    QFileSystemWatcher m_watcher;
    QDateTime m_fileStamp;
    qint64 m_fileBytes = -1;
    bool m_fileChangedOnDisk = false, m_openedElsewhere = false;
    void watchFile();
    QString m_status;
    bool m_busy = false;

    void touch();
    bool applyTransition(Slide &slide, const QString &key, const QVariant &value) const;
    SceneObject *selectedObject();
    const SceneObject *selectedObject() const;

    Document m_document;
    mutable PresentationCache m_presentation;
    mutable int m_presentationRevision = -1, m_navigatorRevision = -1;
    mutable bool m_presentationSkipped = false;
    mutable QVariantList m_navigator;
    History m_history;
    void resetSlideSelection();
    void restoreCurrentSlide(const QString &id);
    QString m_activeShow;
    void applyActiveShow();
    QStringList m_slideSelection;
    QStringList m_collapsedSections;
    QString m_slideAnchor;
    int m_currentSlide = 0;
    QString m_selectedId;
    int m_textSelectionStart = 0, m_textSelectionEnd = 0;
    QStringList m_selectedIds;
    QStringList m_groupScope;
    QVariantMap m_diagramPreview;
    void refreshImport();
    Document m_importSource, m_importDocument;
    QVariantMap m_importPlan, m_importOptions;
    QList<int> m_importSlides;
    QStringList m_importSlideIds;
    // One export at a time, each against the deck as it was when it was queued.
    struct ExportEntry {
        int id = 0;
        int generation = 0;   // which deck it was queued from
        Exports::Request request;
        QString state, message;
        QStringList log;
        int progress = 0;
        Document document;
        std::shared_ptr<Workers::Job> job;
        std::shared_ptr<std::atomic_int> percent;
    };
    QVector<ExportEntry> m_exports;
    QVariantMap m_pendingExport;
    QVariantList m_printers;
    QString m_defaultPrinter;
    bool m_printersRunning = false, m_printersKnown = false;
    int m_reducedMotion = -1, m_highContrast = -1;
    QTimer m_exportTicker;
    int m_exportSerial = 0;
    void startNextExport();
    QString m_reviewAuthor, m_findNeedle;
    QVariantMap m_findOptions, m_findState;
    QString m_importName;
    int m_importBaseRevision = -1, m_importRevision = 0, m_importPreviewSerial = 0;
    QVariantMap m_layoutPreview;
    Document m_layoutPreviewDocument;
    int m_layoutPreviewRevision = 0, m_layoutBaseRevision = -1, m_layoutScope = 0;
    QStringList m_layoutSlideIds;
    QStringList layoutSlideIds(int scope) const;
    QVector<SceneObject> m_diagramObjects;
    int m_diagramRevision=0,m_diagramBaseRevision=-1;
    QString m_diagramSlide;
    QStringList m_diagramScope;
    Slide m_gestureBasis;
    QRectF m_gestureBounds;
    // Where a new chart or table belongs on the current slide: the layout's
    // body area if it has one, else the space under the title, else `fallback`.
    // Where new content goes; `share` is the least part of the slide height it needs.
    QRectF roomForContent(const QRectF &fallback, qreal share = .25) const;
    bool m_gestureActive = false;
    int m_gestureDepth = 0;   // nested beginEdit/endEdit pairs
    QVariantList m_gestureBefore;   // the slide's objects when the gesture began
    bool m_gestureWasModified = false;
    int m_revision = 0;
    bool m_snapEnabled = true;
    QVariantList m_guides;
    bool m_modified = false;
    qreal m_time = 0.0;
    bool m_playing = false;
    QTimer m_ticker;
    QTimer m_autosave;
    QElapsedTimer m_frameTicked; // the last frame-paced tick; the ticker stands down while fresh
    QElapsedTimer m_clock;   // monotonic: playback never drifts with timer jitter
    qreal m_playbackRate = 1.0;
    qreal m_playTo = -1.0;
    qreal m_playFrom = 0.0;
};
