#include "core/chart.h"
#include "core/table.h"
#include "anim/morph.h"
#include "core/imageasset.h"

#include "core/scene.h"
#include "core/connector.h"

#include <QEasingCurve>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace {

qreal lerp(qreal a, qreal b, qreal u) { return a + (b - a) * u; }

QRectF lerpRect(const QRectF &a, const QRectF &b, qreal u) {
    return QRectF(lerp(a.x(), b.x(), u), lerp(a.y(), b.y(), u),
                  lerp(a.width(), b.width(), u), lerp(a.height(), b.height(), u));
}

QColor lerpColor(const QColor &a, const QColor &b, qreal u) {
    return QColor::fromRgbF(lerp(a.redF(), b.redF(), u),
                            lerp(a.greenF(), b.greenF(), u),
                            lerp(a.blueF(), b.blueF(), u),
                            lerp(a.alphaF(), b.alphaF(), u));
}

// Similarity of two sizes, 1.0 when identical, falling to 0 as they diverge.
qreal sizeSimilarity(const QRectF &a, const QRectF &b) {
    const qreal areaA = std::max(1.0, std::abs(a.width() * a.height()));
    const qreal areaB = std::max(1.0, std::abs(b.width() * b.height()));
    return std::min(areaA, areaB) / std::max(areaA, areaB);
}

qreal colorSimilarity(const QColor &a, const QColor &b) {
    const qreal dr = a.redF() - b.redF();
    const qreal dg = a.greenF() - b.greenF();
    const qreal db = a.blueF() - b.blueF();
    const qreal distance = std::sqrt(dr * dr + dg * dg + db * db) / std::sqrt(3.0);
    return 1.0 - distance;
}

SceneObject interpolate(const SceneObject &from, const SceneObject &to, qreal u) {
    SceneObject state = to;
    state.rect = lerpRect(from.rect, to.rect, u);
    state.imageCrop = lerpRect(from.imageCrop,to.imageCrop,u);
    state.imageFocalX=lerp(from.imageFocalX,to.imageFocalX,u); state.imageFocalY=lerp(from.imageFocalY,to.imageFocalY,u);
    state.imageBrightness=lerp(from.imageBrightness,to.imageBrightness,u); state.imageContrast=lerp(from.imageContrast,to.imageContrast,u); state.imageSaturation=lerp(from.imageSaturation,to.imageSaturation,u);
    state.imageTint=lerpColor(from.imageTint,to.imageTint,u); state.imageTintAmount=lerp(from.imageTintAmount,to.imageTintAmount,u);
    if(from.connector && to.connector) {
        if(from.connectorFrom!=to.connectorFrom) { state.connectorFrom.clear(); state.connectorStart=from.connectorStart+(to.connectorStart-from.connectorStart)*u; }
        if(from.connectorTo!=to.connectorTo) { state.connectorTo.clear(); state.connectorEnd=from.connectorEnd+(to.connectorEnd-from.connectorEnd)*u; }
    }
    state.rotation = lerp(from.rotation, to.rotation, u);
    state.opacity = lerp(from.hidden ? 0.0 : from.opacity, to.hidden ? 0.0 : to.opacity, u);
    state.hidden = from.hidden && to.hidden;
    state.cornerRadius = lerp(from.cornerRadius, to.cornerRadius, u);
    state.fill = lerpColor(from.fill, to.fill, u);
    state.strokeWidth = lerp(from.strokeWidth,to.strokeWidth,u);
    state.strokeColor = lerpColor(from.strokeColor,to.strokeColor,u);
    state.fillSecondary = lerpColor(from.fillSecondary,to.fillSecondary,u);
    state.fillAngle = lerp(from.fillAngle,to.fillAngle,u);
    state.shadowX = lerp(from.shadowX,to.shadowX,u); state.shadowY = lerp(from.shadowY,to.shadowY,u);
    state.shadowColor = lerpColor(from.shadowColor,to.shadowColor,u);
    state.textColor = lerpColor(from.textColor, to.textColor, u);

    // Identical text interpolates its metrics. Differing text would cross-fade
    // inside an interpolating box — Gate 0 keeps the incoming string and moves
    // the box, which is the same motion without the per-glyph work.
    state.fontSize = lerp(from.fontSize, to.fontSize, u);
    state.letterSpacing = lerp(from.letterSpacing, to.letterSpacing, u);
    return state;
}

} // namespace

