#include "presenter.h"
#include "anim/presentation.h"
#include "core/link.h"
#include "core/shape.h"
#include <QDesktopServices>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QTransform>
#include <algorithm>

Presenter::Presenter(Backend *backend, ShowGuard *guard, QObject *parent)
    : QObject(parent), m_backend(backend), m_guard(guard) {
  if (!QStandardPaths::isTestModeEnabled())
    m_preferredAudience =
        QSettings("omarchy", "omashow").value("presenter/audience").toString();
  connect(qGuiApp, &QGuiApplication::screenAdded, this,
          [this] { refreshDisplays(); });
  connect(qGuiApp, &QGuiApplication::screenRemoved, this,
          [this] { refreshDisplays(); });
  connect(backend, &Backend::timeChanged, this, &Presenter::timeChanged);
  connect(backend, &Backend::playingChanged, this, [this] {
    if (m_running) {
      m_status = m_backend->playing() ? tr("Build playing")
                 : m_userPaused       ? tr("Paused")
                                      : tr("Ready · Next to continue");
      emit stateChanged();
    }
  });
  connect(backend, &Backend::documentChanged, this, &Presenter::timeChanged);
  connect(backend, &Backend::currentSlideChanged, this,
          &Presenter::timeChanged);
  m_timer.setInterval(100);
  connect(&m_timer, &QTimer::timeout, this, &Presenter::clockChanged);
  m_advance.setSingleShot(true);
  connect(&m_advance, &QTimer::timeout, this, [this] {
    if (m_running && !m_userPaused && !m_frozen && m_blank == 0) next();
  });
  m_status = tr("Ready to present");
}
Presenter::~Presenter() {
  if (m_running)
    m_guard->release();
}
int Presenter::chooseAudience(const QStringList &names,
                              const QString &preferred) {
  if (names.isEmpty())
    return -1;
  const int saved = names.indexOf(preferred);
  return saved >= 0 ? saved : names.size() - 1;
}
QVariantList Presenter::displays() const {
  QVariantList list;
  int index = 0;
  for (auto *screen : QGuiApplication::screens()) {
    const auto size = screen->size();
    list.append(QVariantMap{{"index", index++},
                            {"name", screen->name()},
                            {"label", tr("%1 · %2 × %3")
                                          .arg(screen->name().isEmpty()
                                                   ? tr("Display %1").arg(index)
                                                   : screen->name())
                                          .arg(size.width())
                                          .arg(size.height())},
                            {"width", size.width()},
                            {"height", size.height()},
                            {"refreshRate", screen->refreshRate()}});
  }
  return list;
}
int Presenter::audienceIndex() const {
  QStringList names;
  for (auto *s : QGuiApplication::screens())
    names.append(s->name());
  return chooseAudience(names, m_preferredAudience);
}
int Presenter::presenterIndex() const {
  const int audience = audienceIndex();
  for (int i = 0; i < QGuiApplication::screens().size(); ++i)
    if (i != audience)
      return i;
  return audience;
}
void Presenter::setAudienceIndex(int index) {
  const auto screens = QGuiApplication::screens();
  if (index < 0 || index >= screens.size())
    return;
  m_preferredAudience = screens.at(index)->name();
  if (!QStandardPaths::isTestModeEnabled())
    QSettings("omarchy", "omashow")
        .setValue("presenter/audience", m_preferredAudience);
  if (m_running)
    routeWindows();
  emit displaysChanged();
}
void Presenter::attach(QWindow *audience, QWindow *console, QWindow *editor) {
  m_audience = audience;
  m_console = console;
  m_editor = editor;
}
void Presenter::refreshDisplays() {
  if (m_running) {
    if (QGuiApplication::screens().isEmpty()) {
      stop();
      m_status = tr("Display disconnected · show ended");
      emit stateChanged();
    } else
      routeWindows();
  }
  emit displaysChanged();
}
void Presenter::routeWindows() {
  if (!m_audience || !m_console)
    return;
  const auto screens = QGuiApplication::screens();
  const int audience = audienceIndex();
  if (audience < 0)
    return;
  // Hide the console before moving outputs, so it cannot migrate onto an
  // audience output during a swap or a screen removal.
  m_console->hide();
  m_audience->hide();
  m_audience->setScreen(screens.at(audience));
  m_audience->setGeometry(screens.at(audience)->geometry());
  if (m_rehearsal) {
    const auto area = screens.at(audience)->availableGeometry();
    m_audience->setGeometry(
        QRect(area.topLeft(), QSize(qMax(480, area.width() / 2),
                                    qMax(270, area.height() / 2))));
    m_audience->showNormal();
    m_console->setScreen(screens.at(presenterIndex()));
    m_console->setGeometry(
        QRect(screens.at(presenterIndex())->availableGeometry().topLeft(),
              QSize(1200, 760)));
    m_console->showNormal();
    m_console->requestActivate();
  } else {
    // The output is assigned BEFORE full screen, so Wayland receives an
    // output-specific fullscreen request.
    if (screens.size() > 1) {
      m_console->setScreen(screens.at(presenterIndex()));
      m_console->setGeometry(screens.at(presenterIndex())->availableGeometry());
      m_console->showMaximized();
    }
    m_audience->showFullScreen();
    if (screens.size() > 1)
      m_console->requestActivate();
    else
      m_audience->requestActivate();
  }
}
bool Presenter::start(bool fromCurrent, bool rehearsal) {
  if (m_running)
    return true;
  if (!m_audience || !m_console || m_backend->document().slides.isEmpty() ||
      audienceIndex() < 0) {
    m_status = tr("No audience window or display is available");
    emit stateChanged();
    return false;
  }
  const auto eligible = Presentation::slideIndices(m_backend->document());
  int first = -1;
  for (int i : eligible)
    if (!fromCurrent || i >= m_backend->currentSlide()) {
      first = i;
      break;
    }
  if (first < 0) {
    m_status = tr("No included slides from this position");
    emit stateChanged();
    return false;
  }
  m_backend->setIncludeSkipped(false);
  m_rehearsal = rehearsal;
  if (rehearsal) clearTimings();
  m_running = true;
  m_blank = 0;
  m_backend->setMediaSuppressed(false);
  m_frozen = false;
  m_userPaused = false;
  m_backend->pause();
  m_guard->engage();
  m_clock.start();
  m_timer.start();
  emit stateChanged();
  if (m_editor)
    m_editor->hide();
  routeWindows();
  jump(first);
  return true;
}
void Presenter::stop() {
  if (!m_running)
    return;
  m_elapsedStopped = elapsed();
  m_running = false;
  m_pendingExternalLink = QUrl();
  m_backend->pause();
  m_backend->setIncludeSkipped(true);
  m_backend->setTime(m_backend->slideStart());
  m_timer.stop();
  m_guard->release();
  m_frozen = false;
  m_blank = 0;
  recordSlideTime();
  m_timedSlide = -1;
  m_annotation = 0;
  m_drawing = false;
  m_ink.clear();
  hidePointer();
  emit annotationChanged();
  m_advance.stop();
  m_backend->setMediaSuppressed(false);
  if (m_audience)
    m_audience->hide();
  if (m_console)
    m_console->hide();
  if (m_editor) {
    m_editor->showNormal();
    m_editor->requestActivate();
  }
  m_status = tr("Show ended");
  emit stateChanged();
  emit timeChanged();
  emit clockChanged();
}
QVector<qreal> Presenter::clickBoundaries() const {
  QVector<qreal> result;
  const auto &d = m_backend->document();
  const int index = m_backend->currentSlide();
  if (index < 0 || index >= d.slides.size())
    return result;
  for (const auto &step : d.slides.at(index).timeline.resolvedSteps())
    if (step.trigger == BuildTrigger::OnClick)
      result.append(qMax(0.0, step.start - step.delay));
  return result;
}
void Presenter::runToBoundary() {
  const auto clicks = clickBoundaries();
  const auto &slide =
      m_backend->document().slides.at(m_backend->currentSlide());
  const qreal localEnd = m_consumedClicks < clicks.size()
                             ? clicks.at(m_consumedClicks)
                             : slide.timeline.duration();
  m_boundary = m_backend->slideStart() + localEnd;
  m_userPaused = false;
  // Reduced motion: arrive at the moment rather than travelling to it.
  if (m_backend->reducedMotion()) {
    m_backend->pause();
    m_backend->setTime(m_boundary);
    m_advance.stop();
    const auto &reduced = m_backend->document().slides.at(m_backend->currentSlide());
    if (m_consumedClicks >= clicks.size() && reduced.advanceAfter >= 0 &&
        nextSlideIndex() >= 0 && !m_frozen && m_blank == 0)
      m_advance.start(int(reduced.advanceAfter * 1000));
    m_status = tr("Ready · Next to continue");
    emit stateChanged();
    return;
  }
  // A slide that moves on by itself waits out its hold after the last build.
  m_advance.stop();
  if (m_consumedClicks >= clicks.size() && slide.advanceAfter >= 0 &&
      nextSlideIndex() >= 0 && !m_frozen && m_blank == 0) {
    const qreal remaining = qMax(qreal(0), m_boundary - m_backend->time());
    m_advance.start(int((remaining + slide.advanceAfter) * 1000));
  }
  if (m_boundary > m_backend->time() + .000001)
    m_backend->playUntil(m_boundary);
  else {
    m_backend->pause();
    m_status = tr("Ready · Next to continue");
    emit stateChanged();
  }
}
void Presenter::jump(int index) {
  if (!m_running || index < 0 || index >= m_backend->slideCount() ||
      m_backend->document().slides.at(index).skipped)
    return;
  recordSlideTime();
  m_timedSlide = index;
  m_slideClock.start();
  m_backend->pause();
  m_backend->setCurrentSlide(index);
  m_backend->setTime(m_backend->slideStart());
  m_consumedClicks = 0;
  runToBoundary();
  emit timeChanged();
}
void Presenter::next() {
  if (!m_running)
    return;
  if (m_backend->playing() || m_userPaused) {
    m_backend->pause();
    m_backend->setTime(m_boundary);
    m_userPaused = false;
    m_status = tr("Ready · Next to continue");
    emit stateChanged();
    return;
  }
  const auto clicks = clickBoundaries();
  if (m_consumedClicks < clicks.size()) {
    ++m_consumedClicks;
    runToBoundary();
    return;
  }
  const int next = nextSlideIndex();
  if (next < 0) {
    m_status = tr("Last slide · End show when ready");
    emit stateChanged();
    return;
  }
  const qreal transitionStart = m_backend->reducedMotion()
                                    ? m_backend->slideStart() + m_backend->slideDuration() +
                                          Presentation::transitionSeconds(m_backend->document(), next)
                                    : m_backend->slideStart() + m_backend->slideDuration();
  recordSlideTime();
  m_timedSlide = next;
  m_slideClock.start();
  m_backend->setCurrentSlide(next);
  m_backend->setTime(transitionStart);
  m_consumedClicks = 0;
  runToBoundary();
  emit timeChanged();
}
void Presenter::previous() {
  if (!m_running)
    return;
  m_backend->pause();
  m_userPaused = false;
  if (m_consumedClicks > 0) {
    --m_consumedClicks;
    const auto clicks = clickBoundaries();
    m_boundary = m_backend->slideStart() + clicks.at(m_consumedClicks);
    m_backend->setTime(m_boundary);
  } else {
    for (int i = m_backend->currentSlide() - 1; i >= 0; --i)
      if (!m_backend->document().slides.at(i).skipped) {
        jump(i);
        break;
      }
  }
  emit stateChanged();
  emit timeChanged();
}
void Presenter::pauseResume() {
  if (!m_running)
    return;
  if (m_backend->playing()) {
    m_userPaused = true;
    m_advance.stop();
    m_backend->pause();
  } else if (m_userPaused) {
    m_userPaused = false;
    m_backend->playUntil(m_boundary);
  }
  emit stateChanged();
}
void Presenter::setBlankMode(int mode) {
  m_blank = qBound(0, mode, 2);
  m_backend->setMediaSuppressed(m_blank!=0 || m_frozen);
  if (m_blank != 0) m_advance.stop();
  else if (m_running && !m_userPaused) runToBoundary();
  emit stateChanged();
}
void Presenter::setFrozen(bool frozen) {
  if (m_frozen == frozen)
    return;
  if (frozen)
    m_frozenTime = m_backend->time();
  m_frozen = frozen;
  m_backend->setMediaSuppressed(m_blank!=0 || m_frozen);
  if (m_frozen) m_advance.stop();
  else if (m_running && !m_userPaused) runToBoundary();
  emit stateChanged();
  emit timeChanged();
}
void Presenter::restartClock() {
  m_clock.restart();
  m_elapsedStopped = 0;
  emit clockChanged();
}
void Presenter::setTargetMinutes(int minutes) {
  m_targetMinutes = qBound(1, minutes, 600);
  emit clockChanged();
}
void Presenter::swapDisplays() {
  if (QGuiApplication::screens().size() > 1)
    setAudienceIndex(presenterIndex());
}
int Presenter::buildCount() const { return m_backend->builds().size(); }
int Presenter::buildNumber() const {
  int count = 0;
  for (const auto &value : m_backend->builds())
    if (value.toMap().value("start").toDouble() < m_backend->localTime())
      ++count;
  return count;
}

