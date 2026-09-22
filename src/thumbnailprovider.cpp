#include "core/design.h"
#include "core/shape.h"
#include "core/starter.h"
#include "core/templates.h"
#include "core/deckresize.h"
#include "thumbnailprovider.h"

#include "anim/evaluator.h"
#include "backend.h"
#include "core/scene.h"
#include "render/scenerenderer.h"
#include <QMutexLocker>
#include <QUrl>
#include <QDeadlineTimer>

SlideThumbnailProvider::SlideThumbnailProvider(Backend *backend)
    : QQuickImageProvider(QQuickImageProvider::Image, ForceAsynchronousImageLoading) {
    // Capture implicitly shared values only on the GUI thread. The provider's
    // low-priority image thread never calls Backend or reads its live document.
    const auto refresh = [this, backend] { capture(backend); };
    QObject::connect(backend, &Backend::documentChanged, &m_observer, refresh);
    QObject::connect(backend, &Backend::selectionChanged, &m_observer, refresh);
    QObject::connect(backend, &Backend::currentSlideChanged, &m_observer, refresh);
    QObject::connect(backend, &Backend::layoutPreviewChanged, &m_observer, refresh);
    QObject::connect(backend, &Backend::deckImportChanged, &m_observer, refresh);
    QObject::connect(backend, &Backend::diagramPreviewChanged, &m_observer, refresh);
    QObject::connect(backend, &Backend::mediaOptimisationChanged, &m_observer, refresh);
    capture(backend);
}

void SlideThumbnailProvider::capture(Backend *backend) {
    Snapshot next;
    next.document = backend->document();
    next.layoutDocument = backend->layoutPreviewDocument();
    next.layoutOk = backend->layoutPreview().value("ok").toBool();
    next.layoutRevision = backend->layoutPreview().value("revision").toInt();
    next.importDocument = backend->importPreviewDocument();
    next.importSource = backend->importSourceDocument();
    next.importOk = backend->deckImport().value("ok").toBool();
    next.importRevision = backend->deckImport().value("revision").toInt();
    next.diagram = backend->diagramObjects();
    next.current = backend->currentSlide();
    next.revision = backend->revision();
    next.selected = backend->selectedIds();
    next.before = backend->m_mediaPreviewSource;
    next.after = backend->m_mediaPreview;
    QMutexLocker lock(&m_mutex);
    m_snapshot = std::move(next);
    m_captured.wakeAll();
}

SlideThumbnailProvider::Snapshot SlideThumbnailProvider::snapshotFor(int layoutRevision,
                                                                    int importRevision,
                                                                    int documentRevision) {
    QMutexLocker lock(&m_mutex);
    QDeadlineTimer deadline(250);
    while ((layoutRevision > 0 && m_snapshot.layoutRevision != layoutRevision) ||
           (importRevision > 0 && m_snapshot.importRevision != importRevision) ||
           (documentRevision > 0 && m_snapshot.revision < documentRevision)) {
        if (!m_captured.wait(&m_mutex, deadline)) break;
    }
    return m_snapshot;
}

