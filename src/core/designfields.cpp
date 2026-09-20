#include "core/design.h"
#include <cmath>

QVariantMap Design::fieldProperties(const MasterFields &f) {
    return {{"showNumber",f.showNumber},{"showDate",f.showDate},{"showFooter",f.showFooter},
            {"hideOnFirst",f.hideOnFirst},{"firstNumber",f.firstNumber},{"date",f.date},{"footer",f.footer}};
}

bool Design::setFieldProperty(MasterFields &f, const QString &key, const QVariant &value) {
    if (QStringList{"showNumber","showDate","showFooter","hideOnFirst"}.contains(key)) {
        if (value.metaType().id() != QMetaType::Bool) return false;
        if (key == "showNumber") f.showNumber = value.toBool();
        if (key == "showDate") f.showDate = value.toBool();
        if (key == "showFooter") f.showFooter = value.toBool();
        if (key == "hideOnFirst") f.hideOnFirst = value.toBool();
    } else if (key == "firstNumber") {
        bool ok = false; const qreal n = value.toDouble(&ok);
        if (!ok || !std::isfinite(n) || n < 0 || n > 999999 || n != int(n)) return false;
        f.firstNumber = int(n);
    } else if (key == "date" || key == "footer") {
        if (value.metaType().id() != QMetaType::QString || value.toString().size() > (key == "date" ? 80 : 500)) return false;
        if (key == "date") f.date = value.toString(); else f.footer = value.toString();
    } else return false;
    return true;
}

QVector<SceneObject> Design::fields(const Document &d, int index, const Master &master) {
    const auto &f = master.fields;
    if (f.hideOnFirst && index == 0) return {};
    QVector<SceneObject> result;
    const auto add = [&](const QString &role, const QString &text, qreal x, qreal width, int align) {
        if (text.trimmed().isEmpty()) return;
        SceneObject object;
        object.id = "@field/" + master.id + '/' + role;
        object.type = ObjectType::Text;
        object.text = text;
        object.rect = QRectF(d.size.width()*x, d.size.height()*.925,
                            d.size.width()*width, d.size.height()*.045);
        object.fontSize = d.size.height()*.025;
        object.fontToken = "body"; object.textColorToken = "muted";
        object.textAlign = align; object.verticalAlign = 1; object.textFit = 1;
        object.locked = true;
        result.append(themed(d.theme,object));
    };
    if (f.showFooter) add("footer",f.footer,.08,.52,0);
    if (f.showDate) add("date",f.date,.62,.22,2);
    if (f.showNumber) add("number",QString::number(qint64(f.firstNumber)+index),.87,.05,2);
    return result;
}

qreal Design::contrastRatio(QColor color, const QColor &background) {
    // WCAG 2.2 SC 1.4.3, sRGB relative luminance; compare the unrounded ratio.
    // https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html
    const auto luminance = [](const QColor &color) {
        const auto linear = [](qreal c) { return c <= .04045 ? c/12.92 : std::pow((c+.055)/1.055,2.4); };
        return .2126*linear(color.redF())+.7152*linear(color.greenF())+.0722*linear(color.blueF());
    };
    if (!color.isValid() || !background.isValid() || background.alpha() != 255) return 0;
    if (color.alpha() < 255) {
        const qreal alpha = color.alphaF();
        color = QColor::fromRgbF(color.redF()*alpha+background.redF()*(1-alpha),
                                 color.greenF()*alpha+background.greenF()*(1-alpha),
                                 color.blueF()*alpha+background.blueF()*(1-alpha));
    }
    const qreal foreground = luminance(color), back = luminance(background);
    return (qMax(foreground,back)+.05)/(qMin(foreground,back)+.05);
}
QVariantList Design::themeContrast(const DeckTheme &theme) {
    const auto background = theme.colors.value("background");
    QVariantList result;
    for (const QString &token : {"foreground","muted","accent"}) {
        const qreal ratio = contrastRatio(theme.colors.value(token), background);
        const bool valid = ratio > 0;
        result.append(QVariantMap{{"token",token},{"known",valid},{"ratio",ratio},
                                  {"normal",valid && ratio >= 4.5},{"large",valid && ratio >= 3.0}});
    }
    return result;
}