int Presenter::nextSlideIndex() const {
  for (int i = m_backend->currentSlide() + 1; i < m_backend->slideCount(); ++i)
    if (!m_backend->document().slides.at(i).skipped)
      return i;
  return -1;
}

bool Presenter::activateAt(qreal x, qreal y) {
  if (!m_running || m_blank != 0 || m_frozen)
    return false;
  const auto states =
      m_backend->statesAt(audienceTime(), false);
  for (int i = states.size() - 1; i >= 0; --i) {
    const auto &o = states[i];
    if (o.hidden || o.opacity <= .01)
      continue;
    QTransform transform;
    const auto c = o.rect.center();
    transform.translate(c.x(), c.y());
    transform.rotate(o.rotation);
    transform.translate(-c.x(), -c.y());
    if (!Shape::contains(o, transform.inverted().map(QPointF(x, y))))
      continue;
    if (o.linkKind == 0)
      return false;
    const auto error = Links::issue(o, m_backend->document());
    if (!error.isEmpty()) {
      m_status = error;
      emit stateChanged();
      return true;
    }
    if (o.linkKind == 1 || o.linkKind == 2) {
      m_backend->pause();
      m_userPaused = true;
      m_pendingExternalLink = QUrl(Links::normalized(o.linkKind, o.linkTarget));
      m_status = tr("Link ready · review in presenter console");
      emit stateChanged();
      emit externalLinkRequested(m_pendingExternalLink);
      return true;
    }
    if (o.linkKind == 3) {
      for (int index = 0; index < m_backend->slideCount(); ++index)
        if (m_backend->document().slides[index].id == o.linkTarget) {
          if (m_backend->document().slides[index].skipped) {
            m_status = tr("The linked slide is skipped in this show.");
            emit stateChanged();
          } else
            jump(index);
          break;
        }
    } else if (o.linkKind == 4)
      next();
    else if (o.linkKind == 5)
      previous();
    else if (o.linkKind == 6) {
      for (int index = 0; index < m_backend->slideCount(); ++index)
        if (!m_backend->document().slides[index].skipped) {
          jump(index);
          break;
        }
    } else if (o.linkKind == 7) {
      for (int index = m_backend->slideCount() - 1; index >= 0; --index)
        if (!m_backend->document().slides[index].skipped) {
          jump(index);
          break;
        }
    } else if (o.linkKind == 8)
      stop();
    return true;
  }
  return false;
}
bool Presenter::openPendingLink() {
  if (!m_running || m_pendingExternalLink.isEmpty())
    return false;
  const auto url = m_pendingExternalLink;
  m_pendingExternalLink = QUrl();
  emit stateChanged();
  if (!Links::validate(url.scheme() == "mailto" ? 2 : 1, url.toString(),
                       m_backend->document())
           .isEmpty())
    return false;
  const bool opened = QDesktopServices::openUrl(url);
  m_status = opened ? tr("Link opened · resume when ready")
                    : tr("The desktop could not open this link.");
  emit stateChanged();
  return opened;
}
void Presenter::cancelPendingLink() {
  if (m_pendingExternalLink.isEmpty())
    return;
  m_pendingExternalLink = QUrl();
  m_status = tr("Link dismissed · resume when ready");
  emit stateChanged();
}

