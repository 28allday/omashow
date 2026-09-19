#pragma once
#include "core/mediaasset.h"
namespace MediaTranscode {
// H.264/AAC MP4: balanced 1080p or compact 720p; audio-only AAC.
MediaAsset::Result preview(const SceneObject &source, int preset,
                           const std::shared_ptr<MediaAsset::Job> &job);
} // namespace MediaTranscode
