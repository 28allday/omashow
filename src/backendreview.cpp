#include "backend.h"
#include "core/spelling.h"
#include "core/design.h"
#include "core/findreplace.h"
#include "core/review.h"
#include "filepicker.h"
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {
QString authorSettingsPath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/review.ini";
}
QString viewSettingsPath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/view.ini";
}
QString panelSettingsPath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/panels.ini";
}
} // namespace

// How the person wants the interface to behave. Neither touches the deck: a
// file saved with reduced motion on is the same file.
bool Backend::reducedMotion() const {
    if (m_reducedMotion >= 0) return m_reducedMotion == 1;
    if (qEnvironmentVariableIsSet("OMASHOW_REDUCED_MOTION"))
        return qEnvironmentVariableIntValue("OMASHOW_REDUCED_MOTION") != 0;
    QSettings settings(viewSettingsPath(), QSettings::IniFormat);
    return settings.value("reducedMotion", false).toBool();
}

void Backend::setReducedMotion(bool reduced) {
    if (reduced == reducedMotion()) return;
    m_reducedMotion = reduced ? 1 : 0;
    QSettings settings(viewSettingsPath(), QSettings::IniFormat);
    settings.setValue("reducedMotion", reduced);
    settings.sync();
    emit viewPreferencesChanged();
}

bool Backend::highContrast() const {
    if (m_highContrast >= 0) return m_highContrast == 1;
    if (qEnvironmentVariableIsSet("OMASHOW_HIGH_CONTRAST"))
        return qEnvironmentVariableIntValue("OMASHOW_HIGH_CONTRAST") != 0;
    QSettings settings(viewSettingsPath(), QSettings::IniFormat);
    return settings.value("highContrast", false).toBool();
}

void Backend::setHighContrast(bool high) {
    if (high == highContrast()) return;
    m_highContrast = high ? 1 : 0;
    QSettings settings(viewSettingsPath(), QSettings::IniFormat);
    settings.setValue("highContrast", high);
    settings.sync();
    emit viewPreferencesChanged();
}

// Where the panels were left: their widths, and whether they were put away.
QVariantMap Backend::panelState() const {
    QSettings settings(panelSettingsPath(), QSettings::IniFormat);
    QVariantMap state;
    for (const auto &key : settings.childKeys()) state[key] = settings.value(key);
    return state;
}

void Backend::setPanelState(const QString &key, const QVariant &value) {
    static const QStringList known{"navigatorWidth", "inspectorWidth", "navigatorCollapsed",
                                   "inspectorCollapsed", "notesOpen"};
    if (!known.contains(key)) return;
    QSettings settings(panelSettingsPath(), QSettings::IniFormat);
    if (settings.value(key) == value) return;
    settings.setValue(key, value);
    settings.sync();
}

QString Backend::reviewAuthor() const {
    if (!m_reviewAuthor.isEmpty()) return m_reviewAuthor;
    QSettings settings(authorSettingsPath(), QSettings::IniFormat);
    return settings.value("author").toString();
}

void Backend::setReviewAuthor(const QString &author) {
    const auto trimmed = author.trimmed().left(120);
    if (trimmed == reviewAuthor()) return;
    m_reviewAuthor = trimmed;
    QSettings settings(authorSettingsPath(), QSettings::IniFormat);
    settings.setValue("author", trimmed);
    settings.sync();
    emit reviewAuthorChanged();
}

QVariantList Backend::comments() const { return Review::threads(m_document); }

QVariantMap Backend::statistics() const { return Review::statistics(m_document); }

QVariantList Backend::reviewIssues() const { return Review::issues(m_document); }

QVariantList Backend::readingOrder() const {
    if (m_currentSlide < 0 || m_currentSlide >= m_document.slides.size()) return {};
    const auto &slide = m_document.slides.at(m_currentSlide);
    const Slide shown = Design::resolve(m_document, m_currentSlide);
    QVariantList rows;
    for (const auto &id : Review::readingOrder(m_document, m_currentSlide)) {
        const auto *object = slide.find(id);
        const auto *visible = shown.find(id);
        if (!object) continue;
        const auto &described = visible ? *visible : *object;
        QString summary = described.text.section('\n', 0, 0).left(60);
        if (summary.trimmed().isEmpty())
            summary = described.type == ObjectType::Image ? tr("Picture")
                    : described.type == ObjectType::Media ? tr("Film or sound")
                    : described.type == ObjectType::Table ? tr("Table")
                    : described.type == ObjectType::Chart ? tr("Chart")
                                                          : tr("Shape");
        rows.append(QVariantMap{{"id", id}, {"summary", summary},
                                {"alt", described.altText}, {"altTitle", described.altTitle},
                                {"hidden", described.hidden},
                                {"describable", described.type == ObjectType::Image ||
                                                described.type == ObjectType::Media ||
                                                described.type == ObjectType::Table ||
                                                described.type == ObjectType::Chart},
                                {"custom", !slide.readingOrder.isEmpty()}});
    }
    return rows;
}