// --- drawing over the show ---------------------------------------------------
void Presenter::setAnnotation(int mode) {
  const int wanted = qBound(0, mode, 3);
  if (wanted == m_annotation) return;
  m_annotation = wanted;
  if (m_annotation != 3) endStroke();
  if (m_annotation == 0) hidePointer();
  m_status = m_annotation == 1   ? tr("Pointer")
             : m_annotation == 2 ? tr("Spotlight")
             : m_annotation == 3 ? tr("Drawing")
                                 : m_status;
  emit annotationChanged();
  emit stateChanged();
}

QVariantList Presenter::ink() const {
  QVariantList strokes;
  const int slide = m_backend->currentSlide();
  for (const auto &stroke : m_ink) {
    if (stroke.slide != slide || stroke.points.size() < 2) continue;
    QVariantList points;
    for (const auto &point : stroke.points) points.append(QVariantMap{{"x", point.x()}, {"y", point.y()}});
    strokes.append(QVariantMap{{"points", points}});
  }
  return strokes;
}

void Presenter::movePointer(qreal x, qreal y) {
  m_pointer = QPointF(x, y);
  m_pointerVisible = m_annotation != 0;
  if (m_drawing) extendStroke(x, y);
  emit pointerChanged();
}

void Presenter::hidePointer() {
  if (!m_pointerVisible) return;
  m_pointerVisible = false;
  emit pointerChanged();
}

