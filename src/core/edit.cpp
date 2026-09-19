#include "core/design.h"
#include "core/edit.h"
#include "core/connector.h"

#include <QUuid>

QString Edit::newId(const QString &prefix) {
    return prefix + QLatin1Char('-')
         + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

int Edit::addSlide(Document &document, int afterIndex) {
    Slide slide;
    slide.id = newId(QStringLiteral("slide"));
    slide.background = document.slides.isEmpty() ? QColor(11, 22, 38)
                                                 : document.slides.first().background;
    const int at = qBound(0, afterIndex + 1, document.slides.size());
    const QString layoutId = afterIndex >= 0 && afterIndex < document.slides.size()
        ? document.slides.at(afterIndex).layoutId : QString();
    if (afterIndex >= 0 && afterIndex < document.slides.size()) slide.sectionId = document.slides.at(afterIndex).sectionId;
    document.slides.insert(at, slide);
    if (!layoutId.isEmpty()) Design::applyLayout(document, at, layoutId);
    return at;
}

int Edit::duplicateSlide(Document &document, int index) {
    if (index < 0 || index >= document.slides.size())
        return index;
    Slide copy = document.slides.at(index);
    copy.id = newId(QStringLiteral("slide"));
    document.slides.insert(index + 1, copy);
    return index + 1;
}

bool Edit::deleteSlide(Document &document, int index) {
    if (index < 0 || index >= document.slides.size() || document.slides.size() <= 1)
        return false;
    document.slides.removeAt(index);
    return true;
}

bool Edit::moveSlide(Document &document, int from, int to) {
    if (from < 0 || from >= document.slides.size())
        return false;
    to = qBound(0, to, document.slides.size() - 1);
    if (from == to)
        return false;
    const QString targetSection = document.slides.at(to).sectionId;
    document.slides.move(from, to);
    document.slides[to].sectionId = targetSection;
    return true;
}

QString Edit::addText(Document &document, int slideIndex, const QPointF &centre) {
    if (slideIndex < 0 || slideIndex >= document.slides.size())
        return QString();

    SceneObject object;
    object.id = newId(QStringLiteral("text"));
    object.type = ObjectType::Text;
    object.text = QStringLiteral("Text");
    object.fontSize = 64;
    object.fontWeight = 600;
    object.textColorToken = QStringLiteral("foreground");
    object.fontToken = QStringLiteral("body");
    object.textColor = document.theme.colors.value(object.textColorToken,QColor(231,237,247));
    object.fontFamily = document.theme.fonts.value(object.fontToken,object.fontFamily);
    object.rect = QRectF(centre.x() - 350, centre.y() - 50, 700, 100);
    document.slides[slideIndex].objects.append(object);
    return object.id;
}

QString Edit::addRect(Document &document, int slideIndex, const QPointF &centre) {
    if (slideIndex < 0 || slideIndex >= document.slides.size())
        return QString();

    SceneObject object;
    object.id = newId(QStringLiteral("rect"));
    object.type = ObjectType::Rect;
    object.fillToken = QStringLiteral("accent");
    object.fill = document.theme.colors.value(object.fillToken,QColor(39,194,255));
    object.cornerRadius = 12;
    object.rect = QRectF(centre.x() - 200, centre.y() - 120, 400, 240);
    document.slides[slideIndex].objects.append(object);
    return object.id;
}

bool Edit::deleteObject(Document &document, int slideIndex, const QString &id) {
    if (slideIndex < 0 || slideIndex >= document.slides.size())
        return false;

    const auto resolved=Design::resolve(document,slideIndex);
    Slide &slide = document.slides[slideIndex];
    for (int i = 0; i < slide.objects.size(); ++i) {
        if (slide.objects.at(i).id != id)
            continue;
        Connector::detachTarget(slide,resolved,id);
        slide.objects.removeAt(i);
        // A build pointing at a deleted object would be an invisible orphan.
        for (int j = slide.timeline.steps.size() - 1; j >= 0; --j) {
            if (slide.timeline.steps.at(j).targetId == id)
                slide.timeline.steps.removeAt(j);
        }
        return true;
    }
    return false;
}

bool Edit::raiseObject(Document &document, int slideIndex, const QString &id, int delta) {
    if (slideIndex < 0 || slideIndex >= document.slides.size())
        return false;

    Slide &slide = document.slides[slideIndex];
    for (int i = 0; i < slide.objects.size(); ++i) {
        if (slide.objects.at(i).id != id)
            continue;
        const int to = qBound(0, i + delta, slide.objects.size() - 1);
        if (to == i)
            return false;
        slide.objects.move(i, to);
        return true;
    }
    return false;
}
