#include "SceneVisibility.h"
#include "engine/graphics/Model.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>

namespace engine::graphics::detail {
using namespace math;
WorldBounds worldBounds(const Mat4& model, Vec3 localCenter, Vec3 halfExtent) {
    const auto& m = model.values;
    return {transformPoint(model, localCenter),
            {std::abs(m[0]) * halfExtent.x + std::abs(m[4]) * halfExtent.y + std::abs(m[8]) * halfExtent.z,
             std::abs(m[1]) * halfExtent.x + std::abs(m[5]) * halfExtent.y + std::abs(m[9]) * halfExtent.z,
             std::abs(m[2]) * halfExtent.x + std::abs(m[6]) * halfExtent.y + std::abs(m[10]) * halfExtent.z}};
}
void SceneVisibility::setView(const Mat4& projection, std::array<float, 4> clipPlane, float padding) {
    planes_[6] = clipPlane;
    const auto& m = projection.values;
    int plane = 0;
    for (int axis = 0; axis < 3; ++axis) {
        for (float sign : {-1.0f, 1.0f}) {
            const float extent = 1 + (axis < 2 ? padding : 0);
            planes_[plane++] = {extent * m[3] + sign * m[axis], extent * m[7] + sign * m[4 + axis],
                                extent * m[11] + sign * m[8 + axis], extent * m[15] + sign * m[12 + axis]};
        }
    }
}
bool SceneVisibility::visible(const WorldBounds& box) const {
    const auto outside = [&](const auto& plane) {
        const float distance = plane[0] * box.center.x + plane[1] * box.center.y + plane[2] * box.center.z + plane[3];
        const float radius =
            std::abs(plane[0]) * box.radius.x + std::abs(plane[1]) * box.radius.y + std::abs(plane[2]) * box.radius.z;
        return distance + radius < 0;
    };
    return box.enabled && !outside(planes_[6]) && std::none_of(planes_.begin(), planes_.begin() + 6, outside);
}
void SceneVisibility::prepare(const Scene& scene) {
    objects_.clear();
    objects_.reserve(scene.objects.size());
    for (const auto& object : scene.objects) {
        objects_.push_back(std::visit(
            [](const auto& item) {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<T, ModelInstance>) {
                    if (!item.model)
                        return WorldBounds{{}, {}, false};
                    if (item.pose && &item.pose->data() != item.model->animationData()) {
                        throw std::invalid_argument("Model instance pose belongs to a different model");
                    }
                    return item.pose
                               ? worldBounds(item.transform, item.pose->boundsCenter(), item.pose->boundsHalfExtent())
                               : worldBounds(item.transform, item.model->boundsCenter(),
                                             item.model->boundsHalfExtent());
                } else if constexpr (std::is_same_v<T, Quad>) {
                    auto box = worldBounds(item.model, {}, {.5f, .5f, 0});
                    box.enabled = item.opacity > 0;
                    return box;
                } else {
                    return worldBounds(item.model, {},
                                       std::is_same_v<T, Cube> ? Vec3{.5f, .5f, .5f} : Vec3{.6f, .6f, .6f});
                }
            },
            object));
    }
}
bool SceneVisibility::surfaceVisible(const Quad& surface) const {
    return surface.opacity > 0 && visible(worldBounds(surface.model, {}, {.5f, .5f, 0}));
}
} // namespace engine::graphics::detail