void Presenter::beginStroke(qreal x, qreal y) {
  if (m_annotation != 3) return;
  m_ink.append({m_backend->currentSlide(), {QPointF(x, y)}});
  m_drawing = true;
  emit annotationChanged();
}

void Presenter::extendStroke(qreal x, qreal y) {
  if (!m_drawing || m_ink.isEmpty()) return;
  auto &points = m_ink.last().points;
  // Ignore the jitter of a slow hand; a stroke stays a handful of points.
  if (!points.isEmpty() && (points.last() - QPointF(x, y)).manhattanLength() < 2) return;
  if (points.size() > 4000) return;
  points.append(QPointF(x, y));
  emit annotationChanged();
}

void Presenter::endStroke() {
  if (!m_drawing) return;
  m_drawing = false;
  if (!m_ink.isEmpty() && m_ink.last().points.size() < 2) m_ink.removeLast();
  emit annotationChanged();
}

void Presenter::undoInk() {
  const int slide = m_backend->currentSlide();
  for (int i = m_ink.size() - 1; i >= 0; --i)
    if (m_ink.at(i).slide == slide) {
      m_ink.removeAt(i);
      emit annotationChanged();
      return;
    }
}

void Presenter::clearInk() {
  if (m_ink.isEmpty()) return;
  m_ink.clear();
  m_drawing = false;
  emit annotationChanged();
}