QVariantList Backend::outline() const {
    QVariantList rows;
    for (int i = 0; i < m_document.slides.size(); ++i) {
        const auto &slide = m_document.slides.at(i);
        const Slide shown = Design::resolve(m_document, i);
        QString titleId, bodyId, title, body;
        // The layout's own roles first; a freeform slide falls back to the order
        // its text was written in.
        for (const auto &o : slide.objects) {
            if (o.type != ObjectType::Text) continue;
            const auto *visible = shown.find(o.id);
            const auto text = visible ? visible->text : o.text;
            if (o.placeholderId == QLatin1String("title") && titleId.isEmpty()) {
                titleId = o.id; title = text;
            } else if (o.placeholderId == QLatin1String("body") && bodyId.isEmpty()) {
                bodyId = o.id; body = text;
            }
        }
        for (const auto &o : slide.objects) {
            if (o.type != ObjectType::Text || o.id == titleId || o.id == bodyId) continue;
            const auto *visible = shown.find(o.id);
            const auto text = visible ? visible->text : o.text;
            if (titleId.isEmpty()) { titleId = o.id; title = text; }
            else if (bodyId.isEmpty()) { bodyId = o.id; body = text; }
        }
        int open = 0;
        for (const auto &comment : m_document.comments)
            if (comment.slideId == slide.id && comment.parentId.isEmpty() && !comment.resolved) ++open;
        rows.append(QVariantMap{{"index", i}, {"slideId", slide.id},
                                {"title", title}, {"titleId", titleId},
                                {"body", body}, {"bodyId", bodyId},
                                {"skipped", slide.skipped}, {"notes", slide.notes},
                                {"comments", open}});
    }
    return rows;
}

bool Backend::setOutlineText(const QString &slideId, const QString &objectId,
                             const QString &text) {
    for (int i = 0; i < m_document.slides.size(); ++i) {
        if (m_document.slides.at(i).id != slideId) continue;
        const auto *existing = m_document.slides.at(i).find(objectId);
        if (!existing || existing->type != ObjectType::Text || existing->locked) return false;
        if (existing->text == text) return false;
        // After the snapshot: begin() shares the document's buffers, so a
        // pointer taken before it would write into the undo copy as well.
        m_history.begin(m_document, tr("Edit outline"));
        auto *object = m_document.slides[i].find(objectId);
        object->text = text;
        Design::markOverride(*object, QStringLiteral("text"));
        m_history.commit();
        touch();
        return true;
    }
    return false;
}

// --- comments --------------------------------------------------------------
QString Backend::addComment(const QString &text, bool onSelection) {
    if (m_currentSlide < 0 || m_currentSlide >= m_document.slides.size()) return {};
    const auto slideId = m_document.slides.at(m_currentSlide).id;
    const auto objectId = onSelection && selectionCount() == 1 ? m_selectedId : QString();
    m_history.begin(m_document, tr("Add comment"));
    const auto id = Review::add(m_document, slideId, objectId, reviewAuthor(), text);
    if (id.isEmpty()) {
        m_history.abandon();
        setStatus(tr("A comment needs some words."));
        return {};
    }
    m_history.commit();
    touch();
    return id;
}

QString Backend::replyToComment(const QString &parentId, const QString &text) {
    m_history.begin(m_document, tr("Reply to comment"));
    const auto id = Review::add(m_document, QString(), QString(), reviewAuthor(), text, parentId);
    if (id.isEmpty()) {
        m_history.abandon();
        return {};
    }
    m_history.commit();
    touch();
    return id;
}

bool Backend::setCommentText(const QString &id, const QString &text) {
    m_history.begin(m_document, tr("Edit comment"));
    if (!Review::setText(m_document, id, text)) {
        m_history.abandon();
        return false;
    }
    m_history.commit();
    touch();
    return true;
}

bool Backend::resolveComment(const QString &id, bool resolved) {
    m_history.begin(m_document, resolved ? tr("Resolve comment") : tr("Reopen comment"));
    if (!Review::setResolved(m_document, id, resolved)) {
        m_history.abandon();
        return false;
    }
    m_history.commit();
    touch();
    return true;
}