QImage SlideThumbnailProvider::requestImage(const QString &id, QSize *size,
                                            const QSize &requestedSize) {
    const auto blank = [&] {
        const int width = qBound(1, requestedSize.width() > 0 ? requestedSize.width() : 320, 2048);
        QImage image(width, qMax(1, width * 9 / 16), QImage::Format_RGBA8888);
        image.fill(Qt::transparent);
        if (size) *size = image.size();
        return image;
    };
    const Snapshot snapshot = snapshotFor(
        id.startsWith("layout-apply/") ? id.section('/', 2, 2).toInt() : 0,
        id.startsWith("import/") || id.startsWith("import-source/") ? id.section('/', 2, 2).toInt() : 0,
        !id.isEmpty() && id.at(0).isDigit() ? id.section('/', 1, 1).toInt() : 0);
    if(id.startsWith("media-comparison/")) {
        const auto &object = id.section('/',1,1)=="after" ? snapshot.after : snapshot.before;
        auto image = object.type == ObjectType::Media ? MediaAsset::frameAt(object, id.section('/',2,2).toDouble()) : object.image;
        if (image.isNull()) return blank();
        if(requestedSize.width()>0) image=image.scaledToWidth(requestedSize.width(),Qt::SmoothTransformation);
        if(size) *size=image.size();
        return image;
    }
    if(id.startsWith("diagram/")) {
        const auto docSize=snapshot.document.size;
        const int width=requestedSize.width()>0?requestedSize.width():800;
        const QSize pixels(width,qMax(1,qRound(width*docSize.height()/docSize.width()))); if(size)*size=pixels;
        return SceneRenderer::render(snapshot.diagram,docSize,pixels,snapshot.document.theme.colors.value("background"));
    }
    if (id.startsWith("layout-apply/")) {
        const auto &document = snapshot.layoutDocument;
        const int index = id.section('/', 1, 1).toInt();
        if (!snapshot.layoutOk || index < 0 || index >= document.slides.size()) return blank();
        const auto slide = Design::resolve(document, index);
        const int width = requestedSize.width() > 0 ? requestedSize.width() : 640;
        const QSize pixels(width, qRound(width * document.size.height() / document.size.width()));
        if (size) *size = pixels;
        return SceneRenderer::render(slide.objects, document.size, pixels, slide.background);
    }
    // "import/<index>" is the slide as it would arrive; "import-source/<index>"
    // is the slide as it stands in the deck being imported from.
    if (id.startsWith("import/") || id.startsWith("import-source/")) {
        const bool arriving = id.startsWith("import/");
        const auto &document = arriving ? snapshot.importDocument : snapshot.importSource;
        const int index = id.section('/', 1, 1).toInt();
        if ((arriving && !snapshot.importOk) || index < 0 || index >= document.slides.size()) return blank();
        const auto slide = Design::resolve(document, index);
        const int width = requestedSize.width() > 0 ? requestedSize.width() : 640;
        const QSize pixels(width, qMax(1, qRound(width * document.size.height() / document.size.width())));
        if (size) *size = pixels;
        return SceneRenderer::render(slide.objects, document.size, pixels, slide.background);
    }
    // "template/<id>" draws a template's opening slide. The id is a file path
    // or a built-in name, and the deck is read here rather than through Backend
    // — this thread must never touch the live document.
    if (id.startsWith("template/")) {
        Document document;
        QString error;
        // Everything after the first slash is the id, so a template path with
        // slashes in it survives whether or not the URL arrived decoded.
        const auto templateId = QUrl::fromPercentEncoding(
            id.mid(QStringLiteral("template/").size()).toUtf8());
        if (!Templates::open(templateId, &document, &error) ||
            document.slides.isEmpty())
            return blank();
        const auto slide = Design::resolve(document, 0);
        const int width = requestedSize.width() > 0 ? requestedSize.width() : 480;
        const QSize pixels(width, qMax(1, qRound(width * document.size.height() / document.size.width())));
        if (size) *size = pixels;
        return SceneRenderer::render(slide.objects, document.size, pixels, slide.background);
    }
    if(id.startsWith("table/")) {
        auto slide=Design::resolve(snapshot.document,id.section('/',1,1).toInt());
        const auto *found=slide.find(id.section('/',2,2)); if(!found || (found->type!=ObjectType::Table && found->type!=ObjectType::Chart)) return blank();
        auto o=*found; o.rotation=0; o.rect.moveTopLeft(QPointF(0,0));
        const int width=requestedSize.width()>0?requestedSize.width():800;
        const QSize pixels(width,qMax(1,qRound(width*o.rect.height()/o.rect.width()))); if(size) *size=pixels;
        return SceneRenderer::render({o},o.rect.size(),pixels,Qt::transparent);
    }
    if(id.startsWith("shape/")) {
        SceneObject o; o.shapeKind=id.section('/',1,1).toInt(); o.rect=QRectF(12,12,76,66); o.fill=QColor("#57c8eb"); o.strokeWidth=2; o.strokeColor=QColor("#d5eaf2");
        if(o.shapeKind==13 || o.shapeKind==22) o.fillStyle=5;
        const QSize pixels(160,144); if(size) *size=pixels;
        return SceneRenderer::render({o},QSizeF(100,90),pixels,Qt::transparent);
    }
    if(id.startsWith("combine/")) {
        const int operation=id.section('/',1,1).toInt();
        auto slide=Design::resolve(snapshot.document,snapshot.current); const auto ids=snapshot.selected;
        QVector<SceneObject> selected;
        for (const auto &o : slide.objects) if (ids.contains(o.id)) selected.append(o);
        for(int i=slide.objects.size()-1;i>=0;--i) if(ids.contains(slide.objects[i].id)) slide.objects.removeAt(i);
        slide.objects+=Shape::combine(selected, operation);
        const auto docSize=snapshot.document.size; const int width=requestedSize.width()>0?requestedSize.width():640;
        const QSize pixels(width,qRound(width*docSize.height()/docSize.width())); if(size) *size=pixels;
        return SceneRenderer::render(slide.objects,docSize,pixels,slide.background);
    }
    if(id.startsWith("resize/")) {
        const auto parts=id.split('/');
        auto preview=snapshot.document;
        DeckResize::apply(preview,QSizeF(parts.value(1).toDouble(),parts.value(2).toDouble()),parts.value(3).toInt()==0);
        const int index=parts.value(4).toInt();
        if(index<0 || index>=preview.slides.size()) return blank();
        const auto slide=Design::resolve(preview,index);
        const int width=requestedSize.width()>0?requestedSize.width():640;
        const QSize pixels(width,qRound(width*preview.size.height()/preview.size.width()));
        if(size) *size=pixels;
        return SceneRenderer::render(slide.objects,preview.size,pixels,slide.background);
    }
    if (id.startsWith("start/")) {
        const auto parts = id.split('/');
        const Document preview = Starter::create(parts.value(1).toInt(),
            QSizeF(parts.value(2).toDouble(), parts.value(3).toDouble()), parts.value(4).toInt());
        if (preview.slides.isEmpty()) return blank();
        const Slide slide = Design::resolve(preview, 0);
        const int w = requestedSize.width() > 0 ? requestedSize.width() : 640;
        const QSize pixels(w, qRound(w * preview.size.height() / preview.size.width()));
        if (size) *size = pixels;
        return SceneRenderer::render(slide.objects, preview.size, pixels, slide.background);
    }
    if (id.startsWith("layout/")) {
        Document preview = snapshot.document;
        Design::ensureDefaults(preview);
        const QString layoutId = id.section('/', 1, 1);
        const int preset = id.section('/', 3, 3).toInt();
        if (preset >= 0 && preset <= 2) preview.theme = Design::preset(preset);
        preview.slides.clear();
        Slide slide; slide.id = "preview"; preview.slides.append(slide);
        const QString chosen = Design::layout(preview, layoutId) ? layoutId : preview.layouts.first().id;
        Design::applyLayout(preview, 0, chosen);
        const Slide resolved = Design::resolve(preview, 0);
        const int w = requestedSize.width() > 0 ? requestedSize.width() : 960;
        const QSize pixels(w, qRound(w * preview.size.height() / preview.size.width()));
        if (size) *size = pixels;
        return SceneRenderer::render(resolved.objects, preview.size, pixels, resolved.background);
    }
    const int index = id.section(QLatin1Char('/'), 0, 0).toInt();
    const Document &document = snapshot.document;

    const int width = requestedSize.width() > 0 ? requestedSize.width() : 320;
    const int height = qRound(width * document.size.height() / document.size.width());
    if (size)
        *size = QSize(width, height);

    if (index < 0 || index >= document.slides.size()) {
        QImage blank(width, height, QImage::Format_RGBA8888);
        blank.fill(Qt::transparent);
        return blank;
    }

    // Settled, not mid-build: a navigator full of half-faded objects tells you
    // nothing about what the slide is.
    const Slide slide = Design::resolve(document, index);
    return SceneRenderer::render(slide.objects,
                                 document.size, QSize(width, height), slide.background);
}
