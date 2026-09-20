#include "core/deckimport.h"
#include "core/deckresize.h"
#include "core/design.h"
#include "core/edit.h"
#include "render/textlayout.h"
#include <QFileInfo>
#include <QFontDatabase>
#include <QSet>
#include <QTransform>
#include <algorithm>

namespace {

// An imported name only changes when the deck already has that name, so a
// deck imported into an empty one reads exactly as it did.
QString freeName(const QString &name, QSet<QString> &taken) {
    QString candidate = name;
    if (taken.contains(candidate)) candidate = name + " (imported)";
    for (int n = 2; taken.contains(candidate); ++n)
        candidate = name + QString(" (imported %1)").arg(n);
    taken.insert(candidate);
    return candidate;
}

// Every identifier the target already uses. An imported id only changes when it
// would collide — so slides that morph into each other in the source still do
// here, and nothing starts morphing into a slide that is already open.
void collectIds(const QVector<SceneObject> &objects, QSet<QString> &ids) {
    for (const auto &o : objects) {
        ids.insert(o.id);
        for (const auto &group : o.groups) ids.insert(group);
        for (const auto &cell : o.table.cells) ids.insert(cell.id);
    }
}
QSet<QString> usedIds(const Document &d) {
    QSet<QString> ids;
    for (const auto &slide : d.slides) { ids.insert(slide.id); collectIds(slide.objects, ids); }
    for (const auto &master : d.masters) { ids.insert(master.id); collectIds(master.objects, ids); }
    for (const auto &layout : d.layouts) { ids.insert(layout.id); collectIds(layout.placeholders, ids); }
    for (const auto &style : d.objectStyles) { ids.insert(style.id); collectIds({style.appearance}, ids); }
    for (const auto &section : d.sections) ids.insert(section.id);
    return ids;
}

// A token the receiving theme does not define cannot be inherited, so the value
// it had in the source deck is baked in and the link dropped. Everything the
// theme does define keeps its link and follows the theme it lands in.
void keepOnlyKnownTokens(SceneObject &o, const DeckTheme &theme, const DeckTheme &source) {
    const auto colour = [&](QString &token, QColor &value) {
        if (token.isEmpty() || theme.colors.contains(token)) return;
        value = source.colors.value(token, value);
        token.clear();
    };
    colour(o.fillToken, o.fill);
    colour(o.textColorToken, o.textColor);
    colour(o.table.headerFillToken, o.table.headerFill);
    colour(o.table.borderColorToken, o.table.borderColor);
    if (!o.fontToken.isEmpty() && !theme.fonts.contains(o.fontToken)) {
        o.fontFamily = source.fonts.value(o.fontToken, o.fontFamily);
        o.fontToken.clear();
    }
}

bool rendersText(const SceneObject &o) {
    return o.type == ObjectType::Text || o.type == ObjectType::Table || o.type == ObjectType::Chart;
}

QString mediaKind(const SceneObject &o) {
    return o.mediaVideo ? QStringLiteral("video") : QStringLiteral("audio");
}

} // namespace

