#pragma once
#include "core/scene.h"
#include <atomic>
#include <memory>

namespace MediaAsset {
constexpr qint64 embedLimit = 64LL * 1024 * 1024;
struct Job {
  std::atomic_bool canceled{false};
  std::atomic_int progress{0};
};
struct Result {
  SceneObject object;
  QString error;
  bool ok() const { return error.isEmpty() && !object.imageId.isEmpty(); }
};
Result fromFile(const QString &path, bool embed,
                const std::shared_ptr<Job> &job = {},
                const QString &displayName = {});
bool validate(const SceneObject &object, QString *error);
QImage frameAt(const SceneObject &object, qreal seconds);
qreal playbackDuration(const SceneObject &object);
void evaluate(SceneObject &object, qreal elapsed, qreal cueDuration);
qint64 embeddedBytes(const Document &document);
void copySource(SceneObject &target, const SceneObject &source);
QString linkState(const SceneObject &object, bool authorized);
} // namespace MediaAsset
