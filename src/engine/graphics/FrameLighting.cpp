#include "FrameLighting.h"
#include <algorithm>
#include <cmath>

namespace engine::graphics::detail {
using namespace math;
namespace {
float linearChannel(float value) {
    value = std::isfinite(value) ? std::max(value, 0.0f) : 0;
    return value <= .04045f ? value / 12.92f : std::pow((value + .055f) / 1.055f, 2.4f);
}
Color linearColor(Color color) {
    return {linearChannel(color.r), linearChannel(color.g), linearChannel(color.b)};
}
} // namespace
void FrameLighting::prepare(const Scene& scene) {
    sunDirection = normalize(scene.light.direction);
    sunRadiance = linearColor(scene.sky.sunColor);
    Color sky;
    if (scene.sky.hasTexturedClouds()) {
        const auto palette = scene.sky.palette();
        sky = palette.top;
        hazeColor = palette.horizon;
    } else {
        const float night = std::floor(std::clamp(scene.sky.nightBlend, 0.0f, 1.0f) * 4 + .5f) / 4;
        sky = {scene.sky.top.r * (1 - night) + .045f * night, scene.sky.top.g * (1 - night) + .055f * night,
               scene.sky.top.b * (1 - night) + .14f * night};
        hazeColor = {scene.sky.bottom.r * (1 - night) + .38f * night, scene.sky.bottom.g * (1 - night) + .31f * night,
                     scene.sky.bottom.b * (1 - night) + .40f * night};
    }
    skyRadiance = linearColor(sky);
    horizonRadiance = linearColor(hazeColor);
    pointCount = 0;
    for (const auto& light : scene.pointLights) {
        if (!std::isfinite(light.position.x) || !std::isfinite(light.position.y) || !std::isfinite(light.position.z) ||
            !std::isfinite(light.radius) || light.radius <= 0 || !std::isfinite(light.intensity) ||
            light.intensity <= 0 || !std::isfinite(light.color.r) || !std::isfinite(light.color.g) ||
            !std::isfinite(light.color.b)) {
            continue;
        }
        const int index = pointCount;
        pointPositions[index * 4] = light.position.x;
        pointPositions[index * 4 + 1] = light.position.y;
        pointPositions[index * 4 + 2] = light.position.z;
        pointPositions[index * 4 + 3] = std::clamp(light.radius, .01f, 200.0f);
        const float intensity = std::min(light.intensity, 64.0f);
        pointRadiance[index * 3] = linearChannel(std::clamp(light.color.r, 0.0f, 1.0f)) * intensity;
        pointRadiance[index * 3 + 1] = linearChannel(std::clamp(light.color.g, 0.0f, 1.0f)) * intensity;
        pointRadiance[index * 3 + 2] = linearChannel(std::clamp(light.color.b, 0.0f, 1.0f)) * intensity;
        if (++pointCount == static_cast<int>(kMaxPointLights)) {
            break;
        }
    }
}
} // namespace engine::graphics::detail
