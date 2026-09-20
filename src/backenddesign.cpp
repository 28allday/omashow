#include "backend.h"
#include "core/design.h"
#include "core/edit.h"
#include <QDate>

QVariantMap Backend::design() const {
  QVariantMap colors, fonts;
  for (auto it = m_document.theme.colors.cbegin();
       it != m_document.theme.colors.cend(); ++it)
    colors[it.key()] = it.value().name(it.value().alpha() == 255 ? QColor::HexRgb : QColor::HexArgb);
  for (auto it = m_document.theme.fonts.cbegin();
       it != m_document.theme.fonts.cend(); ++it)
    fonts[it.key()] = it.value();
  QVariantList masters, layouts;
  int linked = 0;
  for (const auto &s : m_document.slides)
    if (!s.layoutId.isEmpty())
      ++linked;
  for (const auto &m : m_document.masters) {
    int uses = 0, slides = 0;
    for (const auto &l : m_document.layouts)
      if (l.masterId == m.id) {
        ++uses;
        for (const auto &s : m_document.slides)
          if (s.layoutId == l.id)
            ++slides;
      }
    masters.append(
        QVariantMap{{"id", m.id},
                    {"name", m.name},
                    {"layouts", uses},
                    {"slides", slides},
                    {"background", m_document.theme.colors
                                       .value(m.backgroundToken, m.background)
                                       .name(QColor::HexRgb)},
                    {"backgroundToken", m.backgroundToken},
                    {"fields", Design::fieldProperties(m.fields)}});
  }
  for (const auto &l : m_document.layouts) {
    int uses = 0;
    QVariantList placeholders;
    for (const auto &s : m_document.slides)
      if (s.layoutId == l.id)
        ++uses;
    for (const auto &p : l.placeholders)
      placeholders.append(
          Design::properties(Design::themed(m_document.theme, p)));
    layouts.append(QVariantMap{{"id", l.id},
                               {"name", l.name},
                               {"masterId", l.masterId},
                               {"slides", uses},
                               {"placeholders", placeholders}});
  }
  return {{"name", m_document.theme.name},
          {"contrast", Design::themeContrast(m_document.theme)},
          {"colors", colors},
          {"fonts", fonts},
          {"masters", masters},
          {"layouts", layouts},
          {"linkedSlides", linked}};
}
QVariantMap Backend::slideDesign() const {
  if (m_currentSlide < 0 || m_currentSlide >= m_document.slides.size())
    return {};
  const auto &s = m_document.slides.at(m_currentSlide);
  const auto *l = Design::layout(m_document, s.layoutId);
  return {{"layoutId", s.layoutId},
          {"layoutName", l ? l->name : tr("Freeform")},
          {"showMasterObjects", s.showMasterObjects},
          {"showMasterFields", s.showMasterFields},
          {"hasMaster", l && Design::master(m_document, l->masterId)},
          {"background", Design::resolve(m_document, m_currentSlide)
                             .background.name(QColor::HexRgb)},
          {"backgroundOverride", s.backgroundOverride}};
}
void Backend::setupDesign() {
  if (!m_document.layouts.isEmpty())
    return;
  m_history.begin(m_document, tr("Add layouts"));
  Design::ensureDefaults(m_document);
  m_history.commit();
  touch();
}
void Backend::applyTheme(int index) {
  if (index < 0 || index > 2)
    return;
  m_history.begin(m_document, tr("Change theme"));
  m_document.theme = Design::preset(index);
  Design::ensureDefaults(m_document);
  m_history.commit();
  touch();
}
void Backend::setThemeToken(const QString &key, const QString &value,
                            bool font) {
  if (font
          ? (!m_document.theme.fonts.contains(key) || value.trimmed().isEmpty())
          : (!m_document.theme.colors.contains(key) ||
             !QColor(value).isValid()))
    return;
  if (font ? m_document.theme.fonts.value(key) == value
           : m_document.theme.colors.value(key) == QColor(value))
    return;
  m_history.begin(m_document, tr("Change theme %1").arg(key));
  if (font)
    m_document.theme.fonts[key] = value;
  else
    m_document.theme.colors[key] = QColor(value);
  m_history.commit();
  touch();
}
void Backend::applyLayout(const QString &id) {
  if (!Design::layout(m_document, id))
    return;
  m_history.begin(m_document, tr("Apply layout"));
  if (!Design::applyLayout(m_document, m_currentSlide, id)) {
    m_history.abandon();
    return;
  }
  m_history.commit();
  touch();
}
void Backend::setSlideBackground(const QString &color, bool reset) {
  if (m_document.slides.isEmpty() || (!reset && !QColor(color).isValid()))
    return;
  m_history.begin(m_document, tr("Change slide background"));
  auto &s = m_document.slides[m_currentSlide];
  s.backgroundOverride = !reset;
  if (!reset)
    s.background = QColor(color);
  m_history.commit();
  touch();
}
void Backend::resetPlaceholder(bool geometry) {
  const auto *p = static_cast<const Backend *>(this)->selectedObject();
  if (!p || p->placeholderId.isEmpty())
    return;
  m_history.begin(m_document, geometry ? tr("Reset placeholder position")
                                       : tr("Reset placeholder style"));
  Design::reset(*selectedObject(), geometry);
  m_history.commit();
  touch();
}
QString Backend::addMaster(const QString &copyId) {
  const auto *source = Design::master(m_document, copyId);
  if (!copyId.isEmpty() && !source)
    return {};
  Master m = source ? *source : Master();
  m.id = Edit::newId("master");
  m.name = source ? tr("%1 copy").arg(source->name) : tr("New master");
  m_history.begin(m_document, tr("Add master"));
  m_document.masters.append(m);
  m_history.commit();
  touch();
  return m.id;
}
void Backend::setMasterProperty(const QString &id, const QString &key,
                                const QString &value) {
  for (int i = 0; i < m_document.masters.size(); ++i)
    if (m_document.masters.at(i).id == id) {
      if ((key == "name" && value.trimmed().isEmpty()) ||
          (key == "background" && !QColor(value).isValid()))
        return;
      if (key != "name" && key != "background" && key != "backgroundToken")
        return;
      if (key == "backgroundToken" && !value.isEmpty() &&
          !m_document.theme.colors.contains(value))
        return;
      m_history.begin(m_document, tr("Edit master"));
      auto &m = m_document.masters[i];
      if (key == "name")
        m.name = value;
      if (key == "background") {
        m.background = QColor(value);
        m.backgroundToken.clear();
      }
      if (key == "backgroundToken")
        m.backgroundToken = value;
      m_history.commit();
      touch();
      return;
    }
}
void Backend::moveMaster(const QString &id, int delta) {
  for (int i = 0; i < m_document.masters.size(); ++i)
    if (m_document.masters.at(i).id == id) {
      const int to = qBound(0, i + delta, int(m_document.masters.size()) - 1);
      if (to == i)
        return;
      m_history.begin(m_document, tr("Reorder masters"));
      m_document.masters.move(i, to);
      m_history.commit();
      touch();
      return;
    }
}
bool Backend::deleteMaster(const QString &id, const QString &replacement) {
  if (!Design::master(m_document, id))
    return false;
  bool used = false;
  for (const auto &l : m_document.layouts)
    if (l.masterId == id)
      used = true;
  if (used && (replacement == id || !Design::master(m_document, replacement))) {
    setStatus(tr("Choose a replacement for the layouts using this master."));
    return false;
  }
  m_history.begin(m_document, tr("Delete master"));
  for (auto &l : m_document.layouts)
    if (l.masterId == id)
      l.masterId = replacement;
  for (int i = 0; i < m_document.masters.size(); ++i)
    if (m_document.masters.at(i).id == id) {
      m_document.masters.removeAt(i);
      break;
    }
  m_history.commit();
  touch();
  return true;
}
QString Backend::addLayout(const QString &masterId, const QString &copyId) {
  if (!Design::master(m_document, masterId))
    return {};
  const auto *source = Design::layout(m_document, copyId);
  if (!copyId.isEmpty() && !source)
    return {};
  SlideLayout l = source ? *source : SlideLayout();
  l.id = Edit::newId("layout");
  l.masterId = masterId;
  l.name = source ? tr("%1 copy").arg(source->name) : tr("New layout");
  m_history.begin(m_document, tr("Add layout"));
  m_document.layouts.append(l);
  m_history.commit();
  touch();
  return l.id;
}
void Backend::setLayoutProperty(const QString &id, const QString &key,
                                const QString &value) {
  if ((key == "name" && value.trimmed().isEmpty()) ||
      (key == "masterId" && !Design::master(m_document, value)))
    return;
  if (key != "name" && key != "masterId")
    return;
  for (int i = 0; i < m_document.layouts.size(); ++i)
    if (m_document.layouts.at(i).id == id) {
      m_history.begin(m_document, tr("Edit layout"));
      if (key == "name")
        m_document.layouts[i].name = value;
      else
        m_document.layouts[i].masterId = value;
      m_history.commit();
      touch();
      return;
    }
}
bool Backend::deleteLayout(const QString &id, const QString &replacement) {
  if (!Design::layout(m_document, id))
    return false;
  bool used = false;
  for (const auto &s : m_document.slides)
    if (s.layoutId == id)
      used = true;
  if (used && (replacement == id || !Design::layout(m_document, replacement))) {
    setStatus(tr("Choose a replacement for the slides using this layout."));
    return false;
  }
  m_history.begin(m_document, tr("Delete layout"));
  for (int i = 0; i < m_document.slides.size(); ++i)
    if (m_document.slides.at(i).layoutId == id)
      Design::applyLayout(m_document, i, replacement);
  for (int i = 0; i < m_document.layouts.size(); ++i)
    if (m_document.layouts.at(i).id == id) {
      m_document.layouts.removeAt(i);
      break;
    }
  m_history.commit();
  touch();
  return true;
}
void Backend::setPlaceholderProperty(const QString &id, const QString &role,
                                     const QString &key,
                                     const QVariant &value) {
  for (int i = 0; i < m_document.layouts.size(); ++i)
    if (m_document.layouts.at(i).id == id) {
      const auto l = m_document.layouts.at(i);
      for (int j = 0; j < l.placeholders.size(); ++j)
        if (l.placeholders.at(j).id == role) {
          SceneObject changed = l.placeholders.at(j);
          if (!Design::setProperty(changed, key, value))
            return;
          if ((key == "fillToken" || key == "textColorToken") &&
              !value.toString().isEmpty() &&
              !m_document.theme.colors.contains(value.toString()))
            return;
          if (key == "fontToken" && !value.toString().isEmpty() &&
              !m_document.theme.fonts.contains(value.toString()))
            return;
          Design::markOverride(changed, key);
          m_history.begin(m_document, tr("Edit placeholder"));
          m_document.layouts[i].placeholders[j] = changed;
          m_history.commit();
          touch();
          return;
        }
    }
}
void Backend::addPlaceholder(const QString &id, bool shape) {
  for (int i = 0; i < m_document.layouts.size(); ++i)
    if (m_document.layouts.at(i).id == id) {
      SceneObject p;
      p.id = Edit::newId(shape ? "shape" : "text");
      p.type = shape ? ObjectType::Rect : ObjectType::Text;
      p.text = tr("Your text");
      p.rect =
          QRectF(m_document.size.width() * .1, m_document.size.height() * .4,
                 m_document.size.width() * .8, m_document.size.height() * .2);
      p.fontToken = "body";
      p.textColorToken = "foreground";
      p.fillToken = "accent";
      m_history.begin(m_document, tr("Add placeholder"));
      m_document.layouts[i].placeholders.append(p);
      for (auto &s : m_document.slides)
        if (s.layoutId == id) {
          SceneObject o = p;
          o.id = Edit::newId("placeholder");
          o.placeholderId = p.id;
          s.objects.append(o);
        }
      m_history.commit();
      touch();
      return;
    }
}
void Backend::movePlaceholder(const QString &id, const QString &role,
                              int delta) {
  for (int i = 0; i < m_document.layouts.size(); ++i)
    if (m_document.layouts.at(i).id == id) {
      const auto before = m_document.layouts.at(i).placeholders;
      for (int j = 0; j < before.size(); ++j)
        if (before.at(j).id == role) {
          const int to = qBound(0, j + delta, int(before.size()) - 1);
          if (j == to)
            return;
          m_history.begin(m_document, tr("Reorder placeholders"));
          auto &placeholders = m_document.layouts[i].placeholders;
          placeholders.move(j, to);
          for (auto &s : m_document.slides)
            if (s.layoutId == id) {
              QVector<int> positions;
              QMap<QString, SceneObject> byRole;
              for (int k = 0; k < s.objects.size(); ++k)
                if (!s.objects.at(k).placeholderId.isEmpty()) {
                  positions.append(k);
                  byRole[s.objects.at(k).placeholderId] = s.objects.at(k);
                }
              int position = 0;
              for (const auto &p : placeholders)
                if (byRole.contains(p.id))
                  s.objects[positions.at(position++)] = byRole.value(p.id);
            }
          m_history.commit();
          touch();
          return;
        }
    }
}
void Backend::deletePlaceholder(const QString &id, const QString &role) {
  const auto *l = Design::layout(m_document, id);
  if (!l)
    return;
  int position = -1;
  for (int i = 0; i < l->placeholders.size(); ++i)
    if (l->placeholders.at(i).id == role)
      position = i;
  if (position < 0)
    return;
  m_history.begin(m_document, tr("Remove placeholder"));
  for (int i = 0; i < m_document.slides.size(); ++i) {
    if (m_document.slides.at(i).layoutId != id)
      continue;
    const Slide resolved = Design::resolve(m_document, i);
    for (auto &o : m_document.slides[i].objects)
      if (o.placeholderId == role)
        if (const auto *shown = resolved.find(o.id))
          o = Design::detached(*shown);
  }
  for (auto &layout : m_document.layouts)
    if (layout.id == id)
      layout.placeholders.removeAt(position);
  m_history.commit();
  touch();
}
QVariantList Backend::navigator() const {
  if (m_navigatorRevision == m_revision) return m_navigator;
  QVariantList list;
  QString previous;
  for (int i = 0; i < m_document.slides.size(); ++i) {
    const auto &s = m_document.slides.at(i);
    QString name;
    for (const auto &section : m_document.sections)
      if (section.id == s.sectionId)
        name = section.name;
    QStringList outline;
    int pictures=0,media=0,openComments=0;
    for(const auto &comment:m_document.comments)
      if(comment.slideId==s.id && comment.parentId.isEmpty() && !comment.resolved) ++openComments;
    for(const auto &o:Design::resolve(m_document,i).objects) {
      if(o.hidden || o.id.startsWith("@field/")) continue;
      if(o.type==ObjectType::Text && !o.text.trimmed().isEmpty()) outline.append(o.text);
      if(o.type==ObjectType::Image) ++pictures;
      if(o.type==ObjectType::Media) ++media;
    }
    list.append(
        QVariantMap{{"index", i},
                    {"title",outline.isEmpty()?tr("Untitled slide"):outline.first().section('\n',0,0)},
                    {"outline",outline.join('\n')},
                    {"skipped",s.skipped},
                    {"notes",!s.notes.trimmed().isEmpty()},
                    {"buildCount",s.timeline.steps.size()},
                    {"imageCount",pictures},{"mediaCount",media},{"commentCount",openComments},
                    {"id", s.id},
                    {"sectionId", s.sectionId},
                    {"sectionName", name},
                    {"sectionStart", !s.sectionId.isEmpty() &&
                                         (i == 0 || previous != s.sectionId)}});
    previous = s.sectionId;
  }
  m_navigator = list;
  m_navigatorRevision = m_revision;
  return list;
}
void Backend::startSection(const QString &name) {
  if (name.trimmed().isEmpty() || m_document.slides.isEmpty())
    return;
  const QString previous = m_document.slides.at(m_currentSlide).sectionId;
  Section section{Edit::newId("section"), name.trimmed()};
  m_history.begin(m_document, tr("Start section"));
  m_document.sections.append(section);
  for (int i = m_currentSlide; i < m_document.slides.size(); ++i) {
    if (m_document.slides.at(i).sectionId != previous)
      break;
    m_document.slides[i].sectionId = section.id;
  }
  m_history.commit();
  touch();
}
void Backend::renameSection(const QString &id, const QString &name) {
  if (name.trimmed().isEmpty())
    return;
  for (int i = 0; i < m_document.sections.size(); ++i)
    if (m_document.sections.at(i).id == id) {
      if(m_document.sections.at(i).name == name.trimmed()) return;
      m_history.begin(m_document, tr("Rename section"));
      m_document.sections[i].name = name.trimmed();
      m_history.commit();
      touch();
      return;
    }
}
void Backend::removeSection(const QString &id) {
  for (int i = 0; i < m_document.sections.size(); ++i)
    if (m_document.sections.at(i).id == id) {
      m_history.begin(m_document, tr("Remove section"));
      m_document.sections.removeAt(i);
      for (auto &s : m_document.slides)
        if (s.sectionId == id)
          s.sectionId.clear();
      m_history.commit();
      touch();
      return;
    }
}

void Backend::setMasterField(const QString &masterId, const QString &key, const QVariant &value) {
  for (int index = 0; index < m_document.masters.size(); ++index) {
    const auto master = m_document.masters.at(index);
    if (master.id != masterId) continue;
    auto fields = master.fields;
    if (!Design::setFieldProperty(fields, key, value)) return;
    if (key == "showDate" && fields.showDate && fields.date.isEmpty())
      fields.date = QDate::currentDate().toString(Qt::ISODate);
    if (Design::fieldProperties(fields) == Design::fieldProperties(master.fields)) return;
    m_history.begin(m_document, tr("Change master fields"));
    m_document.masters[index].fields = fields;
    m_history.commit();
    touch();
    return;
  }
}
