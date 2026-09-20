#include "anim/presentation.h"
#include "backend.h"
#include "core/design.h"
#include <cmath>

QVariantList Backend::builds() const {
  QVariantList list;
  if (m_currentSlide < 0 || m_currentSlide >= m_document.slides.size())
    return list;
  const auto &timeline = m_document.slides.at(m_currentSlide).timeline;
  const auto resolved = timeline.resolvedSteps();
  const auto slide = Design::resolve(m_document, m_currentSlide);
  for (int i = 0; i < resolved.size(); ++i) {
    const auto &step = resolved.at(i);
    const auto *o = slide.find(step.targetId);
    const QString label =
        o ? (o->type == ObjectType::Text ? o->text.left(40) : o->type == ObjectType::Image ? tr("Picture") : o->type == ObjectType::Media ? o->mediaName : o->type == ObjectType::Table ? tr("Table") : o->type == ObjectType::Chart ? tr("Chart") : tr("Shape"))
          : tr("Missing object");
    list.append(QVariantMap{{"index", i},
                            {"targetId", step.targetId},
                            {"label", label},
                            {"effect", int(step.effect)},
                            {"phase", int(step.phase)},
                            {"trigger", int(step.trigger)},
                            {"start", step.start},
                            {"absoluteStart", timeline.steps.at(i).start},
                            {"duration", step.duration},
                            {"delay", step.delay},
                            {"easing", int(step.easing)},
                            {"amountX", step.amountX},
                            {"amountY", step.amountY},
                            {"amount", step.amount},
                            {"unit", step.unit},
                            {"text", o && o->type == ObjectType::Text}});
  }
  return list;
}
QVariantList Backend::slideObjects() const {
  QVariantList list;
  const auto slide = Design::resolve(m_document, m_currentSlide);
  if (m_currentSlide < 0 || m_currentSlide >= m_document.slides.size())
    return list;
  for (const auto &o : slide.objects)
    if (m_document.slides.at(m_currentSlide).find(o.id))
      list.append(
          QVariantMap{{"id", o.id},
                      {"name", o.type == ObjectType::Text ? o.text.left(40)
                                                          : o.type == ObjectType::Image ? tr("Picture") : o.type == ObjectType::Media ? o.mediaName : o.type == ObjectType::Table ? tr("Table") : o.type == ObjectType::Chart ? tr("Chart") : tr("Shape")}});
  return list;
}
qreal Backend::slideStart() const {
  return Presentation::slideStart(m_document,m_currentSlide,m_includeSkipped);
}
qreal Backend::slideDuration() const {
  return Presentation::slideDuration(m_document, m_currentSlide);
}
qreal Backend::localTime() const {
  return qBound(0.0, m_time - slideStart(), slideDuration());
}
void Backend::setLocalTime(qreal time) {
  pause();
  setTime(slideStart() + qBound(0.0, time, slideDuration()));
}
void Backend::setPlaybackRate(qreal rate) {
  if (!std::isfinite(rate) || rate < 0.1 || rate > 4 || rate == m_playbackRate)
    return;
  if (m_playing) {
    tick();
    m_playFrom = m_time;
    m_clock.restart();
  }
  m_playbackRate = rate;
  emit playbackRateChanged();
}
void Backend::previewSlide() {
  if (m_playing) {
    pause();
    return;
  }
  const qreal start = slideStart(), end = start + slideDuration();
  if (m_time < start || m_time >= end)
    setTime(start);
  play();
  m_playTo = end;
}
void Backend::previewBuild(int index) {
  const auto list = builds();
  if (index < 0 || index >= list.size())
    return;
  const auto step = list.at(index).toMap();
  pause();
  setTime(slideStart() + step.value("start").toDouble());
  play();
  m_playTo = slideStart() + step.value("start").toDouble() +
             step.value("duration").toDouble();
}
int Backend::addBuild(const QString &targetId, int phase, int effect) {
  if (m_currentSlide < 0 || m_currentSlide >= m_document.slides.size() ||
      !m_document.slides.at(m_currentSlide).find(targetId) || phase < 0 ||
      phase > 1 || effect < 1 || effect == int(Effect::Media) || effect > int(Effect::Reveal))
    return -1;
  m_history.begin(m_document, tr("Add build"));
  auto &timeline = m_document.slides[m_currentSlide].timeline;
  BuildStep step;
  step.targetId = targetId;
  step.phase = BuildPhase(phase);
  step.effect = Effect(effect);
  step.trigger = timeline.steps.isEmpty() ? BuildTrigger::Absolute
                                          : BuildTrigger::AfterPrevious;
  timeline.steps.append(step);
  const int index = timeline.steps.size() - 1;
  m_history.commit();
  touch();
  return index;
}
void Backend::removeBuild(int index) {
  if (index < 0 || index >= builds().size())
    return;
  m_history.begin(m_document, tr("Remove build"));
  m_document.slides[m_currentSlide].timeline.steps.removeAt(index);
  m_history.commit();
  touch();
}
void Backend::moveBuild(int from, int to) {
  const int size = builds().size();
  if (from < 0 || from >= size || to < 0 || to >= size || from == to)
    return;
  m_history.begin(m_document, tr("Reorder builds"));
  m_document.slides[m_currentSlide].timeline.steps.move(from, to);
  m_history.commit();
  touch();
}
void Backend::setBuildProperty(int index, const QString &key,
                               const QVariant &value) {
  if (index < 0 || index >= builds().size())
    return;
  BuildStep changed =
      m_document.slides.at(m_currentSlide).timeline.steps.at(index);
  if(changed.effect==Effect::Media && (key=="duration" || key=="effect" || key=="phase" || key=="easing")) return;
  bool ok = false;
  const qreal number = value.toDouble(&ok);
  if (!ok || !std::isfinite(number))
    return;
  if (key == "start") {
    changed.start = qMax(0.0, number);
    changed.trigger = BuildTrigger::Absolute;
  } else if (key == "duration")
    changed.duration = qMax(0.01, number);
  else if (key == "delay")
    changed.delay = qMax(0.0, number);
  else if (key == "trigger" && number >= 0 && number <= 3)
    changed.trigger = BuildTrigger(value.toInt());
  else if (key == "phase" && number >= 0 && number <= 1)
    changed.phase = BuildPhase(value.toInt());
  else if (key == "effect" && number >= 1 && number <= int(Effect::Reveal) &&
           number != int(Effect::Media))
    changed.effect = Effect(value.toInt());
  else if (key == "amountX" && std::abs(number) <= 100000)
    changed.amountX = number;
  else if (key == "amountY" && std::abs(number) <= 100000)
    changed.amountY = number;
  else if (key == "amount" && std::abs(number) <= 100000)
    changed.amount = number;
  else if (key == "unit" && number >= 0 && number <= 2)
    changed.unit = value.toInt();
  else if (key == "easing" && (number == QEasingCurve::Linear ||
                               number == QEasingCurve::OutCubic ||
                               number == QEasingCurve::InOutCubic))
    changed.easing = QEasingCurve::Type(value.toInt());
  else
    return;
  m_history.begin(m_document, tr("Edit build"));
  m_document.slides[m_currentSlide].timeline.steps[index] = changed;
  m_history.commit();
  touch();
}

