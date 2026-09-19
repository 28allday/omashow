#include "core/objectstyle.h"
#include "core/design.h"
#include "core/imageasset.h"
QStringList ObjectStyles::keys() {
  return {"fill",          "fillToken",     "fillStyle",        "fillSecondary",
          "fillAngle",     "patternStyle",  "opacity",          "cornerRadius",
          "strokeColor",   "strokeWidth",   "strokeStyle",      "strokeJoin",
          "strokeCap",     "shadowEnabled", "shadowColor",      "shadowX",
          "shadowY",       "fontSize",      "fontWeight",       "fontFamily",
          "fontToken",     "textColor",     "textColorToken",   "uppercase",
          "letterSpacing", "italic",        "underline",        "textAlign",
          "verticalAlign", "lineHeight",    "paragraphSpacing", "textIndent",
          "listStyle",     "listStart",     "textFit"};
}
SceneObject ObjectStyles::capture(const SceneObject &object) {
  SceneObject appearance;
  const auto properties = Design::properties(object);
  for (const auto &key : keys())
    Design::setProperty(appearance, key, properties.value(key), false);
  if (object.fillStyle == 4) {
    ImageAsset::copyData(appearance, object);
    appearance.imageMode = object.imageMode;
    appearance.imageCrop = object.imageCrop;
  }
  return appearance;
}
void ObjectStyles::apply(SceneObject &object, const SceneObject &appearance) {
  const auto properties = Design::properties(appearance);
  for (const auto &key : keys()) {
    Design::setProperty(object, key, properties.value(key), false);
    Design::markOverride(object, key);
  }
  if (appearance.fillStyle == 4 && object.type == ObjectType::Rect) {
    ImageAsset::copyData(object, appearance);
    object.imageMode = appearance.imageMode;
    object.imageCrop = appearance.imageCrop;
    for (const auto &key :
         {"imageId", "imageMode", "cropX", "cropY", "cropW", "cropH"})
      Design::markOverride(object, key);
  }
}
