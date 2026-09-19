#include "backend.h"
#include "core/design.h"
#include "core/edit.h"
#include "core/imageasset.h"
#include "core/objectcopy.h"
#include "io/bundle.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QTextCursor>
#include <QTextDocument>
#include <QFutureWatcher>
#include <QtConcurrent>
namespace {
const char *mimeType = "application/x-omashow-objects";
}

void Backend::copyAsync(bool cut) {
    if (!hasSelection() || !beginOperation(cut ? tr("Cutting objects…") : tr("Copying objects…"))) return;
    const auto fragment = ObjectCopy::extract(m_document, m_currentSlide, selectedIds(), m_groupScope.size());
    const auto job = m_operationJob;
    const int revision = m_revision, generation = m_documentGeneration, clipboard = m_clipboardVersion;
    const auto selected = selectedIds(); const int slide = m_currentSlide;
    m_copyPending = true; m_pasteAfterCopy = false;
    QStringList text;
    for (const auto &o : fragment.slides.first().objects) if (o.type == ObjectType::Text) text.append(o.text);
    auto *watcher = new QFutureWatcher<QByteArray>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, job, cut, revision, generation, clipboard, selected, slide, text] {
        const auto bytes = watcher->result(); watcher->deleteLater();
        const bool pasteNext = m_pasteAfterCopy;
        m_copyPending = m_pasteAfterCopy = false;
        endOperation();
        if (job->canceled || generation != m_documentGeneration) return;
        if (clipboard != m_clipboardVersion) { emit failed(tr("The clipboard changed while copying. Copy again to use these objects.")); return; }
        auto *data = new QMimeData;
        data->setData(mimeType, bytes);
        if (!text.isEmpty()) data->setText(text.join('\n'));
        QGuiApplication::clipboard()->setMimeData(data);
        if (cut) {
            if (revision != m_revision || slide != m_currentSlide || selected != selectedIds() || m_gestureActive) {
                emit failed(tr("Copied the original selection; newer edits were kept and nothing was cut.")); return;
            }
            deleteSelected();
        }
        setStatus(tr("Copied %1 objects").arg(selected.size()));
        if (pasteNext) pasteAsync();
    });
    watcher->setFuture(QtConcurrent::run(Workers::io(), [fragment, job] {
        return job->canceled ? QByteArray() : Bundle::toBytes(fragment);
    }));
}

void Backend::pasteAsync() {
    if (m_copyPending) { m_pasteAfterCopy = true; return; }
    if (!operation().isEmpty()) return;
    const auto *data = QGuiApplication::clipboard()->mimeData();
    if (!data) return;
    if (!data->hasFormat(mimeType) && !data->hasImage() && !data->hasFormat("image/svg+xml")) { paste(); return; }
    // Clipboard access belongs to the GUI thread; only its copied payload goes
    // to the worker. Parsing, decompression and picture encoding happen there.
    const bool native = data->hasFormat(mimeType), svg = !native && data->hasFormat("image/svg+xml");
    const auto bytes = native ? data->data(mimeType) : svg ? data->data("image/svg+xml") : QByteArray();
    if (bytes.size() > 128*1024*1024) { emit failed(tr("The clipboard content is too large.")); return; }
    const auto image = !native && !svg ? QGuiApplication::clipboard()->image() : QImage();
    if (!beginOperation(tr("Pasting objects…"))) return;
    const auto job = m_operationJob;
    const int generation = m_documentGeneration, revision = m_revision, index = m_currentSlide;
    const auto groups = m_groupScope; const auto documentSize = m_document.size;
    auto *watcher = new QFutureWatcher<Bundle::ReadResult>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, job, generation, revision, index, groups] {
        const auto result = watcher->result(); watcher->deleteLater(); endOperation();
        if (job->canceled) return;
        if (generation != m_documentGeneration || revision != m_revision || index != m_currentSlide || groups != m_groupScope || m_gestureActive) {
            emit failed(tr("The deck changed while pasting. Paste again to use the current slide.")); return;
        }
        if (!result.ok) { emit failed(result.error); return; }
        m_history.begin(m_document, tr("Paste objects"));
        const auto ids = ObjectCopy::insert(m_document, index, result.document, groups);
        if (ids.isEmpty()) { m_history.cancel(m_document); emit failed(tr("The clipboard contains no usable objects.")); return; }
        m_history.commit(); selectIds(ids); touch();
    });
    watcher->setFuture(QtConcurrent::run(Workers::io(), [native, svg, bytes, image, documentSize, job] {
        Bundle::ReadResult result;
        if (job->canceled) return result;
        if (native) return Bundle::fromBytes(bytes);
        SceneObject picture;
        result.ok = svg ? ImageAsset::decode(picture, bytes, &result.error) : ImageAsset::fromImage(picture, image, &result.error);
        if (result.ok) {
            picture.id = Edit::newId("image");
            auto size = ImageAsset::size(picture); size.scale(documentSize*.7, Qt::KeepAspectRatio);
            picture.rect = QRectF(QPointF((documentSize.width()-size.width())/2, (documentSize.height()-size.height())/2), size);
            Slide slide; slide.id = "image"; slide.objects.append(picture); result.document.slides.append(slide);
        }
        return result;
    }));
}

