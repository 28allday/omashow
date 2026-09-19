#include "core/chart.h"
#include "core/table.h"
#include <QFontMetricsF>
#include <QLocale>
#include <QPainter>
#include <QSet>
#include <cmath>
#include <numeric>
#include <optional>
namespace {
qreal luminance(QColor c) {
  auto f = [](qreal x) {
    return x <= .04045 ? x / 12.92 : std::pow((x + .055) / 1.055, 2.4);
  };
  return .2126 * f(c.redF()) + .7152 * f(c.greenF()) + .0722 * f(c.blueF());
}
QColor contrastColor(QColor color, QColor background, qreal ratio) {
  const qreal back = luminance(background);
  const bool brighten = back < .18;
  for (int i = 0; i < 100; ++i) {
    const qreal v = luminance(color);
    if ((qMax(v, back) + .05) / (qMin(v, back) + .05) >= ratio)
      break;
    const qreal target = brighten ? 1 : 0;
    color = QColor::fromRgbF(color.redF() * .95 + target * .05,
                             color.greenF() * .95 + target * .05,
                             color.blueF() * .95 + target * .05);
  }
  return color;
}
bool finiteNumber(const QVariant &v, qreal &out) {
  bool ok = false;
  out = v.toDouble(&ok);
  return ok && std::isfinite(out) && std::abs(out) <= 1e15;
}
qreal tickStep(qreal span) {
  const qreal raw = qMax(1e-15, span / 5),
              base = std::pow(10, std::floor(std::log10(raw))), n = raw / base;
  return (n <= 1 ? 1 : n <= 2 ? 2 : n <= 5 ? 5 : 10) * base;
}
std::optional<qreal> parse(const QString &value, const QLocale &locale,
                           bool &invalid) {
  const auto text = value.trimmed();
  if (text.isEmpty())
    return {};
  bool ok = false;
  const qreal n = locale.toDouble(text, &ok);
  if (!ok || !std::isfinite(n) || std::abs(n) > 1e15) {
    invalid = true;
    return {};
  }
  return n;
}
} // namespace
QStringList Chart::names() {
  return {"Column", "Bar",    "Stacked column", "Stacked bar",
          "Line",   "Area",   "Stacked area",   "Pie",
          "Donut",  "Scatter"};
}
QVariantMap Chart::encode(const ChartData &c) {
  QVariantMap colors;
  for (auto it = c.seriesColors.cbegin(); it != c.seriesColors.cend(); ++it)
    colors[it.key()] = it.value().name(QColor::HexArgb);
  return {{"kind", c.kind},
          {"title", c.title},
          {"xTitle", c.xTitle},
          {"yTitle", c.yTitle},
          {"locale", c.locale},
          {"legend", c.legend},
          {"grid", c.grid},
          {"labels", c.labels},
          {"includeZero", c.includeZero},
          {"logarithmic", c.logarithmic},
          {"manualY", c.manualY},
          {"manualX", c.manualX},
          {"minimumY", c.minimumY},
          {"maximumY", c.maximumY},
          {"minimumX", c.minimumX},
          {"maximumX", c.maximumX},
          {"numberFormat", c.numberFormat},
          {"decimals", c.decimals},
          {"palette", c.palette},
          {"currency", c.currency},
          {"seriesColors", colors}};
}
bool Chart::decode(const QVariant &value, ChartData &out) {
  if (value.metaType().id() != QMetaType::QVariantMap)
    return false;
  const auto m = value.toMap();
  ChartData c;
  const auto known = encode(c);
  if (m.size() != known.size())
    return false;
  for (auto it = m.cbegin(); it != m.cend(); ++it)
    if (!known.contains(it.key()))
      return false;
  for (auto key : {"title", "xTitle", "yTitle", "locale", "currency"})
    if (m.value(key).metaType().id() != QMetaType::QString ||
        m.value(key).toString().size() > 1024)
      return false;
  c.title = m["title"].toString();
  c.xTitle = m["xTitle"].toString();
  c.yTitle = m["yTitle"].toString();
  c.locale = m["locale"].toString();
  c.currency = m["currency"].toString();
  if (c.locale != "en_GB" && c.locale != "de_DE" && c.locale != "fr_FR")
    return false;
  for (auto key : {"legend", "grid", "labels", "includeZero", "logarithmic",
                   "manualY", "manualX"})
    if (m.value(key).metaType().id() != QMetaType::Bool)
      return false;
  c.legend = m["legend"].toBool();
  c.grid = m["grid"].toBool();
  c.labels = m["labels"].toBool();
  c.includeZero = m["includeZero"].toBool();
  c.logarithmic = m["logarithmic"].toBool();
  c.manualY = m["manualY"].toBool();
  c.manualX = m["manualX"].toBool();
  for (auto key : {"kind", "numberFormat", "decimals", "palette"}) {
    qreal n;
    if (!finiteNumber(m[key], n) || n != int(n) || n < 0)
      return false;
  }
  c.kind = m["kind"].toInt();
  c.numberFormat = m["numberFormat"].toInt();
  c.decimals = m["decimals"].toInt();
  c.palette = m["palette"].toInt();
  if (c.kind >= names().size() || c.numberFormat > 3 || c.decimals > 6 ||
      c.palette > 2)
    return false;
  if (!finiteNumber(m["minimumY"], c.minimumY) ||
      !finiteNumber(m["maximumY"], c.maximumY) ||
      !finiteNumber(m["minimumX"], c.minimumX) ||
      !finiteNumber(m["maximumX"], c.maximumX))
    return false;
  if (c.minimumY >= c.maximumY || c.minimumX >= c.maximumX ||
      (c.logarithmic && c.kind != 4 && c.kind != 9) ||
      (c.logarithmic && c.manualY && c.minimumY <= 0))
    return false;
  if ((c.kind <= 3 || c.kind == 5 || c.kind == 6) && c.manualY &&
      (c.minimumY > 0 || c.maximumY < 0))
    return false;
  if (m["seriesColors"].metaType().id() != QMetaType::QVariantMap ||
      m["seriesColors"].toMap().size() > Table::maxRows * Table::maxColumns)
    return false;
  const auto colors = m["seriesColors"].toMap();
  for (auto it = colors.cbegin(); it != colors.cend(); ++it) {
    const QColor color(it.value().toString());
    if (it.key().isEmpty() || it.key().size() > 128 || !color.isValid() ||
        color.alpha() != 255)
      return false;
    c.seriesColors[it.key()] = color;
  }
  out = c;
  return true;
}
bool Chart::validate(const SceneObject &o, QString *error) {
  ChartData decoded;
  bool ok = decode(encode(o.chart), decoded) && Table::validate(o.table) &&
            o.table.rows.size() >= 2 && o.table.columns.size() >= 2;
  QSet<QString> ids;
  for (const auto &cell : o.table.cells) {
    if (cell.rowSpan != 1 || cell.columnSpan != 1)
      ok = false;
    ids.insert(cell.id);
  }
  for (auto it = o.chart.seriesColors.cbegin();
       it != o.chart.seriesColors.cend(); ++it)
    if (!ids.contains(it.key()))
      ok = false;
  if (!ok && error)
    *error = "The chart has invalid settings, merged data cells or fewer than "
             "two rows/columns.";
  return ok;
}
QString Chart::number(qreal value, const ChartData &c) {
  if (value == 0)
    value = 0;
  QLocale locale(c.locale);
  QString text;
  if (c.numberFormat == 0)
    text = locale.toString(value, 'g', 6);
  else
    text = locale.toString(c.numberFormat == 2 ? value * 100 : value, 'f',
                           c.decimals);
  return c.numberFormat == 2   ? text + '%'
         : c.numberFormat == 3 ? c.currency + text
                               : text;
}
QVector<QColor> Chart::palette(const DeckTheme &theme, int variant) {
  const QColor background = theme.colors.value("background", QColor("#10151f")),
               accent = theme.colors.value("accent", QColor("#4b91bb"));
  QVector<QColor> colors;
  // A restrained two-hue default. Markers, line styles and high-contrast
  // hatching also identify series; colour is never the only distinction.
  const QStringList fallback =
      variant == 1 ? QStringList{"#477c9a", "#ae6950", "#5b8d79",
                                 "#91835a", "#7c7097", "#9d6379"}
                   : QStringList{"#477c9a", "#719aab", "#466b75",
                                 "#7b8394", "#58617a", "#9ba4ad"};
  for (int i = 0; i < 8; ++i) {
    QColor c =
        (variant == 0 ? theme.colors : QMap<QString, QColor>())
            .value(QString("chart%1").arg(i + 1),
                   i == 0 ? accent : QColor(fallback[i % fallback.size()]));
    if (variant == 2)
      c = theme.colors.value("foreground", QColor(Qt::white));
    c.setAlpha(255);
    colors.append(contrastColor(c, background, 3));
  }
  return colors;
}
Chart::Layout Chart::layout(const SceneObject &o) {
  Layout l;
  QString validation;
  if (!validate(o, &validation)) {
    l.issues.append(validation);
    return l;
  }
  const auto &c = o.chart;
  const auto &t = o.table;
  const int rows = t.rows.size() - 1, columns = t.columns.size(),
            series = columns - 1;
  l.horizontal = c.kind == 1 || c.kind == 3;
  l.circular = c.kind == 7 || c.kind == 8;
  for (int s = 1; s < columns; ++s)
    l.series.append(t.cells[s].text.isEmpty() ? QString("Series %1").arg(s)
                                              : t.cells[s].text);
  for (int r = 1; r <= rows; ++r)
    l.categories.append(t.cells[r * columns].text.isEmpty()
                            ? QString::number(r)
                            : t.cells[r * columns].text);
  QLocale locale(c.locale);
  QVector<QVector<std::optional<qreal>>> values(rows);
  QVector<std::optional<qreal>> xs(rows);
  qreal low = 0, high = 0, minPositive = 1e15, xLow = 1e15, xHigh = -1e15;
  int numbers = 0;
  for (int r = 0; r < rows; ++r) {
    qreal positive = 0, negative = 0;
    values[r].resize(series);
    if (c.kind == 9) {
      bool invalid = false;
      xs[r] = parse(t.cells[(r + 1) * columns].text, locale, invalid);
      if (invalid) {
        ++l.invalid;
        if (l.issues.size() < 12)
          l.issues.append(Table::address(r + 1, 0) + ": invalid X value");
      }
      if (xs[r]) {
        xLow = qMin(xLow, *xs[r]);
        xHigh = qMax(xHigh, *xs[r]);
      }
    }
    for (int s = 0; s < (l.circular ? 1 : series); ++s) {
      bool invalid = false;
      const auto value =
          parse(t.cells[(r + 1) * columns + s + 1].text, locale, invalid);
      values[r][s] = value;
      if (invalid) {
        ++l.invalid;
        if (l.issues.size() < 12)
          l.issues.append(Table::address(r + 1, s + 1) +
                          ": invalid number for " + c.locale);
        continue;
      }
      if (!value) {
        ++l.missing;
        continue;
      }
      if (c.kind == 9 && !xs[r]) {
        ++l.missing;
        continue;
      }
      ++numbers;
      low = qMin(low, *value);
      high = qMax(high, *value);
      if (*value > 0)
        minPositive = qMin(minPositive, *value);
      if (*value >= 0)
        positive += *value;
      else
        negative += *value;
      if (c.logarithmic && *value <= 0 &&
          !l.issues.contains(
              "Logarithmic scales require values greater than zero."))
        l.issues.append("Logarithmic scales require values greater than zero.");
      if (l.circular && s == 0 && *value < 0 &&
          !l.issues.contains("Pie and donut values cannot be negative."))
        l.issues.append("Pie and donut values cannot be negative.");
    }
    if (c.kind == 2 || c.kind == 3 || c.kind == 6) {
      low = qMin(low, negative);
      high = qMax(high, positive);
    }
  }
  if (!numbers)
    l.issues.append("Add numeric data to draw this chart.");
  if (l.circular && series > 1)
    l.warnings.append("Pie and donut charts display the first value series.");
  if (l.missing)
    l.warnings.append(QString("%1 missing value%2 %3 left as gaps.")
                          .arg(l.missing)
                          .arg(l.missing == 1 ? "" : "s")
                          .arg(l.missing == 1 ? "is" : "are"));
  if (!l.issues.isEmpty())
    return l;
  const qreal fs = qMax(2.0, o.fontSize), margin = fs * .7;
  int legendEntries = l.circular ? rows : series;
  const qreal legendCell = qMax(fs * 5.5, o.rect.width() / 4);
  const int perRow = qMax(1, int((o.rect.width() - 2 * margin) / legendCell));
  const int legendRows = c.legend ? (legendEntries + perRow - 1) / perRow : 0;
  const qreal top = margin + (c.title.isEmpty() ? 0 : fs * 1.8),
              bottom =
                  margin +
                  (l.circular || (l.horizontal ? c.yTitle : c.xTitle).isEmpty()
                       ? 0
                       : fs * 1.5) +
                  (!l.circular ? fs * 2.8 : 0) + legendRows * fs * 1.6;
  l.plot = o.rect.adjusted(l.circular ? margin : fs * 5, top,
                           -(l.horizontal || c.kind == 9 ? fs * 2 : margin),
                           -bottom);
  if (l.plot.width() < fs * 4 || l.plot.height() < fs * 3) {
    l.issues.append(
        "Enlarge the chart or hide its legend to make room for the plot.");
    return l;
  }
  auto color = [&](int s, int category = -1) {
    const auto key = category >= 0 ? t.cells[(category + 1) * columns].id
                                   : t.cells[s + 1].id;
    auto colors = c.resolvedColors;
    if (colors.isEmpty())
      colors = palette(DeckTheme(), c.palette);
    const auto result = c.seriesColors.value(
        key, colors[(category >= 0 ? category : s) % colors.size()]);
    const qreal foreground = luminance(result), background = luminance(o.fill);
    const QString warning = "A series colour has less than 3:1 contrast "
                            "against the chart background.";
    if ((qMax(foreground, background) + .05) /
                (qMin(foreground, background) + .05) <
            3 &&
        !l.warnings.contains(warning))
      l.warnings.append(warning);
    return result;
  };
  if (l.circular) {
    qreal sum = 0;
    for (int r = 0; r < rows; ++r)
      if (values[r][0])
        sum += *values[r][0];
    if (sum <= 0) {
      l.issues.append("Pie and donut charts need a positive total.");
      return l;
    }
    const qreal diameter = qMin(l.plot.width(), l.plot.height());
    QRectF circle(l.plot.center() - QPointF(diameter / 2, diameter / 2),
                  QSizeF(diameter, diameter));
    qreal angle = 90;
    for (int r = 0; r < rows; ++r)
      if (values[r][0] && *values[r][0] > 0) {
        Mark mark;
        mark.kind = Mark::Slice;
        mark.category = r;
        mark.value = *values[r][0];
        mark.color = color(0, r);
        mark.rect = circle;
        mark.startAngle = angle;
        mark.sweepAngle = -360 * mark.value / sum;
        angle += mark.sweepAngle;
        l.marks.append(mark);
      }
    return l;
  }
  if (!c.includeZero && c.kind != 0 && c.kind != 1 && c.kind != 2 &&
      c.kind != 3 && c.kind != 5 && c.kind != 6) {
    low = 1e15;
    high = -1e15;
    for (int r = 0; r < rows; ++r)
      for (int s = 0; s < series; ++s)
        if (values[r][s] && (c.kind != 9 || xs[r])) {
          low = qMin(low, *values[r][s]);
          high = qMax(high, *values[r][s]);
        }
  }
  if (c.logarithmic) {
    low = std::pow(10, std::floor(std::log10(minPositive)));
    high = std::pow(10, std::ceil(std::log10(qMax(high, minPositive))));
    if (high <= low)
      high = low * 10;
  } else {
    if (high <= low) {
      const qreal delta = qMax(1.0, std::abs(low) * .1);
      low -= delta;
      high += delta;
    }
    const qreal step = tickStep(high - low);
    low = std::floor(low / step) * step;
    high = std::ceil(high / step) * step;
  }
  if (c.manualY) {
    low = c.minimumY;
    high = c.maximumY;
  }
  l.minY = low;
  l.maxY = high;
  auto fraction = [&](qreal value) {
    return c.logarithmic ? (std::log10(value) - std::log10(low)) /
                               (std::log10(high) - std::log10(low))
                         : (value - low) / (high - low);
  };
  auto valuePos = [&](qreal value) {
    return l.horizontal ? l.plot.left() + fraction(value) * l.plot.width()
                        : l.plot.bottom() - fraction(value) * l.plot.height();
  };
  if (c.logarithmic) {
    for (int power = int(std::floor(std::log10(low)));
         power <= int(std::ceil(std::log10(high))) && l.yTicks.size() < 40;
         ++power) {
      qreal n = std::pow(10, power);
      if (n >= low && n <= high)
        l.yTicks.append({n, valuePos(n), number(n, c)});
    }
  } else {
    const qreal step = tickStep(high - low);
    for (qreal v = std::ceil(low / step) * step;
         v <= high + step * .001 && l.yTicks.size() < 20; v += step)
      l.yTicks.append({v, valuePos(v), number(v, c)});
  }
  if (c.kind == 9) {
    if (xHigh <= xLow) {
      xLow -= 1;
      xHigh += 1;
    }
    if (!c.manualX) {
      const qreal padding = (xHigh - xLow) * .06;
      xLow -= padding;
      xHigh += padding;
    }
    qreal step = tickStep(xHigh - xLow);
    xLow = std::floor(xLow / step) * step;
    xHigh = std::ceil(xHigh / step) * step;
    if (c.manualX) {
      xLow = c.minimumX;
      xHigh = c.maximumX;
    }
    l.minX = xLow;
    l.maxX = xHigh;
    const auto xPos = [&](qreal v) {
      return l.plot.left() + (v - xLow) / (xHigh - xLow) * l.plot.width();
    };
    step = tickStep(xHigh - xLow);
    for (qreal x = std::ceil(xLow / step) * step;
         x <= xHigh + step * .001 && l.xTicks.size() < 20; x += step)
      l.xTicks.append({x, xPos(x), number(x, c)});
    for (int s = 0; s < series; ++s)
      for (int r = 0; r < rows; ++r)
        if (xs[r] && values[r][s]) {
          Mark mark;
          mark.kind = Mark::Point;
          mark.series = s;
          mark.category = r;
          mark.value = *values[r][s];
          mark.color = color(s);
          const QPointF point(xPos(*xs[r]), valuePos(mark.value));
          mark.rect = QRectF(point - QPointF(fs * .2, fs * .2),
                             QSizeF(fs * .4, fs * .4));
          l.marks.append(mark);
        }
  } else {
    const qreal categorySize =
        (l.horizontal ? l.plot.height() : l.plot.width()) / rows;
    for (int r = 0; r < rows; ++r)
      l.xTicks.append({qreal(r),
                       l.horizontal ? l.plot.top() + categorySize * (r + .5)
                                    : l.plot.left() + categorySize * (r + .5),
                       l.categories[r]});
    if (c.kind <= 3) {
      const bool stacked = c.kind == 2 || c.kind == 3;
      const qreal barWidth = categorySize * .72 / (stacked ? 1 : series);
      for (int r = 0; r < rows; ++r) {
        qreal positive = 0, negative = 0;
        for (int s = 0; s < series; ++s)
          if (values[r][s]) {
            const qreal v = *values[r][s],
                        base = stacked ? (v >= 0 ? positive : negative) : 0,
                        end = base + v;
            if (v >= 0)
              positive = end;
            else
              negative = end;
            const qreal a = valuePos(base), b = valuePos(end),
                        category = l.xTicks[r].position - categorySize * .36 +
                                   (stacked ? 0 : s * barWidth);
            Mark mark;
            mark.kind = Mark::Bar;
            mark.series = s;
            mark.category = r;
            mark.value = v;
            mark.color = color(s);
            mark.rect =
                l.horizontal
                    ? QRectF(qMin(a, b), category, std::abs(b - a), barWidth)
                    : QRectF(category, qMin(a, b), barWidth, std::abs(b - a));
            l.marks.append(mark);
          }
      }
    } else {
      QVector<qreal> positive(rows, 0), negative(rows, 0);
      for (int s = 0; s < series; ++s) {
        QVector<QPointF> top, bottom;
        auto flush = [&]() {
          if (top.isEmpty())
            return;
          Mark mark;
          mark.kind = c.kind == 4 ? Mark::Line : Mark::Area;
          mark.series = s;
          mark.category = -1;
          mark.color = color(s);
          mark.path.moveTo(top[0]);
          for (int i = 1; i < top.size(); ++i)
            mark.path.lineTo(top[i]);
          if (mark.kind == Mark::Area) {
            for (int i = bottom.size() - 1; i >= 0; --i)
              mark.path.lineTo(bottom[i]);
            mark.path.closeSubpath();
          }
          l.marks.append(mark);
          top.clear();
          bottom.clear();
        };
        for (int r = 0; r < rows; ++r) {
          if (!values[r][s]) {
            flush();
            continue;
          }
          const qreal v = *values[r][s],
                      base = c.kind == 6 ? (v >= 0 ? positive[r] : negative[r])
                                         : 0,
                      end = v + base;
          if (v >= 0)
            positive[r] = end;
          else
            negative[r] = end;
          const qreal x = l.xTicks[r].position;
          top.append({x, valuePos(end)});
          if (c.kind != 4)
            bottom.append({x, valuePos(base)});
          Mark point;
          point.kind = Mark::Point;
          point.series = s;
          point.category = r;
          point.value = v;
          point.color = color(s);
          point.rect = QRectF(QPointF(x - fs * .15, valuePos(end) - fs * .15),
                              QSizeF(fs * .3, fs * .3));
          l.marks.append(point);
        }
        flush();
      }
    }
  }
  int outside = 0;
  for (const auto &mark : l.marks)
    if (mark.kind == Mark::Point && !l.plot.contains(mark.rect.center()))
      ++outside;
  if (outside)
    l.warnings.append(
        QString("%1 points lie outside the chosen axis bounds.").arg(outside));
  if (!c.labels && !l.circular && rows > 12)
    l.warnings.append("Category labels may be abbreviated; full names remain "
                      "in the data editor.");
  return l;
}
void Chart::paint(QPainter &p, const SceneObject &o) {
  const auto l = layout(o);
  const auto &c = o.chart;
  const qreal fs = qMax(2.0, o.fontSize), margin = fs * .7;
  p.save();
  p.fillRect(o.rect, o.fill);
  QFont font(o.fontFamily);
  font.setPixelSize(qMax(1, qRound(fs)));
  font.setWeight(QFont::Weight(o.fontWeight));
  font.setItalic(o.italic);
  font.setUnderline(o.underline);
  p.setFont(font);
  p.setPen(o.textColor);
  const QString horizontalTitle =
      l.circular ? QString() : (l.horizontal ? c.yTitle : c.xTitle);
  const QString verticalTitle = l.horizontal ? c.xTitle : c.yTitle;
  auto within = [&](QRectF rect) {
    rect.setWidth(qMin(rect.width(), o.rect.width() - margin * 2));
    rect.moveLeft(qBound(o.rect.left() + margin, rect.left(),
                         o.rect.right() - margin - rect.width()));
    return rect;
  };
  auto label = [&](QRectF rect, const QString &text,
                   int alignment = Qt::AlignLeft | Qt::AlignVCenter,
                   bool elide = true) {
    p.setPen(o.textColor);
    const QFontMetricsF metrics(p.font());
    p.drawText(rect, alignment,
               elide ? metrics.elidedText(text, Qt::ElideRight,
                                          qMax(0.0, rect.width()))
                     : text);
  };
  if (!c.title.isEmpty()) {
    font.setWeight(QFont::DemiBold);
    p.setFont(font);
    label(o.rect.adjusted(margin, margin, -margin,
                          -o.rect.height() + margin + fs * 1.5),
          c.title);
    font.setWeight(QFont::Weight(o.fontWeight));
    p.setFont(font);
  }
  if (!l.issues.isEmpty()) {
    p.drawText(o.rect.adjusted(margin, margin + fs * 2.1, -margin, -margin),
               Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
               QStringLiteral("Check chart data\n") + l.issues.join('\n'));
    p.restore();
    return;
  }
  const auto &plot = l.plot;
  const QColor grid = QColor::fromRgbF(o.textColor.redF(), o.textColor.greenF(),
                                       o.textColor.blueF(), .18);
  if (!l.circular) {
    for (const auto &tick : l.yTicks) {
      if (c.grid) {
        p.setPen(QPen(grid, qMax(.5, fs * .04)));
        p.drawLine(l.horizontal ? QPointF(tick.position, plot.top())
                                : QPointF(plot.left(), tick.position),
                   l.horizontal ? QPointF(tick.position, plot.bottom())
                                : QPointF(plot.right(), tick.position));
      }
      if (l.horizontal)
        label(within(QRectF(tick.position - fs * 3, plot.bottom() + fs * .2,
                            fs * 6, fs * 1.5)),
              tick.label, Qt::AlignHCenter | Qt::AlignVCenter);
      else
        label(QRectF(o.rect.left() + margin, tick.position - fs * .65,
                     plot.left() - o.rect.left() - margin - fs * .4, fs * 1.3),
              tick.label, Qt::AlignRight | Qt::AlignVCenter);
    }
    const int count = l.xTicks.size();
    const qreal slot =
        (l.horizontal ? plot.height() : plot.width()) / qMax(1, count);
    for (const auto &tick : l.xTicks) {
      if (l.horizontal)
        label(QRectF(o.rect.left() + margin, tick.position - fs * .7,
                     plot.left() - o.rect.left() - margin - fs * .5, fs * 1.4),
              tick.label, Qt::AlignRight | Qt::AlignVCenter);
      else
        label(within(QRectF(tick.position - slot * .48, plot.bottom() + fs * .3,
                            slot * .96, fs * 1.5)),
              tick.label, Qt::AlignHCenter | Qt::AlignVCenter);
    }
    p.setPen(QPen(o.textColor, qMax(.5, fs * .06)));
    p.drawLine(plot.bottomLeft(), plot.bottomRight());
    p.drawLine(plot.topLeft(), plot.bottomLeft());
  }
  auto brush = [&](const Mark &m) {
    return QBrush(m.color, c.palette == 2 ? Qt::BrushStyle(Qt::Dense2Pattern +
                                                           (m.series % 6))
                                          : Qt::SolidPattern);
  };
  p.save();
  p.setClipRect(plot, Qt::IntersectClip);
  for (const auto &m : l.marks)
    if (m.kind == Mark::Area || m.kind == Mark::Line) {
      QColor color = m.color;
      if (m.kind == Mark::Area) {
        color.setAlphaF(.3);
        p.fillPath(m.path, color);
      }
      p.setPen(QPen(m.color, qMax(.7, fs * .1),
                    Qt::PenStyle(Qt::SolidLine + m.series % 5)));
      p.setBrush(Qt::NoBrush);
      p.drawPath(m.path);
    }
  for (const auto &m : l.marks) {
    if (m.kind == Mark::Bar) {
      p.setPen(QPen(m.color, qMax(.5, fs * .04)));
      p.setBrush(brush(m));
      p.drawRect(m.rect);
    } else if (m.kind == Mark::Point) {
      p.setPen(QPen(m.color, qMax(.5, fs * .06)));
      p.setBrush(m.color);
      switch (m.series % 4) {
      case 0:
        p.drawEllipse(m.rect);
        break;
      case 1:
        p.drawRect(m.rect);
        break;
      case 2: {
        QPainterPath diamond;
        diamond.moveTo(m.rect.center().x(), m.rect.top());
        diamond.lineTo(m.rect.right(), m.rect.center().y());
        diamond.lineTo(m.rect.center().x(), m.rect.bottom());
        diamond.lineTo(m.rect.left(), m.rect.center().y());
        diamond.closeSubpath();
        p.drawPath(diamond);
        break;
      }
      default:
        p.drawLine(m.rect.topLeft(), m.rect.bottomRight());
        p.drawLine(m.rect.topRight(), m.rect.bottomLeft());
        break;
      }
    } else if (m.kind == Mark::Slice) {
      p.save();
      if (c.kind == 8) {
        QPainterPath ring;
        ring.setFillRule(Qt::OddEvenFill);
        ring.addEllipse(m.rect);
        ring.addEllipse(QRectF(m.rect.center() - QPointF(m.rect.width() * .28,
                                                         m.rect.height() * .28),
                               m.rect.size() * .56));
        p.setClipPath(ring, Qt::IntersectClip);
      }
      p.setPen(QPen(o.fill, qMax(.5, fs * .06)));
      Mark slice = m;
      slice.series = m.category;
      p.setBrush(brush(slice));
      p.drawPie(m.rect, qRound(m.startAngle * 16), qRound(m.sweepAngle * 16));
      p.restore();
    }
  }
  p.restore();
  if (c.labels) {
    const QFontMetricsF metrics(p.font());
    for (const auto &m : l.marks) {
      if (m.kind == Mark::Line || m.kind == Mark::Area)
        continue;
      QPointF point;
      if (m.kind == Mark::Slice) {
        const qreal angle = (m.startAngle + m.sweepAngle / 2) * M_PI / 180;
        point = m.rect.center() + QPointF(std::cos(angle), -std::sin(angle)) *
                                      m.rect.width() * .37;
      } else if (m.kind == Mark::Bar)
        point = l.horizontal
                    ? QPointF(m.value >= 0 ? m.rect.right() + fs
                                           : m.rect.left() - fs,
                              m.rect.center().y())
                    : QPointF(m.rect.center().x(),
                              m.value >= 0 ? m.rect.top() - fs * .75
                                           : m.rect.bottom() + fs * .75);
      else
        point = m.rect.center() + QPointF(0, -fs * .75);
      if (!plot.adjusted(-fs, -fs, fs, fs).contains(point))
        continue;
      QString value = number(m.value, c);
      const qreal width = metrics.horizontalAdvance(value) + fs * .5;
      QRectF box(point - QPointF(width / 2, fs * .65), QSizeF(width, fs * 1.3));
      box = within(box);
      p.fillRect(box, o.fill);
      label(box, value, Qt::AlignCenter, false);
    }
  }
  if (!horizontalTitle.isEmpty())
    label(QRectF(plot.left(), o.rect.bottom() - margin - fs * 1.5, plot.width(),
                 fs * 1.4),
          horizontalTitle, Qt::AlignCenter);
  if (!verticalTitle.isEmpty() && !l.circular) {
    p.save();
    p.translate(o.rect.left() + fs * .6, plot.center().y());
    p.rotate(-90);
    label(QRectF(-plot.height() / 2, -fs * .6, plot.height(), fs * 1.2),
          verticalTitle, Qt::AlignCenter);
    p.restore();
  }
  if (c.legend) {
    const auto entries = l.circular ? l.categories : l.series;
    const qreal slot = qMax(fs * 5.5, o.rect.width() / 4);
    const int perRow = qMax(1, int((o.rect.width() - 2 * margin) / slot));
    const int rows = (entries.size() + perRow - 1) / perRow;
    const qreal y = o.rect.bottom() - margin -
                    (horizontalTitle.isEmpty() ? 0 : fs * 1.5) -
                    rows * fs * 1.6;
    for (int i = 0; i < entries.size(); ++i) {
      const QPointF origin(o.rect.left() + margin + (i % perRow) * slot,
                           y + (i / perRow) * fs * 1.6);
      QColor color;
      for (const auto &mark : l.marks)
        if ((l.circular ? mark.category : mark.series) == i) {
          color = mark.color;
          break;
        }
      if (!color.isValid())
        color = o.textColor;
      Mark mark;
      mark.color = color;
      mark.series = i;
      p.setPen(Qt::NoPen);
      p.setBrush(brush(mark));
      p.drawRect(
          QRectF(origin + QPointF(0, fs * .35), QSizeF(fs * .9, fs * .7)));
      label(QRectF(origin + QPointF(fs * 1.3, 0),
                   QSizeF(slot - fs * 1.5, fs * 1.4)),
            entries[i]);
    }
  }
  p.restore();
}
