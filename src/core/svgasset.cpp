#include "core/svgasset.h"
#include "core/imageasset.h"
#include <QPainter>
#include <QRegularExpression>
#include <QHash>
#include <QSet>
#include <QSvgRenderer>
#include <QXmlStreamReader>
#include <cmath>
#include <functional>

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
  // <use> and url(#…) copy other elements, and a copy can hold copies: a few
  // dozen lines can ask the renderer for billions of shapes. Record every
  // reference, and the references inside each element that has an id, so the
  // expanded size can be counted before anything is drawn.
  QStringList openIds, references;
  QHash<QString, QStringList> inside;
  // How many elements each id'd element holds: a copy of it draws them all.
  QHash<QString, qint64> held;
  const auto refer = [&](const QString &target) {
    if (target.isEmpty()) return;
    references.append(target);
    for (const auto &id : openIds)
      if (!id.isEmpty()) inside[id].append(target);
  };
  const auto referencesIn = [&](const QString &value) {
    static const QRegularExpression local(
        "url\\s*\\(\\s*['\"]?#([^)'\"\\s]*)", QRegularExpression::CaseInsensitiveOption);
    auto matches = local.globalMatch(value);
    while (matches.hasNext()) refer(matches.next().captured(1));
  };
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
      for (const auto &id : openIds)
        if (!id.isEmpty()) ++held[id];
      openIds.append(xml.attributes().value("id").toString());
      for (const auto &attribute : xml.attributes()) {
        const auto name = attribute.name().toString().toLower(),
                   value = attribute.value().toString();
        if (name.startsWith("on") || name == "base" ||
            (name == "href" && !value.trimmed().startsWith('#')) ||
            !safeCss(value))
          return fail("SVG external resources, scripts and animations are "
                      "unsupported.");
        if (name == "href")
          refer(value.trimmed().mid(1));
        referencesIn(value);
      }
      if (tag == "style") {
        const auto css = xml.readElementText();
        if (!safeCss(css))
          return fail("SVG styles must be static and self-contained.");
        referencesIn(css);
        --depth;
        openIds.removeLast();
      }
    } else if (token == QXmlStreamReader::EndElement) {
      --depth;
      if (!openIds.isEmpty()) openIds.removeLast();
    }
  }
  if (xml.hasError() || !root)
    return fail("The SVG XML could not be read.");
  {
    // Everything a reference draws, counted: the element, all it holds, and
    // whatever its own references draw in turn. Loops and chains deeper than
    // any real drawing needs are refused outright.
    constexpr qint64 limit = 100000;
    constexpr int deepest = 256;
    QHash<QString, qint64> copies;
    QSet<QString> visiting;
    bool refused = false;
    std::function<qint64(const QString &, int)> expand = [&](const QString &id, int level) -> qint64 {
      if (const auto known = copies.constFind(id); known != copies.constEnd())
        return *known;
      if (level > deepest || visiting.contains(id)) { refused = true; return limit + 1; }
      visiting.insert(id);
      qint64 total = 1 + held.value(id);
      for (const auto &target : inside.value(id)) {
        if (total > limit) break;
        total += expand(target, level + 1);
        if (refused) break;
      }
      visiting.remove(id);
      total = qMin(total, limit + 1);
      copies.insert(id, total);
      return total;
    };
    qint64 total = 0;
    for (const auto &target : references) {
      total += expand(target, 0);
      if (refused || total > limit)
        return fail("This SVG repeats its own parts too many times to draw.");
    }
  }
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
