#pragma once

#include "engine/graphics/Texture.h"
#include "engine/math/Math.h"

namespace engine::graphics {
// Cached silhouette field and expanded quad placement for an extruded, centered sprite.
// Create once per texture; callers animate the ripple without rebuilding the field.
class SpriteOutline {
  public:
    SpriteOutline() = default;
    explicit SpriteOutline(const Texture& sprite, int radiusPixels, bool discardIsolatedPixels = false);
    const Texture& distanceField() const { return field_; }
    const math::Mat4& transform() const { return transform_; }
    float radius() const { return radius_; }

  private:
    Texture field_;
    math::Mat4 transform_ = math::identity();
    float radius_ = 0;
};
} // namespace engine::graphics