bool Backend::removeComment(const QString &id) {
    m_history.begin(m_document, tr("Delete comment"));
    if (!Review::remove(m_document, id)) {
        m_history.abandon();
        return false;
    }
    m_history.commit();
    touch();
    return true;
}

void Backend::goToComment(const QString &id) {
    for (const auto &comment : m_document.comments) {
        if (comment.id != id) continue;
        for (int i = 0; i < m_document.slides.size(); ++i) {
            if (m_document.slides.at(i).id != comment.slideId) continue;
            setCurrentSlide(i);
            if (comment.objectId.isEmpty()) clearSelection();
            else select(comment.objectId);
            return;
        }
    }
}

// --- reading order ---------------------------------------------------------
bool Backend::moveReadingOrder(const QString &objectId, int delta) {
    m_history.begin(m_document, tr("Change reading order"));
    if (!Review::moveReading(m_document, m_currentSlide, objectId, delta)) {
        m_history.abandon();
        return false;
    }
    m_history.commit();
    touch();
    return true;
}

bool Backend::resetReadingOrder() {
    m_history.begin(m_document, tr("Read in stacking order"));
    if (!Review::resetReading(m_document, m_currentSlide)) {
        m_history.abandon();
        return false;
    }
    m_history.commit();
    touch();
    return true;
}

bool Backend::describeObject(const QString &objectId, const QString &title, const QString &text) {
    if (m_currentSlide < 0 || m_currentSlide >= m_document.slides.size()) return false;
    const auto *existing = m_document.slides.at(m_currentSlide).find(objectId);
    if (!existing) return false;
    SceneObject changed = *existing;
    if (!Design::setProperty(changed, QStringLiteral("altTitle"), title) ||
        !Design::setProperty(changed, QStringLiteral("altText"), text))
        return false;
    if (changed.altTitle == existing->altTitle && changed.altText == existing->altText) return false;
    m_history.begin(m_document, tr("Describe object"));
    auto *object = m_document.slides[m_currentSlide].find(objectId);
    object->altTitle = changed.altTitle;
    object->altText = changed.altText;
    Design::markOverride(*object, QStringLiteral("altText"));
    m_history.commit();
    touch();
    return true;
}

// --- findings ---------------------------------------------------------------
bool Backend::dismissIssue(const QString &key, bool dismissed) {
    m_history.begin(m_document, dismissed ? tr("Ignore finding") : tr("Raise finding again"));
    if (!Review::dismiss(m_document, key, dismissed)) {
        m_history.abandon();
        return false;
    }
    m_history.commit();
    touch();
    return true;
}

// --- language and the words this deck knows ---------------------------------
QString Backend::deckLanguage() const { return Review::language(m_document); }
QStringList Backend::spellingLanguages() const { return Spelling::installed(); }
bool Backend::spellingAvailable() const { return Spelling::available(deckLanguage()); }
QStringList Backend::knownWords() const { return m_document.knownWords; }
bool Backend::smartPunctuation() const { return m_document.smartPunctuation; }

bool Backend::setDeckLanguage(const QString &language) {
    if (language.size() > 32 || language == m_document.language) return false;
    m_history.begin(m_document, tr("Change the deck's language"));
    m_document.language = language;
    m_history.commit();
    touch();
    return true;
}
bool Backend::setSmartPunctuation(bool on) {
    if (on == m_document.smartPunctuation) return false;
    m_history.begin(m_document, tr("Change smart punctuation"));
    m_document.smartPunctuation = on;
    m_history.commit();
    touch();
    return true;
}
// A word the deck should know travels with the deck, not with the computer:
// somebody else opening it sees the same review.
bool Backend::teachWord(const QString &word) {
    const auto tidy = word.trimmed();
    if (tidy.isEmpty() || tidy.size() > 120 || m_document.knownWords.contains(tidy)) return false;
    m_history.begin(m_document, tr("Teach the deck a word"));
    m_document.knownWords.append(tidy);
    m_document.knownWords.sort();
    m_history.commit();
    touch();
    return true;
}
bool Backend::forgetWord(const QString &word) {
    if (!m_document.knownWords.contains(word)) return false;
    m_history.begin(m_document, tr("Forget a word"));
    m_document.knownWords.removeAll(word);
    m_history.commit();
    touch();
    return true;
}
QStringList Backend::spellingSuggestions(const QString &word) const {
    return Spelling::suggest(word, deckLanguage(), 6);
}

