#include "core/design.h"
#include "core/shape.h"
#include "core/starter.h"
#include "core/deckresize.h"
#include "thumbnailprovider.h"

#include "anim/evaluator.h"
#include "backend.h"
#include "core/scene.h"
#include "render/scenerenderer.h"

SlideThumbnailProvider::SlideThumbnailProvider(Backend *backend)
    : QQuickImageProvider(QQuickImageProvider::Image), m_backend(backend) {}

QImage SlideThumbnailProvider::requestImage(const QString &id, QSize *size,
                                            const QSize &requestedSize) {
    if(id.startsWith("media-comparison/")) {
        auto image=m_backend->mediaComparisonFrame(id.section('/',1,1)=="after",id.section('/',2,2).toDouble());
        if(!image.isNull() && requestedSize.width()>0) image=image.scaledToWidth(requestedSize.width(),Qt::SmoothTransformation);
        if(size) *size=image.size();
        return image;
    }
    if(id.startsWith("diagram/")) {
        const auto docSize=m_backend->slideSize();
        const int width=requestedSize.width()>0?requestedSize.width():800;
        const QSize pixels(width,qMax(1,qRound(width*docSize.height()/docSize.width()))); if(size)*size=pixels;
        return SceneRenderer::render(m_backend->diagramObjects(),docSize,pixels,m_backend->document().theme.colors.value("background"));
    }
    if(id.startsWith("table/")) {
        auto slide=Design::resolve(m_backend->document(),id.section('/',1,1).toInt());
        const auto *found=slide.find(id.section('/',2,2)); if(!found || (found->type!=ObjectType::Table && found->type!=ObjectType::Chart)) return {};
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
        auto slide=Design::resolve(m_backend->document(),m_backend->currentSlide()); const auto ids=m_backend->selectedIds();
        for(int i=slide.objects.size()-1;i>=0;--i) if(ids.contains(slide.objects[i].id)) slide.objects.removeAt(i);
        slide.objects+=m_backend->combinedShapes(operation);
        const auto docSize=m_backend->slideSize(); const int width=requestedSize.width()>0?requestedSize.width():640;
        const QSize pixels(width,qRound(width*docSize.height()/docSize.width())); if(size) *size=pixels;
        return SceneRenderer::render(slide.objects,docSize,pixels,slide.background);
    }
    if(id.startsWith("resize/")) {
        const auto parts=id.split('/');
        auto preview=m_backend->document();
        DeckResize::apply(preview,QSizeF(parts.value(1).toDouble(),parts.value(2).toDouble()),parts.value(3).toInt()==0);
        const int index=parts.value(4).toInt();
        if(index<0 || index>=preview.slides.size()) return {};
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
        if (preview.slides.isEmpty()) return {};
        const Slide slide = Design::resolve(preview, 0);
        const int w = requestedSize.width() > 0 ? requestedSize.width() : 640;
        const QSize pixels(w, qRound(w * preview.size.height() / preview.size.width()));
        if (size) *size = pixels;
        return SceneRenderer::render(slide.objects, preview.size, pixels, slide.background);
    }
    if (id.startsWith("layout/")) {
        Document preview = m_backend->document();
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
    const Document &document = m_backend->document();

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
