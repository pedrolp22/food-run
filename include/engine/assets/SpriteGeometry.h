#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace engine::assets {
struct SpriteGeometry {
    // Position xyz, UV xy, normal xyz. Unit depth, centered on the opaque bounds.
    static constexpr int stride = 8;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
};
// Alpha rows follow texture UV order (bottom first). Front/back cutouts and pixel contour
// walls share the same silhouette; scale Z in the draw transform to set the thickness.
SpriteGeometry extrudeSprite(std::span<const std::uint8_t> alpha, int width, int height);
} // namespace engine::assets