void Backend::playUntil(qreal end) {
  play();
  m_playTo = qBound(m_time, end, duration());
}
QString Backend::slideNotes() const {
  return m_currentSlide >= 0 && m_currentSlide < m_document.slides.size()
             ? m_document.slides.at(m_currentSlide).notes
             : QString();
}
void Backend::setSlideNotes(const QString &notes) {
  if (m_currentSlide < 0 || m_currentSlide >= m_document.slides.size() ||
      slideNotes() == notes)
    return;
  m_history.begin(m_document, tr("Edit speaker notes"));
  m_document.slides[m_currentSlide].notes = notes;
  m_history.commit();
  touch();
}

int Backend::addBuildForSelection(int phase, int effect) {
    const auto ids=selectedIds();
    if(ids.isEmpty() || phase<0 || phase>1 || effect<1 || effect>2)return -1;
    m_history.begin(m_document,tr("Animate selected objects"));
    int first=-1;
    for(const auto &id:ids){
        const int index=addBuild(id,phase,effect);
        if(first<0)first=index;
        else setBuildProperty(index,"trigger",int(BuildTrigger::WithPrevious));
    }
    m_history.commit();touch();return first;
}

// --- transitions ------------------------------------------------------------
QVariantMap Backend::slideTransition() const {
  const bool has = m_currentSlide >= 0 && m_currentSlide < m_document.slides.size();
  const auto &slide = has ? m_document.slides.at(m_currentSlide) : Slide();
  return {{"kind", has ? slide.transition : -1},
          {"effective", Presentation::transitionKind(m_document, m_currentSlide)},
          {"direction", has ? slide.transitionDirection : 0},
          {"seconds", has ? slide.transitionSeconds : -1},
          {"effectiveSeconds", Presentation::transitionSeconds(m_document, m_currentSlide)},
          {"advanceAfter", has ? slide.advanceAfter : -1},
          {"first", m_currentSlide == 0},
          {"deckKind", m_document.transition},
          {"deckSeconds", m_document.transitionDuration},
          {"names", QStringList{tr("Cut"), tr("Fade"), tr("Push"), tr("Morph")}},
          {"directions", QStringList{tr("Left"), tr("Right"), tr("Up"), tr("Down")}}};
}

