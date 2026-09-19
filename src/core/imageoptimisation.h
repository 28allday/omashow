#pragma once
#include "core/mediaasset.h"
namespace ImageOptimisation {
// Photograph JPEG at 85/70 quality and 1920/1280px; PNG at 1920px.
MediaAsset::Result preview(const SceneObject &source, int preset,
                           const std::shared_ptr<MediaAsset::Job> &job);
} // namespace ImageOptimisation
