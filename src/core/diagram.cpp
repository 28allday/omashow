#include "core/diagram.h"
#include "core/connector.h"
#include "core/design.h"
#include "core/edit.h"
#include "core/shape.h"
#include "render/textlayout.h"
#include <cmath>
#include <functional>
namespace {
struct Node {
  QString text;
  int parent = -1, depth = 0;
  QVector<int> children;
  qreal span = 1, centre = 0;
};
} // namespace
Diagram::Result Diagram::build(const QSizeF &size, const DeckTheme &theme,
                               int kind, const QString &outline,
                               bool vertical) {
  Result result;
  if (kind < 0 || kind > 1 || !std::isfinite(size.width()) ||
      !std::isfinite(size.height()) || size.width() < 160 ||
      size.height() < 90) {
    result.error = "Choose a process or hierarchy on a valid slide.";
    return result;
  }
  if (outline.size() > 8192 || outline.contains(QChar(0))) {
    result.error = "Use an outline up to 8,192 characters.";
    return result;
  }
  QVector<Node> nodes;
  QVector<int> parents;
  QVector<int> roots;
  const auto lines = outline.split('\n');
  for (int line = 0; line < lines.size(); ++line) {
    const auto raw = lines[line];
    if (raw.trimmed().isEmpty())
      continue;
    int spaces = 0, tabs = 0, pos = 0;
    while (pos < raw.size() && (raw[pos] == ' ' || raw[pos] == '\t')) {
      if (raw[pos] == '\t')
        ++tabs;
      else
        ++spaces;
      ++pos;
    }
    if (kind == 1 && ((spaces && tabs) || spaces % 2)) {
      result.error =
          QString("Line %1: indent with two spaces or one tab per level.")
              .arg(line + 1);
      return result;
    }
    const int depth = kind == 1 ? spaces / 2 + tabs : 0;
    if (depth > 5 || depth > parents.size()) {
      result.error =
          QString("Line %1: indentation skips a parent or exceeds six levels.")
              .arg(line + 1);
      return result;
    }
    const auto text = raw.mid(pos).trimmed();
    if (text.size() > 240) {
      result.error = QString("Line %1: shorten the label to 240 characters.")
                         .arg(line + 1);
      return result;
    }
    Node node;
    node.text = text;
    node.depth = depth;
    if (depth)
      node.parent = parents[depth - 1];
    const int index = nodes.size();
    nodes.append(node);
    if (node.parent >= 0)
      nodes[node.parent].children.append(index);
    else
      roots.append(index);
    parents.resize(depth + 1);
    parents[depth] = index;
    result.levels = qMax(result.levels, depth + 1);
    if (nodes.size() > (kind == 0 ? 12 : 40)) {
      result.error = kind == 0 ? "A process supports up to 12 steps. Split "
                                 "longer processes across slides."
                               : "A hierarchy supports up to 40 nodes. Split "
                                 "larger trees across slides.";
      return result;
    }
  }
  result.nodes = nodes.size();
  if (nodes.isEmpty()) {
    result.error = "Enter one label per line.";
    return result;
  }
  const qreal w = size.width(), h = size.height();
  const QRectF area(w * .08, h * .12, w * .84, h * .76);
  QVector<QRectF> boxes(nodes.size());
  if (kind == 0) {
    const qreal gap = (vertical ? h : w) * .025;
    const qreal along = vertical ? area.height() : area.width();
    const qreal length =
        qMin(vertical ? h * .17 : w * .24,
             (along - gap * (nodes.size() - 1)) / nodes.size());
    const qreal across = vertical ? w * .42 : h * .21;
    const qreal offset =
        (along - nodes.size() * length - (nodes.size() - 1) * gap) / 2;
    for (int i = 0; i < nodes.size(); ++i)
      boxes[i] = vertical ? QRectF((w - across) / 2,
                                   area.top() + offset + i * (length + gap),
                                   across, length)
                          : QRectF(area.left() + offset + i * (length + gap),
                                   (h - across) / 2, length, across);
    result.levels = 1;
  } else {
    std::function<qreal(int)> count = [&](int index) {
      auto &node = nodes[index];
      qreal span = 0;
      for (int child : node.children)
        span += count(child);
      return node.span = qMax(1.0, span);
    };
    qreal leaves = 0;
    for (int root : roots)
      leaves += count(root);
    std::function<void(int, qreal)> position = [&](int index, qreal start) {
      auto &node = nodes[index];
      node.centre = start + node.span / 2;
      for (int child : node.children) {
        position(child, start);
        start += nodes[child].span;
      }
    };
    qreal start = 0;
    for (int root : roots) {
      position(root, start);
      start += nodes[root].span;
    }
    const qreal branchStep = (vertical ? area.width() : area.height()) / leaves;
    const qreal levelStep =
        (vertical ? area.height() : area.width()) / result.levels;
    const qreal boxW =
        qMin(w * .26, vertical ? branchStep * .82 : levelStep * .7);
    const qreal boxH =
        qMin(h * .17, vertical ? levelStep * .6 : branchStep * .74);
    for (int i = 0; i < nodes.size(); ++i) {
      const auto &node = nodes[i];
      const QPointF centre =
          vertical ? QPointF(area.left() + node.centre * branchStep,
                             area.top() + (node.depth + .5) * levelStep)
                   : QPointF(area.left() + (node.depth + .5) * levelStep,
                             area.top() + node.centre * branchStep);
      boxes[i] =
          QRectF(centre - QPointF(boxW / 2, boxH / 2), QSizeF(boxW, boxH));
    }
  }
  const auto group = Edit::newId("diagram");
  QVector<SceneObject> shapes, texts, links;
  for (int i = 0; i < nodes.size(); ++i) {
    const auto nodeGroup = Edit::newId("node");
    SceneObject box;
    box.id = Edit::newId("shape");
    box.rect = boxes[i];
    box.groups = {group, nodeGroup};
    box.fillToken = "background";
    box.fill = theme.colors.value("background");
    box.cornerRadius = h * .014;
    box.strokeWidth = h * .003;
    box.strokeColor =
        theme.colors.value(nodes[i].depth == 0 ? "accent" : "muted");
    SceneObject label;
    label.id = Edit::newId("text");
    label.type = ObjectType::Text;
    label.groups = box.groups;
    label.text = nodes[i].text;
    label.rect = box.rect.adjusted(h * .016, h * .012, -h * .016, -h * .012);
    label.fontToken = "body";
    label.fontFamily = theme.fonts.value("body");
    label.fontSize = h * .034;
    label.textColorToken = "foreground";
    label.textColor = theme.colors.value("foreground");
    label.fontWeight = nodes[i].depth == 0 ? 600 : 400;
    label.textAlign = 1;
    label.verticalAlign = 1;
    label.textFit = 1;
    const auto metrics = TextLayout::measure(label);
    if (label.rect.width() <= 0 || label.rect.height() <= 0 ||
        metrics.overflow || metrics.effectiveSize < h * .018) {
      result.error = QString("“%1” cannot fit readably. Shorten labels, change "
                             "direction or use fewer nodes.")
                         .arg(label.text.left(45));
      return result;
    }
    // Store the fitted font size, so editing and resizing use the visible size.
    label.fontSize = metrics.effectiveSize;
    if (metrics.effectiveSize < h * .026 && result.warnings.isEmpty())
      result.warnings.append(
          "Some labels are small. Check the slide at presentation size or "
          "reduce the number of nodes.");
    shapes.append(box);
    texts.append(label);
  }
  for (int i = 0; i < nodes.size(); ++i) {
    const int parent = kind == 0 ? i - 1 : nodes[i].parent;
    if (parent < 0)
      continue;
    SceneObject line;
    line.id = Edit::newId("connector");
    line.groups = {group};
    line.connector = true;
    line.connectorFrom = shapes[parent].id;
    line.connectorTo = shapes[i].id;
    line.connectorStart = shapes[parent].rect.center();
    line.connectorEnd = shapes[i].rect.center();
    line.connectorFromSide = vertical ? 3 : 2;
    line.connectorToSide = vertical ? 1 : 4;
    line.connectorRoute = kind == 0 ? 0 : 1;
    line.connectorArrowEnd = kind == 0;
    line.fillStyle = 5;
    line.shapeKind = Shape::custom;
    line.strokeWidth = h * .003;
    line.strokeColor = theme.colors.value("muted");
    links.append(line);
  }
  result.objects = links;
  for (int i = 0; i < shapes.size(); ++i) {
    result.objects.append(shapes[i]);
    result.objects.append(texts[i]);
  }
  Connector::resolve(result.objects);
  return result;
}
