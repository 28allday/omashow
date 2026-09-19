#include "backend.h"
#include "core/design.h"
#include "core/edit.h"
#include "core/imageasset.h"
#include "core/imagecrop.h"
#include "filepicker.h"
void Backend::insertImageDialog() {
  m_pending = Pending::InsertImage;
  m_imageSlideId = m_document.slides.value(m_currentSlide).id;
  m_chooser->openFile(tr("Insert picture"), tr("Pictures"),
                      {"*.svg", "*.SVG", "*.png", "*.jpg", "*.jpeg", "*.webp",
                       "*.PNG", "*.JPG", "*.JPEG", "*.WEBP"});
}
void Backend::replaceImageDialog() {
  const auto *o = selectedObject();
  if (!o || o->type != ObjectType::Image)
    return;
  m_imageTargetId = o->id;
  m_imageSlideId = m_document.slides.value(m_currentSlide).id;
  m_pending = Pending::ReplaceImage;
  m_chooser->openFile(tr("Replace picture"), tr("Pictures"),
                      {"*.svg", "*.SVG", "*.png", "*.jpg", "*.jpeg", "*.webp",
                       "*.PNG", "*.JPG", "*.JPEG", "*.WEBP"});
}
bool Backend::replaceImage(const QUrl &url) {
  return loadImage(url, true, m_currentSlide, selectedId());
}
bool Backend::insertImage(const QUrl &url) {
  return loadImage(url, false, m_currentSlide, QString());
}
bool Backend::loadImage(const QUrl &url, bool replace, int index,
                        const QString &target) {
  if (!url.isLocalFile() || index < 0 || index >= m_document.slides.size()) {
    emit failed(tr("Choose a local image for the current deck."));
    return false;
  }
  SceneObject image;
  QString error;
  if (!ImageAsset::fromFile(image, url.toLocalFile(), &error)) {
    emit failed(error);
    return false;
  }
  return applyImage(image, replace, index, target, m_groupScope);
}

bool Backend::applyImage(SceneObject image, bool replace, int index, const QString &target, const QStringList &groups) {
  if (replace) {
    const auto *existing = m_document.slides.at(index).find(target);
    if (!existing || (existing->type != ObjectType::Image &&
                      existing->type != ObjectType::Rect)) {
      emit failed(tr("The picture to replace is no longer available."));
      return false;
    }
    m_history.begin(m_document, tr("Replace picture"));
    auto *o = m_document.slides[index].find(target);
    o->imageOriginal.reset();
    ImageAsset::copyData(*o, image);
    if (o->type == ObjectType::Rect) {
      o->fillStyle = 4;
      Design::markOverride(*o, "fillStyle");
    }
    for (const auto &key : {"imageId", "cropX", "cropY", "cropW", "cropH"})
      Design::markOverride(*o, key);
  } else {
    image.id = Edit::newId("image");
    image.groups = groups;
    QSizeF size = ImageAsset::size(image);
    size.scale(m_document.size * .7, Qt::KeepAspectRatio);
    image.rect = QRectF(QPointF((m_document.size.width() - size.width()) / 2,
                                (m_document.size.height() - size.height()) / 2),
                        size);
    m_history.begin(m_document, tr("Insert picture"));
    m_document.slides[index].objects.append(image);
  }
  m_history.commit();
  if (index == m_currentSlide)
    select(replace ? target : image.id);
  touch();
  return true;
}

void Backend::resetImageCrop() {
  const auto ids = selectedIds();
  if (ids.isEmpty())
    return;
  m_history.begin(m_document, tr("Reset picture crop"));
  for (const auto &id : ids)
    if (auto *o = m_document.slides[m_currentSlide].find(id))
      if (o->type == ObjectType::Image) {
        o->imageCrop = QRectF(0, 0, 1, 1);
        for (const auto &key : {"cropX", "cropY", "cropW", "cropH"})
          Design::markOverride(*o, key);
      }
  m_history.commit();
  touch();
}

void Backend::resetImageAdjustments() {
  beginEdit(tr("Reset picture adjustments"));
  setSelectedProperty("imageBrightness", 0);
  setSelectedProperty("imageContrast", 1);
  setSelectedProperty("imageSaturation", 1);
  setSelectedProperty("imageTintAmount", 0);
  endEdit();
}

void Backend::resizeImageCrop(int handle,qreal x,qreal y,bool lockAspect) {
  if(!m_gestureActive || selectionCount()!=1) return;
  const auto *basis=m_gestureBasis.find(selectedId()); if(!basis || basis->type!=ObjectType::Image) return;
  auto result=*basis; if(!ImageCrop::resize(result,*basis,handle,QPointF(x,y),lockAspect)) return;
  auto *o=m_document.slides[m_currentSlide].find(selectedId()); if(!o) return;
  o->rect=result.rect; o->imageCrop=result.imageCrop;
  for(const auto &key:{"x","y","w","h","cropX","cropY","cropW","cropH"}) Design::markOverride(*o,key);
  touch();
}