qreal Morph::scorePair(const SceneObject &from, const SceneObject &to) {
    if (from.type != to.type)
        return 0.0;

    // Same words are the strongest signal a text object is "the same thing"
    // wearing a different size or position.
    if (from.type == ObjectType::Text) {
        if (from.text == to.text)
            return 1.0;
        if (from.text.isEmpty() || to.text.isEmpty())
            return 0.0;
        return 0.35 * sizeSimilarity(from.rect, to.rect);
    }

    if (from.type == ObjectType::Chart) return Table::encode(from.table)==Table::encode(to.table) && Chart::encode(from.chart)==Chart::encode(to.chart) ? 1.0 : 0.0;
    if (from.type == ObjectType::Table) return Table::encode(from.table)==Table::encode(to.table) ? 1.0 : 0.0;
    if (from.type == ObjectType::Media) return !from.mediaId.isEmpty() && from.mediaId == to.mediaId ? 1.0 : 0.0;
    if (from.type == ObjectType::Image) return !from.imageId.isEmpty() && from.imageId == to.imageId ? 1.0 : 0.0;

    if(from.shapeKind!=to.shapeKind || from.pathData!=to.pathData) return 0;
    return 0.6 * sizeSimilarity(from.rect, to.rect)
         + 0.4 * colorSimilarity(from.fill, to.fill);
}

QVector<MorphPair> Morph::match(const Slide &from, const Slide &to) {
    QVector<MorphPair> pairs;
    QSet<QString> takenFrom;
    QSet<QString> takenTo;

    // Tier 1 — identity.
    for (const SceneObject &a : from.objects) {
        if (to.find(a.id)) {
            pairs.append({a.id, a.id, MatchKind::Identity, 1.0});
            takenFrom.insert(a.id);
            takenTo.insert(a.id);
        }
    }

    // Tier 2 — scored, best candidates first so a strong pair cannot be stolen
    // by a weaker one that happened to be evaluated earlier.
    QVector<MorphPair> candidates;
    for (const SceneObject &a : from.objects) {
        if (takenFrom.contains(a.id))
            continue;
        for (const SceneObject &b : to.objects) {
            if (takenTo.contains(b.id))
                continue;
            const qreal score = scorePair(a, b);
            if (score >= kScoreThreshold)
                candidates.append({a.id, b.id, MatchKind::Scored, score});
        }
    }
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const MorphPair &l, const MorphPair &r) { return l.score > r.score; });
    for (const MorphPair &candidate : candidates) {
        if (takenFrom.contains(candidate.fromId) || takenTo.contains(candidate.toId))
            continue;
        pairs.append(candidate);
        takenFrom.insert(candidate.fromId);
        takenTo.insert(candidate.toId);
    }

    // Tier 3 — the honest remainder.
    for (const SceneObject &a : from.objects) {
        if (!takenFrom.contains(a.id))
            pairs.append({a.id, QString(), MatchKind::FadeOut, 0.0});
    }
    for (const SceneObject &b : to.objects) {
        if (!takenTo.contains(b.id))
            pairs.append({QString(), b.id, MatchKind::FadeIn, 0.0});
    }
    return pairs;
}

