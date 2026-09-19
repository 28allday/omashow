#include "core/objectcopy.h"
#include "core/design.h"
#include "core/edit.h"
#include <QSet>
Document ObjectCopy::extract(const Document &source, int index, const QStringList &ids,
                             int scopeDepth) {
    Document fragment;
    fragment.size = source.size;
    if (index < 0 || index >= source.slides.size())
        return fragment;
    const Slide resolved = Design::resolve(source, index);
    Slide slide;
    slide.id = "clipboard";
    for (const auto &o : resolved.objects)
        if (ids.contains(o.id) && source.slides.at(index).find(o.id)) {
            auto copy = Design::detached(o);
            copy.groups = copy.groups.mid(scopeDepth);
            slide.objects.append(copy);
        }
    const auto &steps = source.slides.at(index).timeline.steps;
    const auto times = source.slides.at(index).timeline.resolvedSteps();
    int previous = -2;
    for (int i = 0; i < steps.size(); ++i)
        if (ids.contains(steps.at(i).targetId)) {
            auto step = steps.at(i);
            if (previous != i - 1 && (step.trigger == BuildTrigger::AfterPrevious ||
                                      step.trigger == BuildTrigger::WithPrevious)) {
                step.trigger = BuildTrigger::Absolute;
                step.start = times.at(i).start;
                step.delay = 0;
            }
            slide.timeline.steps.append(step);
            previous = i;
        }
    fragment.slides.append(slide);
    return fragment;
}
QStringList ObjectCopy::insert(Document &d, int index, const Document &fragment,
                               const QStringList &scope, const QPointF &offset) {
    if (index < 0 || index >= d.slides.size() || fragment.slides.size() != 1)
        return {};
    const auto source = Design::resolve(fragment, 0);
    if (source.objects.isEmpty() || source.objects.size() > 10000)
        return {};
    QMap<QString, QString> objects, groups;
    for (const auto &o : source.objects) {
        if (o.id.isEmpty() || objects.contains(o.id))
            return {};
        objects[o.id] = Edit::newId("object");
        for (const auto &group : o.groups)
            if (!groups.contains(group))
                groups[group] = Edit::newId("group");
    }
    QStringList ids;
    auto &target = d.slides[index];
    for (auto o : source.objects) {
        o = Design::detached(o);
        o.id = objects.value(o.id);
        QMap<QString,QColor> colors;
        for(auto &cell:o.table.cells) { const auto old=cell.id; cell.id=Edit::newId("cell"); if(o.chart.seriesColors.contains(old)) colors[cell.id]=o.chart.seriesColors[old]; }
        o.chart.seriesColors=colors;
        o.rect.translate(offset);
        if(o.connector) { o.connectorFrom=objects.value(o.connectorFrom); o.connectorTo=objects.value(o.connectorTo); o.connectorStart+=offset; o.connectorEnd+=offset; }
        o.locked = false;
        o.hidden = false;
        auto nested = scope;
        for (const auto &group : o.groups)
            nested.append(groups.value(group));
        o.groups = nested;
        ids.append(o.id);
        target.objects.append(o);
    }
    for (auto step : source.timeline.steps)
        if (objects.contains(step.targetId)) {
            step.targetId = objects.value(step.targetId);
            target.timeline.steps.append(step);
        }
    return ids;
}