DeckImport::Result DeckImport::build(const Document &target, const Document &source,
                                    const QList<int> &slides, const QVariantMap &options) {
    Result result;
    result.document = target;
    const int mode = options.value("design", MatchByName).toInt();
    const bool importTheme = options.value("theme", false).toBool();
    const bool dropMissingMedia = options.value("dropMissingMedia", false).toBool();
    const QVariantMap substitutes = options.value("fonts").toMap();
    if (mode < MatchByName || mode > KeepAppearance) {
        result.error = "Choose how the design should come across.";
        return result;
    }
    QList<int> chosen;
    for (int index : slides) {
        if (index < 0 || index >= source.slides.size() || chosen.contains(index)) {
            result.error = "Choose slides that exist in that deck.";
            return result;
        }
        chosen.append(index);
    }
    std::sort(chosen.begin(), chosen.end());
    if (chosen.isEmpty()) {
        result.error = "Choose at least one slide to import.";
        return result;
    }
    if (chosen.size() + target.slides.size() > 2000) {
        result.error = "That would make a deck of more than 2000 slides. Import fewer at a time.";
        return result;
    }
    int after = options.value("after", target.slides.size() - 1).toInt();
    after = qBound(-1, after, int(target.slides.size()) - 1);

    // The theme the imported slides will actually be resolved against.
    const DeckTheme theme = importTheme ? source.theme : target.theme;

    // --- which masters and layouts come across -----------------------------
    QSet<QString> masterNames, layoutNames;
    for (const auto &m : target.masters) masterNames.insert(m.name);
    for (const auto &l : target.layouts) layoutNames.insert(l.name);
    QStringList needed;                     // source layout ids, in slide order
    for (int index : chosen) {
        const auto &id = source.slides.at(index).layoutId;
        if (!id.isEmpty() && Design::layout(source, id) && !needed.contains(id)) needed.append(id);
    }
    QHash<QString, QString> layoutFor, masterFor;   // source id -> target id
    QVector<Master> importedMasters;
    QVector<SlideLayout> importedLayouts;
    QVariantList masterRows, layoutRows;
    if (mode != KeepAppearance) {
        const auto importMaster = [&](const QString &sourceId) {
            if (masterFor.contains(sourceId)) return;
            const auto *m = Design::master(source, sourceId);
            if (!m) return;
            Master copy = *m;
            copy.id = Edit::newId("master");
            copy.name = freeName(m->name, masterNames);
            masterFor[sourceId] = copy.id;
            importedMasters.append(copy);
            masterRows.append(QVariantMap{{"name", copy.name}, {"action", "import"},
                                         {"detail", QString("%1 objects").arg(copy.objects.size())}});
        };
        for (const auto &sourceLayoutId : needed) {
            const auto *layout = Design::layout(source, sourceLayoutId);
            const auto *master = Design::master(source, layout->masterId);
            // The roles this import actually depends on.
            QMap<QString, ObjectType> roles;
            for (int index : chosen) {
                if (source.slides.at(index).layoutId != sourceLayoutId) continue;
                for (const auto &o : source.slides.at(index).objects)
                    if (!o.placeholderId.isEmpty()) roles[o.placeholderId] = o.type;
            }
            const SlideLayout *reuse = nullptr;
            QString reason;
            if (mode == MatchByName) {
                for (const auto &candidate : target.layouts) {
                    if (candidate.name != layout->name) continue;
                    const auto *candidateMaster = Design::master(target, candidate.masterId);
                    if (!master || !candidateMaster || candidateMaster->name != master->name) {
                        reason = "a layout of that name belongs to a different master";
                        continue;
                    }
                    bool compatible = true;
                    for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
                        bool found = false;
                        for (const auto &p : candidate.placeholders)
                            if (p.id == it.key() && p.type == it.value()) found = true;
                        if (!found) compatible = false;
                    }
                    if (!compatible) { reason = "its placeholders differ"; continue; }
                    reuse = &candidate;
                    break;
                }
            }
            if (reuse) {
                layoutFor[sourceLayoutId] = reuse->id;
                layoutRows.append(QVariantMap{{"name", reuse->name}, {"action", "reuse"},
                                              {"detail", "matched by name and placeholders"}});
                continue;
            }
            if (master) importMaster(layout->masterId);
            SlideLayout copy = *layout;
            copy.id = Edit::newId("layout");
            copy.name = freeName(layout->name, layoutNames);
            copy.masterId = masterFor.value(layout->masterId);
            layoutFor[sourceLayoutId] = copy.id;
            importedLayouts.append(copy);
            layoutRows.append(QVariantMap{{"name", copy.name}, {"action", "import"},
                                          {"detail", reason.isEmpty()
                                               ? QString("%1 placeholders").arg(copy.placeholders.size())
                                               : reason}});
        }
    }

    // --- the slides themselves ---------------------------------------------
    Document fragment;
    fragment.size = source.slides.isEmpty() ? target.size : source.size;
    fragment.theme = source.theme;
    fragment.masters = importedMasters;
    fragment.layouts = importedLayouts;
    QMap<QString, int> bakedFor;
    for (int index : chosen) {
        const Slide resolved = Design::resolve(source, index);
        Slide out = source.slides.at(index);
        const bool hadDesign = Design::layout(source, out.layoutId) != nullptr;
        const bool bake = hadDesign && !layoutFor.contains(out.layoutId);
        out.objects.clear();
        for (const auto &local : source.slides.at(index).objects) {
            SceneObject o = local;
            if (bake) {
                if (const auto *shown = resolved.find(local.id)) o = Design::detached(*shown);
                else { o.placeholderId.clear(); o.overrides.clear(); }
            }
            out.objects.append(o);
        }
        if (bake) {
            // Without its master the slide keeps the look it had, literally.
            out.layoutId.clear();
            out.background = resolved.background;
            out.backgroundOverride = true;
            ++bakedFor[QStringLiteral("slides")];
        } else {
            out.layoutId = layoutFor.value(out.layoutId);
        }
        fragment.slides.append(out);
    }
    for (auto &slide : fragment.slides)
        for (auto &o : slide.objects) keepOnlyKnownTokens(o, theme, source.theme);
    for (auto &master : fragment.masters) {
        for (auto &o : master.objects) keepOnlyKnownTokens(o, theme, source.theme);
        if (!master.backgroundToken.isEmpty() && !theme.colors.contains(master.backgroundToken)) {
            master.background = source.theme.colors.value(master.backgroundToken, master.background);
            master.backgroundToken.clear();
        }
    }
    for (auto &layout : fragment.layouts)
        for (auto &o : layout.placeholders) keepOnlyKnownTokens(o, theme, source.theme);

    // A different slide size is scaled the same way a deck resize scales it.
    const bool scaled = DeckResize::apply(fragment, target.size, true);

    // --- fonts --------------------------------------------------------------
    QMap<QString, int> families;
    const auto want = [&families](const SceneObject &o) {
        if (rendersText(o) && o.fontToken.isEmpty() && !o.fontFamily.isEmpty())
            ++families[o.fontFamily];
    };
    for (const auto &slide : fragment.slides) for (const auto &o : slide.objects) want(o);
    for (const auto &master : fragment.masters) for (const auto &o : master.objects) want(o);
    for (const auto &layout : fragment.layouts) for (const auto &o : layout.placeholders) want(o);
    if (importTheme)
        for (const auto &family : source.theme.fonts) ++families[family];
    QVariantList fontRows;
    QMap<QString, QString> replacement;
    int unresolvedFonts = 0;
    for (auto it = families.cbegin(); it != families.cend(); ++it) {
        const bool available = QFontDatabase::hasFamily(it.key());
        const QString chosenSubstitute = substitutes.value(it.key()).toString();
        bool resolved = available;
        QString detail;
        if (!available) {
            if (chosenSubstitute == QLatin1String("keep")) {
                resolved = true;
                detail = "kept — the system will substitute a typeface";
            } else if (chosenSubstitute.isEmpty()) {
                detail = "not installed — choose a replacement";
            } else if (!QFontDatabase::hasFamily(chosenSubstitute)) {
                detail = "the chosen replacement is not installed either";
            } else {
                resolved = true;
                replacement[it.key()] = chosenSubstitute;
                detail = "replaced by " + chosenSubstitute;
            }
            if (!resolved) ++unresolvedFonts;
        }
        fontRows.append(QVariantMap{{"family", it.key()}, {"available", available},
                                    {"substitute", chosenSubstitute}, {"uses", it.value()},
                                    {"resolved", resolved}, {"detail", detail}});
    }
    if (!replacement.isEmpty()) {
        const auto swap = [&replacement](SceneObject &o) {
            if (replacement.contains(o.fontFamily)) o.fontFamily = replacement.value(o.fontFamily);
        };
        for (auto &slide : fragment.slides) for (auto &o : slide.objects) swap(o);
        for (auto &master : fragment.masters) for (auto &o : master.objects) swap(o);
        for (auto &layout : fragment.layouts) for (auto &o : layout.placeholders) swap(o);
        if (importTheme)
            for (auto it = fragment.theme.fonts.begin(); it != fragment.theme.fonts.end(); ++it)
                if (replacement.contains(it.value())) it.value() = replacement.value(it.value());
    }

    // --- media and linked data ---------------------------------------------
    QVariantList mediaRows;
    QSet<QString> countedAssets;
    qint64 bytes = 0;
    int missingMedia = 0, droppedMedia = 0;
    for (auto &slide : fragment.slides) {
        QStringList dropped;
        for (int i = 0; i < slide.objects.size(); ++i) {
            auto &o = slide.objects[i];
            const auto countAsset = [&countedAssets, &bytes](const QString &id, qint64 size) {
                if (id.isEmpty() || countedAssets.contains(id)) return;
                countedAssets.insert(id);
                bytes += size;
            };
            countAsset(o.imageId, o.imageData.size());
            if (o.imageOriginal) countAsset(o.imageOriginal->imageId, o.imageOriginal->imageData.size());
            if (o.mediaOriginal) countAsset(o.mediaOriginal->mediaId, o.mediaOriginal->mediaData.size());
            if (o.type != ObjectType::Media) {
                if (!o.dataSource.path.isEmpty())
                    mediaRows.append(QVariantMap{
                        {"name", QFileInfo(o.dataSource.path).fileName()}, {"kind", "data"},
                        {"bytes", 0}, {"linked", true}, {"dropped", false},
                        {"missing", !QFileInfo(o.dataSource.path).isReadable()},
                        {"detail", "linked data keeps the values saved in that deck"}});
                continue;
            }
            const bool linked = !o.mediaPath.isEmpty();
            const bool missing = linked && !QFileInfo(o.mediaPath).isReadable();
            if (!linked) countAsset(o.mediaId, o.mediaData.size());
            // Read permission and playback position are session state, never imported.
            o.mediaReadAllowed = false;
            o.mediaActive = false;
            o.mediaPosition = -1;
            mediaRows.append(QVariantMap{
                {"name", o.mediaName.isEmpty() ? QFileInfo(o.mediaPath).fileName() : o.mediaName},
                {"kind", mediaKind(o)}, {"bytes", linked ? o.mediaBytes : qint64(o.mediaData.size())},
                {"linked", linked}, {"missing", missing}, {"dropped", missing && dropMissingMedia},
                {"detail", missing ? (dropMissingMedia ? "missing — left out of the import"
                                                       : "missing — decide what should happen to it")
                                   : linked ? "linked to a file on this computer" : "embedded in the deck"}});
            if (!missing) continue;
            ++missingMedia;
            if (dropMissingMedia) dropped.append(o.id);
        }
        if (dropped.isEmpty()) continue;
        for (int i = slide.objects.size() - 1; i >= 0; --i)
            if (dropped.contains(slide.objects.at(i).id)) { slide.objects.removeAt(i); ++droppedMedia; }
        for (int i = slide.timeline.steps.size() - 1; i >= 0; --i)
            if (dropped.contains(slide.timeline.steps.at(i).targetId))
                slide.timeline.steps.removeAt(i);
    }

    // --- identities ---------------------------------------------------------
    auto taken = usedIds(target);
    QHash<QString, QString> ids;
    const auto rename = [&ids, &taken](const QString &old, const char *prefix) {
        if (old.isEmpty() || !taken.contains(old)) return old;
        if (!ids.contains(old)) {
            QString next = Edit::newId(prefix);
            while (taken.contains(next)) next = Edit::newId(prefix);
            taken.insert(next);
            ids[old] = next;
        }
        return ids.value(old);
    };
    for (auto &slide : fragment.slides) {
        slide.id = rename(slide.id, "slide");
        for (auto &o : slide.objects) {
            o.id = rename(o.id, "object");
            for (auto &group : o.groups) group = rename(group, "group");
            QMap<QString, QColor> colors;
            for (auto &cell : o.table.cells) {
                const auto old = cell.id;
                cell.id = rename(cell.id, "cell");
                if (o.chart.seriesColors.contains(old)) colors[cell.id] = o.chart.seriesColors.value(old);
            }
            if (!o.chart.seriesColors.isEmpty()) o.chart.seriesColors = colors;
        }
        for (auto &step : slide.timeline.steps) step.targetId = ids.value(step.targetId, step.targetId);
        for (auto &o : slide.objects) {
            if (!o.connector) continue;
            o.connectorFrom = ids.value(o.connectorFrom, o.connectorFrom);
            o.connectorTo = ids.value(o.connectorTo, o.connectorTo);
        }
    }

    // --- sections -----------------------------------------------------------
    QStringList newSections;
    QHash<QString, QString> sectionFor;
    for (auto &slide : fragment.slides) {
        const auto sourceId = slide.sectionId;
        if (sourceId.isEmpty()) continue;
        if (!sectionFor.contains(sourceId)) {
            QString name;
            for (const auto &section : source.sections)
                if (section.id == sourceId) name = section.name;
            if (name.isEmpty()) { sectionFor[sourceId] = QString(); }
            else {
                QString existing;
                for (const auto &section : result.document.sections)
                    if (section.name == name) existing = section.id;
                if (existing.isEmpty()) {
                    Section section{rename(Edit::newId("section"), "section"), name};
                    result.document.sections.append(section);
                    newSections.append(name);
                    existing = section.id;
                }
                sectionFor[sourceId] = existing;
            }
        }
        slide.sectionId = sectionFor.value(sourceId);
    }

    // --- merge --------------------------------------------------------------
    if (importTheme) result.document.theme = fragment.theme;
    result.document.masters += fragment.masters;
    result.document.layouts += fragment.layouts;
    for (int i = 0; i < fragment.slides.size(); ++i) {
        result.document.slides.insert(after + 1 + i, fragment.slides.at(i));
        result.slideIds.append(fragment.slides.at(i).id);
    }

    // --- the report ---------------------------------------------------------
    QVariantList slideRows;
    for (int i = 0; i < fragment.slides.size(); ++i) {
        const int index = after + 1 + i;
        const Slide shown = Design::resolve(result.document, index);
        QString title;
        int overflow = 0, outside = 0;
        for (const auto &o : shown.objects) {
            if (o.hidden) continue;
            if (title.isEmpty() && o.type == ObjectType::Text && !o.text.trimmed().isEmpty() &&
                !o.id.startsWith(QStringLiteral("@field/")))
                title = o.text.section('\n', 0, 0);
            if (o.type == ObjectType::Text && TextLayout::measure(o).overflow) ++overflow;
            QTransform transform;
            const auto centre = o.rect.center();
            transform.translate(centre.x(), centre.y());
            transform.rotate(o.rotation);
            transform.translate(-centre.x(), -centre.y());
            if (!QRectF(QPointF(), result.document.size).contains(transform.mapRect(o.rect))) ++outside;
        }
        const auto *layout = Design::layout(result.document, fragment.slides.at(i).layoutId);
        QStringList issues;
        if (overflow) issues.append(QString("%1 text boxes overflow at this size.").arg(overflow));
        if (outside) issues.append(QString("%1 objects extend beyond the slide.").arg(outside));
        slideRows.append(QVariantMap{
            {"source", chosen.at(i)}, {"index", index}, {"id", fragment.slides.at(i).id},
            {"name", QString("Slide %1").arg(chosen.at(i) + 1)},
            {"title", title.isEmpty() ? QStringLiteral("Untitled slide") : title},
            {"layout", layout ? layout->name : QStringLiteral("No layout · own appearance")},
            {"objects", fragment.slides.at(i).objects.size()},
            {"overflow", overflow}, {"outside", outside}, {"issues", issues}});
    }
    QVariantList themeRows;
    if (importTheme) {
        const auto describe = [&](const QString &token, const QString &from, const QString &to) {
            if (from == to) return;
            themeRows.append(QVariantMap{{"token", token}, {"from", from}, {"to", to}});
        };
        for (auto it = source.theme.colors.cbegin(); it != source.theme.colors.cend(); ++it)
            describe(it.key(), target.theme.colors.value(it.key()).name(QColor::HexRgb),
                     it.value().name(QColor::HexRgb));
        for (auto it = fragment.theme.fonts.cbegin(); it != fragment.theme.fonts.cend(); ++it)
            describe(it.key(), target.theme.fonts.value(it.key()), it.value());
    }
    result.plan = QVariantMap{
        {"slides", slideRows}, {"masters", masterRows}, {"layouts", layoutRows},
        {"fonts", fontRows}, {"media", mediaRows}, {"theme", themeRows},
        {"sections", newSections}, {"design", mode}, {"importTheme", importTheme},
        {"scaled", scaled}, {"after", after}, {"bytes", bytes},
        {"sourceSize", QString("%1 × %2").arg(source.size.width()).arg(source.size.height())},
        {"themeName", source.theme.name}, {"missingFonts", unresolvedFonts},
        {"missingMedia", missingMedia}, {"droppedMedia", droppedMedia},
        {"baked", bakedFor.value(QStringLiteral("slides"))}};
    if (unresolvedFonts > 0) {
        result.error = unresolvedFonts == 1
            ? QStringLiteral("One typeface is missing. Choose a replacement, or keep the name.")
            : QString("%1 typefaces are missing. Choose replacements, or keep the names.").arg(unresolvedFonts);
    } else if (missingMedia > 0 && !dropMissingMedia) {
        result.error = missingMedia == 1
            ? QStringLiteral("One linked file is missing. Leave it out, or relink it in that deck first.")
            : QString("%1 linked files are missing. Leave them out, or relink them in that deck first.").arg(missingMedia);
    }
    if (!result.error.isEmpty()) {
        result.document = target;
        result.slideIds.clear();
    }
    result.plan["ok"] = result.error.isEmpty();
    result.plan["error"] = result.error;
    return result;
}
