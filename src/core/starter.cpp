#include "core/starter.h"
#include "core/design.h"
#include "core/edit.h"
#include <cmath>
bool Starter::validSize(const QSizeF &size) {
    return std::isfinite(size.width()) && std::isfinite(size.height()) && size.width() >= 240 &&
           size.height() >= 240 && size.width() <= 10000 && size.height() <= 10000;
}
Document Starter::create(int theme, const QSizeF &size, int layout) {
    Document d;
    if (!validSize(size) || theme < 0 || theme > 2 || layout < 0 || layout > 2)
        return d;
    d.size = size;
    d.theme = Design::preset(theme);
    Design::ensureDefaults(d);
    Edit::addSlide(d, -1);
    Design::applyLayout(d, 0, d.layouts.at(layout).id);
    return d;
}