bool Presenter::keepInkOnSlide() {
  const int slide = m_backend->currentSlide();
  QVector<QVector<QPointF>> strokes;
  for (const auto &stroke : m_ink)
    if (stroke.slide == slide && stroke.points.size() >= 2) strokes.append(stroke.points);
  if (strokes.isEmpty()) return false;
  bool kept = false;
  for (const auto &points : strokes) {
    QVariantList route;
    for (const auto &point : points) route.append(QVariantMap{{"x", point.x()}, {"y", point.y()}});
    if (m_backend->addPath(route, false, true)) kept = true;
  }
  if (!kept) return false;
  for (int i = m_ink.size() - 1; i >= 0; --i)
    if (m_ink.at(i).slide == slide) m_ink.removeAt(i);
  m_status = tr("Drawing kept on the slide");
  emit annotationChanged();
  emit stateChanged();
  return true;
}

// --- rehearsed timings -------------------------------------------------------
void Presenter::recordSlideTime() {
  if (!m_rehearsal || m_timedSlide < 0 || !m_slideClock.isValid()) return;
  m_timings[m_timedSlide] += m_slideClock.elapsed() / 1000.0;
  m_slideClock.restart();
  emit timingsChanged();
}

QVariantList Presenter::timings() const {
  QVariantList rows;
  const auto &document = m_backend->document();
  auto keys = m_timings.keys();
  std::sort(keys.begin(), keys.end());
  for (int index : keys) {
    if (index < 0 || index >= document.slides.size()) continue;
    rows.append(QVariantMap{{"index", index},
                            {"seconds", m_timings.value(index)},
                            {"current", Presentation::hold(document, index)},
                            {"skipped", document.slides.at(index).skipped}});
  }
  return rows;
}

void Presenter::clearTimings() {
  if (m_timings.isEmpty()) return;
  m_timings.clear();
  m_timedSlide = -1;
  emit timingsChanged();
}

bool Presenter::applyTimings() {
  QVariantList wanted;
  for (const auto &row : timings()) {
    const auto entry = row.toMap();
    // Rehearsed time covers the builds as well; the hold is what is left.
    const qreal builds = m_backend->document()
                             .slides.at(entry["index"].toInt())
                             .timeline.duration();
    wanted.append(QVariantMap{{"index", entry["index"]},
                              {"hold", qMax(qreal(0), entry["seconds"].toDouble() - builds)}});
  }
  if (wanted.isEmpty()) return false;
  if (!m_backend->applyRehearsedTimings(wanted)) return false;
  m_status = tr("Rehearsed timings applied");
  emit stateChanged();
  return true;
}