QVector<SceneObject> Morph::stateAt(const Slide &from, const Slide &to,
                                    const QVector<MorphPair> &pairs, qreal u) {
    const qreal eased = QEasingCurve(QEasingCurve::InOutCubic)
                            .valueForProgress(std::clamp(u, 0.0, 1.0));
    QVector<SceneObject> states;
    states.reserve(pairs.size());

    for (const MorphPair &pair : pairs) {
        switch (pair.kind) {
        case MatchKind::Identity:
        case MatchKind::Scored: {
            const SceneObject *a = from.find(pair.fromId);
            const SceneObject *b = to.find(pair.toId);
            if (a && b) {
                auto state=interpolate(*a,*b,eased);
                if ((a->type==ObjectType::Image && b->type==ObjectType::Image) &&
                    (a->imageId!=b->imageId || a->imageMode!=b->imageMode || a->imageMask!=b->imageMask)) {
                    auto outgoing=state;
                    ImageAsset::copyData(outgoing,*a);
                    outgoing.imageMask=a->imageMask; outgoing.imageMode=a->imageMode; outgoing.imageCrop=a->imageCrop;
                    outgoing.opacity=(a->hidden?0.0:a->opacity)*(1-eased);
                    state.imageCrop=b->imageCrop; state.opacity=(b->hidden?0.0:b->opacity)*eased;
                    states.append(outgoing);
                }
                if(a->type==ObjectType::Chart && b->type==ObjectType::Chart && (Table::encode(a->table)!=Table::encode(b->table) || Chart::encode(a->chart)!=Chart::encode(b->chart))) {
                    auto outgoing=*a; outgoing.rect=state.rect; outgoing.rotation=state.rotation;
                    outgoing.opacity=(a->hidden?0.0:a->opacity)*(1-eased);state.opacity=(b->hidden?0.0:b->opacity)*eased;states.append(outgoing);
                }
                if(a->type==ObjectType::Table && b->type==ObjectType::Table && Table::encode(a->table)!=Table::encode(b->table)) {
                    auto outgoing=*a; outgoing.rect=state.rect; outgoing.rotation=state.rotation;
                    outgoing.opacity=(a->hidden?0.0:a->opacity)*(1-eased);
                    state.opacity=(b->hidden?0.0:b->opacity)*eased; states.append(outgoing);
                }
                if(a->type==ObjectType::Media && b->type==ObjectType::Media) {
                    auto outgoing=*a; outgoing.rect=state.rect; outgoing.rotation=state.rotation;
                    outgoing.mediaActive=false; state.mediaActive=false;
                    outgoing.opacity=(a->hidden?0.0:a->opacity)*(1-eased);
                    state.opacity=(b->hidden?0.0:b->opacity)*eased; states.append(outgoing);
                }
                if(a->type==ObjectType::Rect && b->type==ObjectType::Rect && !a->connector && !b->connector &&
                   (a->shapeKind!=b->shapeKind || a->pathData!=b->pathData || a->fillStyle!=b->fillStyle || (a->fillStyle==4 && a->imageId!=b->imageId))) {
                    auto outgoing=state; outgoing.shapeKind=a->shapeKind; outgoing.pathData=a->pathData; outgoing.pathWinding=a->pathWinding; outgoing.fillStyle=a->fillStyle;
                    ImageAsset::copyData(outgoing,*a); outgoing.opacity=(a->hidden?0.0:a->opacity)*(1-eased);
                    state.opacity=(b->hidden?0.0:b->opacity)*eased; states.append(outgoing);
                }
                states.append(state);
            }
            break;
        }
        case MatchKind::FadeOut: {
            // Gone by 60% — the outgoing slide should clear before the incoming
            // one arrives, or the two read as clutter rather than a change.
            if (const SceneObject *a = from.find(pair.fromId)) {
                SceneObject state = *a;
                state.opacity *= std::clamp(1.0 - eased / 0.6, 0.0, 1.0);
                if (state.opacity > 0.0)
                    states.append(state);
            }
            break;
        }
        case MatchKind::FadeIn: {
            if (const SceneObject *b = to.find(pair.toId)) {
                SceneObject state = *b;
                state.opacity *= std::clamp((eased - 0.4) / 0.6, 0.0, 1.0);
                if (state.opacity > 0.0)
                    states.append(state);
            }
            break;
        }
        }
    }
    Connector::resolve(states);
    return states;
}
