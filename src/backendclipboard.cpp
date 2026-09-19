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
namespace {
const char *mimeType = "application/x-omashow-objects";
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