bool Backend::canPaste() const {
    const auto *data = QGuiApplication::clipboard()->mimeData();
    return data && (data->hasFormat(mimeType) || data->hasText() || data->hasImage() || data->hasFormat("image/svg+xml"));
}
bool Backend::copySelected() {
    if (!hasSelection())
        return false;
    const auto fragment =
        ObjectCopy::extract(m_document, m_currentSlide, selectedIds(), m_groupScope.size());
    auto *data = new QMimeData;
    data->setData(mimeType, Bundle::toBytes(fragment));
    QStringList text;
    for (const auto &o : fragment.slides.first().objects)
        if (o.type == ObjectType::Text)
            text.append(o.text);
    if (!text.isEmpty())
        data->setText(text.join('\n'));
    QGuiApplication::clipboard()->setMimeData(data);
    setStatus(tr("Copied %1 objects").arg(selectionCount()));
    return true;
}
void Backend::cutSelected() {
    if (copySelected())
        deleteSelected();
}
void Backend::paste() {
    const auto *data = QGuiApplication::clipboard()->mimeData();
    if (!data)
        return;
    Document fragment;
    if (data->hasFormat(mimeType)) {
        const auto raw = data->data(mimeType);
        if (raw.size() > 128 * 1024 * 1024) {
            emit failed(tr("The clipboard content is too large."));
            return;
        }
        const auto read = Bundle::fromBytes(raw);
        if (!read.ok) {
            emit failed(tr("The copied objects could not be read: %1").arg(read.error));
            return;
        }
        fragment = read.document;
    } else if (data->hasImage() || data->hasFormat("image/svg+xml")) {
        SceneObject image;
        QString error;
        if (!(data->hasFormat("image/svg+xml") ? ImageAsset::decode(image,data->data("image/svg+xml"),&error) : ImageAsset::fromImage(image,QGuiApplication::clipboard()->image(),&error))) {
            emit failed(error);
            return;
        }
        image.id = Edit::newId("image");
        QSizeF size = ImageAsset::size(image);
        size.scale(m_document.size * .7, Qt::KeepAspectRatio);
        image.rect = QRectF(QPointF((m_document.size.width() - size.width()) / 2,
                                    (m_document.size.height() - size.height()) / 2),
                            size);
        Slide slide;
        slide.id = "image";
        slide.objects.append(image);
        fragment.slides.append(slide);
    } else if (data->hasText() && !data->text().isEmpty()) {
        fragment.size = m_document.size;
        Slide slide;
        slide.id = "text";
        fragment.slides.append(slide);
        const auto id = Edit::addText(
            fragment, 0, QPointF(m_document.size.width() / 2, m_document.size.height() / 2));
        auto *o = fragment.slides[0].find(id);
        o->text = data->text();
        o->textColor = m_document.theme.colors.value("foreground", o->textColor);
        o->fontFamily = m_document.theme.fonts.value("body", o->fontFamily);
    } else
        return;
    m_history.begin(m_document, tr("Paste objects"));
    const auto ids = ObjectCopy::insert(m_document, m_currentSlide, fragment, m_groupScope);
    if (ids.isEmpty()) {
        m_history.cancel(m_document);
        emit failed(tr("The clipboard contains no usable objects."));
        return;
    }
    m_history.commit();
    selectIds(ids);
    touch();
}
void Backend::duplicateSelected() {
    if (!hasSelection())
        return;
    const auto fragment =
        ObjectCopy::extract(m_document, m_currentSlide, selectedIds(), m_groupScope.size());
    m_history.begin(m_document, tr("Duplicate objects"));
    const auto ids = ObjectCopy::insert(m_document, m_currentSlide, fragment, m_groupScope);
    m_history.commit();
    selectIds(ids);
    touch();
}

int Backend::pasteEditorText(QQuickTextDocument *editor, int start, int end) {
    if (!editor || !editor->textDocument())
        return start;
    const auto text = QGuiApplication::clipboard()->text().replace("\r\n", "\n");
    QTextCursor cursor(editor->textDocument());
    const int limit = editor->textDocument()->characterCount() - 1;
    cursor.setPosition(qBound(0, start, limit));
    cursor.setPosition(qBound(0, end, limit), QTextCursor::KeepAnchor);
    cursor.insertText(text);
    return cursor.position();
}