// `value` of -1 means "whatever the deck says", for every key but direction.
bool Backend::setSlideTransition(const QString &key, const QVariant &value, bool everySlide) {
  if (m_document.slides.isEmpty()) return false;
  Slide probe = m_document.slides.at(qBound(0, m_currentSlide, int(m_document.slides.size()) - 1));
  if (!applyTransition(probe, key, value)) return false;
  m_history.begin(m_document, everySlide ? tr("Change every transition")
                                         : tr("Change transition"));
  bool changed = false;
  for (int i = 0; i < m_document.slides.size(); ++i) {
    if (!everySlide && i != m_currentSlide) continue;
    Slide slide = m_document.slides.at(i);
    if (!applyTransition(slide, key, value)) continue;
    if (slide.transition == m_document.slides.at(i).transition &&
        slide.transitionDirection == m_document.slides.at(i).transitionDirection &&
        slide.transitionSeconds == m_document.slides.at(i).transitionSeconds &&
        slide.advanceAfter == m_document.slides.at(i).advanceAfter)
      continue;
    m_document.slides[i].transition = slide.transition;
    m_document.slides[i].transitionDirection = slide.transitionDirection;
    m_document.slides[i].transitionSeconds = slide.transitionSeconds;
    m_document.slides[i].advanceAfter = slide.advanceAfter;
    changed = true;
  }
  if (!changed) {
    // Valid, but already the case: nothing to record, nothing to report.
    m_history.abandon();
    return true;
  }
  m_history.commit();
  touch();
  return true;
}

bool Backend::applyTransition(Slide &slide, const QString &key, const QVariant &value) const {
  bool ok = false;
  const qreal number = value.toDouble(&ok);
  if (!ok || !std::isfinite(number)) return false;
  if (key == QLatin1String("kind")) {
    if (number < -1 || number > Presentation::Morph || number != qRound(number)) return false;
    slide.transition = int(number);
  } else if (key == QLatin1String("direction")) {
    if (number < 0 || number > 3 || number != qRound(number)) return false;
    slide.transitionDirection = int(number);
  } else if (key == QLatin1String("seconds")) {
    if (number < -1 || number > 10) return false;
    slide.transitionSeconds = number < 0 ? -1 : number;
  } else if (key == QLatin1String("advanceAfter")) {
    if (number < -1 || number > 3600) return false;
    slide.advanceAfter = number < 0 ? -1 : number;
  } else {
    return false;
  }
  return true;
}
