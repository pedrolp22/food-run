#include "engine/graphics/Scene.h"
#include <algorithm>
#include <cmath>

namespace engine::graphics {
using namespace math;
Mat4 Camera::viewProjection(float aspect) const {
    return multiply(perspective(radians(verticalFov), aspect, nearPlane, farPlane), lookAt(position, target, up));
}
float DirectionalLight::ambientForNormal(Vec3 normal) const {
    return ambient * (0.95f + 0.30f * normal.y);
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
