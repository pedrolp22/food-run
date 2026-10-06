#include "engine/graphics/Scene.h"
#include <algorithm>
#include <cmath>

namespace engine::graphics {
using namespace math;
Mat4 Camera::viewProjection(float aspect) const {
    return multiply(perspective(radians(verticalFov), aspect, nearPlane, farPlane), lookAt(position, target, up));
}
bool Sky::hasTexturedClouds() const {
    return std::any_of(texturedClouds.begin(), texturedClouds.end(),
                       [](const auto& layer) { return layer.texture && layer.texture->id(); });
}
SkyPalette Sky::palette() const {
    const float phase = std::isfinite(nightBlend) ? std::clamp(nightBlend, 0.0f, 1.0f) : 0;
    const SkyPalette day = {top, bottom, {1, 1, 1}};
    const auto& from = phase < .5f ? day : dusk;
    const auto& to = phase < .5f ? dusk : night;
    const float t = phase < .5f ? phase * 2 : phase * 2 - 1;
    const auto blend = [t](Color a, Color b) {
        return Color{std::lerp(a.r, b.r, t), std::lerp(a.g, b.g, t), std::lerp(a.b, b.b, t)};
    };
    return {blend(from.top, to.top), blend(from.horizon, to.horizon), blend(from.cloudTint, to.cloudTint)};
}
float DirectionalLight::ambientForNormal(Vec3 normal) const {
    return std::clamp(ambient * (0.95f + 0.30f * normal.y), 0.0f, 1.0f);
}
float DirectionalLight::lightForNormal(Vec3 normal) const {
    const float a = ambientForNormal(normal);
    return a + (1.0f - a) * std::max(0.0f, dot(normal, normalize(direction)));
}
Mat4 DirectionalLight::viewProjection() const {
    const Vec3 sun = normalize(direction);
    const Vec3 eye = {shadow.center.x + sun.x * shadow.lightDistance, shadow.center.y + sun.y * shadow.lightDistance,
                      shadow.center.z + sun.z * shadow.lightDistance};
    // Also support a vertical sun without a degenerate lookAt basis.
    const Vec3 up = std::abs(sun.y) > 0.99f ? Vec3{0, 0, 1} : Vec3{0, 1, 0};
    return multiply(orthographic(shadow.halfWidth, shadow.halfHeight, shadow.nearPlane, shadow.farPlane),
                    lookAt(eye, shadow.center, up));
}
} // namespace engine::graphics