void Backend::goToIssue(const QString &key) {
    for (const auto &value : Review::issues(m_document)) {
        const auto row = value.toMap();
        if (row.value("key").toString() != key) continue;
        setCurrentSlide(row.value("slide").toInt());
        const auto objectId = row.value("objectId").toString();
        if (objectId.isEmpty()) clearSelection();
        else select(objectId);
        return;
    }
}

void Backend::exportReviewDialog() {
    m_pending = Pending::ExportReview;
    const QString base = m_fileUrl.isLocalFile()
                             ? QFileInfo(m_fileUrl.toLocalFile()).completeBaseName()
                             : QStringLiteral("Untitled");
    m_chooser->saveFile(tr("Export review"), base + QStringLiteral("-review.txt"),
                        tr("Text files"), {QStringLiteral("*.txt")});
}

bool Backend::exportReview(const QString &path, bool includeDismissed) {
    QVariantList wanted;
    for (const auto &value : Review::issues(m_document))
        if (includeDismissed || !value.toMap().value("dismissed").toBool()) wanted.append(value);
    QString error;
    if (!Workers::write(path, Review::report(m_document, wanted, fileName()), &error)) {
        setStatus(tr("Could not write the review: %1").arg(error));
        emit failed(error);
        return false;
    }
    setStatus(tr("Exported the review to %1").arg(QFileInfo(path).fileName()));
    return true;
}

// --- find and replace -------------------------------------------------------
QVariantMap Backend::findState() const { return m_findState; }

void Backend::findText(const QString &needle, const QVariantMap &options) {
    m_findNeedle = needle;
    m_findOptions = options;
    m_findOptions.remove(QStringLiteral("slides"));
    if (options.value("scope").toInt() == 1)
        m_findOptions["slides"] = selectedSlides();
    else if (options.value("scope").toInt() == 2 && m_currentSlide < m_document.slides.size())
        m_findOptions["slides"] = QStringList{m_document.slides.at(m_currentSlide).id};
    const auto matches = FindReplace::find(m_document, needle, m_findOptions);
    m_findState = QVariantMap{{"needle", needle}, {"matches", matches},
                              {"count", matches.size()},
                              {"scope", options.value("scope").toInt()},
                              {"caseSensitive", m_findOptions.value("caseSensitive").toBool()},
                              {"wholeWords", m_findOptions.value("wholeWords").toBool()},
                              {"notes", m_findOptions.value("notes", true).toBool()},
                              {"revision", m_revision}};
    emit findChanged();
}

void Backend::goToMatch(int index) {
    const auto matches = m_findState.value("matches").toList();
    if (index < 0 || index >= matches.size()) return;
    const auto match = matches.at(index).toMap();
    setCurrentSlide(match.value("slide").toInt());
    const auto objectId = match.value("objectId").toString();
    if (objectId.isEmpty()) clearSelection();
    else select(objectId);
    m_findState["current"] = index;
    emit findChanged();
}

bool Backend::replaceMatch(int index, const QString &replacement) {
    const auto matches = m_findState.value("matches").toList();
    if (index < 0 || index >= matches.size() ||
        m_findState.value("revision").toInt() != m_revision)
        return false;
    auto document = m_document;
    if (!FindReplace::replaceOne(document, matches.at(index).toMap(), m_findNeedle,
                                 replacement, m_findOptions)) {
        setStatus(tr("That match has changed since it was found."));
        findText(m_findNeedle, m_findState);
        return false;
    }
    m_history.begin(m_document, tr("Replace"));
    m_document = document;
    m_history.commit();
    touch();
    findText(m_findNeedle, m_findState);
    return true;
}

int Backend::replaceAllMatches(const QString &replacement) {
    if (m_findNeedle.isEmpty()) return 0;
    auto document = m_document;
    const int replaced = FindReplace::replaceAll(document, m_findNeedle, replacement, m_findOptions);
    if (replaced == 0) return 0;
    m_history.begin(m_document, replaced == 1 ? tr("Replace") : tr("Replace %1 matches").arg(replaced));
    m_document = document;
    m_history.commit();
    touch();
    setStatus(replaced == 1 ? tr("Replaced one match")
                            : tr("Replaced %1 matches").arg(replaced));
    findText(m_findNeedle, m_findState);
    return replaced;
}

void Backend::clearFind() {
    m_findNeedle.clear();
    m_findOptions.clear();
    m_findState.clear();
    emit findChanged();
}
