#pragma once

#include "engine/graphics/Scene.h"
#include <array>
#include <vector>

namespace engine::graphics::detail {
struct WorldBounds {
    math::Vec3 center;
    math::Vec3 radius;
    bool enabled = true;
};
WorldBounds worldBounds(const math::Mat4& transform, math::Vec3 center, math::Vec3 halfExtent);

// Cache scene bounds once, then reuse them for camera, shadow and reflected
// views. Pure geometry: no GPU state or knowledge of drawing programs.
class SceneVisibility {
  public:
    void prepare(const Scene& scene);
    void setView(const math::Mat4& projection, std::array<float, 4> clipPlane = {0, 0, 0, 1}, float padding = 0);
    bool visible(const WorldBounds& bounds) const;
    bool objectVisible(std::size_t index) const { return visible(objects_[index]); }
    bool surfaceVisible(const Quad& surface) const;

  private:
    std::vector<WorldBounds> objects_;
    std::array<std::array<float, 4>, 7> planes_{};
};
} // namespace engine::graphics::detail
