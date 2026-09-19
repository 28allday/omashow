#include "core/svgasset.h"
#include "core/imageasset.h"
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <QSvgRenderer>
#include <QXmlStreamReader>
#include <cmath>

bool SvgAsset::decode(SceneObject &object, const QByteArray &bytes,
                      QString *error) {
  const auto fail = [&](const QString &message) {
    if (error)
      *error = message;
    return false;
  };
  if (bytes.isEmpty() || bytes.size() > 8 * 1024 * 1024)
    return fail("SVG files must be smaller than 8 MB.");
  const QSet<QString> allowed = {
      "svg",     "g",        "defs",     "path",           "rect",
      "ellipse", "circle",   "polygon",  "polyline",       "line",
      "text",    "tspan",    "textPath", "linearGradient", "radialGradient",
      "stop",    "clipPath", "mask",     "pattern",        "use",
      "symbol",  "title",    "desc",     "style"};
  QXmlStreamReader xml(bytes);
  int depth = 0, nodes = 0;
  bool root = false;
  const auto safeCss = [](const QString &value) {
    if (value.contains('\\') || value.contains('@') ||
        value.contains("animation", Qt::CaseInsensitive))
      return false;
    static const QRegularExpression urls(
        "url\\s*\\(([^)]*)\\)", QRegularExpression::CaseInsensitiveOption);
    auto matches = urls.globalMatch(value);
    while (matches.hasNext()) {
      const auto reference =
          matches.next().captured(1).trimmed().remove('"').remove('\'');
      if (!reference.startsWith('#'))
        return false;
    }
    return true;
  };
  while (!xml.atEnd()) {
    const auto token = xml.readNext();
    if (token == QXmlStreamReader::DTD ||
        token == QXmlStreamReader::EntityReference)
      return fail("SVG document types and custom entities are unsupported.");
    if (token == QXmlStreamReader::ProcessingInstruction &&
        xml.processingInstructionTarget() != "xml")
      return fail("SVG processing instructions are unsupported.");
    if (token == QXmlStreamReader::StartElement) {
      const auto tag = xml.name().toString();
      if (!root) {
        if (tag != "svg")
          return fail("This file is not an SVG image.");
        root = true;
      }
      if (tag == "metadata") {
        xml.skipCurrentElement();
        continue;
      }
      if (!allowed.contains(tag))
        return fail(QString("Unsupported SVG element: %1. Use static, "
                            "self-contained vector content.")
                        .arg(tag));
      if (++depth > 128 || ++nodes > 10000)
        return fail("This SVG contains too many nested elements.");
      for (const auto &attribute : xml.attributes()) {
        const auto name = attribute.name().toString().toLower(),
                   value = attribute.value().toString();
        if (name.startsWith("on") || name == "base" ||
            (name == "href" && !value.trimmed().startsWith('#')) ||
            !safeCss(value))
          return fail("SVG external resources, scripts and animations are "
                      "unsupported.");
      }
      if (tag == "style") {
        if (!safeCss(xml.readElementText()))
          return fail("SVG styles must be static and self-contained.");
        --depth;
      }
    } else if (token == QXmlStreamReader::EndElement)
      --depth;
  }
  if (xml.hasError() || !root)
    return fail("The SVG XML could not be read.");
  QSvgRenderer renderer;
  renderer.setOptions(QtSvg::DisableAnimations);
  if (!renderer.load(bytes) || !renderer.isValid() || renderer.animated())
    return fail("The SVG could not be rendered as a static image.");
  const auto size = renderer.defaultSize();
  if (size.isEmpty() || size.width() > 100000 || size.height() > 100000)
    return fail("The SVG has invalid or excessive dimensions.");
  QPicture picture;
  QPainter painter(&picture);
  renderer.render(&painter, QRectF(QPointF(), size));
  painter.end();
  object.type = ObjectType::Image;
  object.imageFormat = "svg";
  object.image = {};
  object.vectorPicture = picture;
  object.vectorSize = size;
  object.imageData = bytes;
  object.imageId = ImageAsset::identity(bytes);
  return true;
}
