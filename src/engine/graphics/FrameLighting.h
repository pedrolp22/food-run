#pragma once

#include "engine/graphics/Scene.h"
#include <array>

namespace engine::graphics::detail {
// Per-frame, view-independent lighting. All color conversions and validation
// happen once; mirrored views only replace their camera-position uniform.
struct FrameLighting {
    math::Vec3 sunDirection;
    Color skyRadiance;
    Color horizonRadiance;
    Color sunRadiance;
    Color hazeColor;
    std::array<float, kMaxPointLights * 4> pointPositions{};
    std::array<float, kMaxPointLights * 3> pointRadiance{};
    int pointCount = 0;
    void prepare(const Scene& scene);
};
} // namespace engine::graphics::detail
