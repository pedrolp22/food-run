#include "engine/assets/SpriteGeometry.h"
#include "engine/math/Math.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace engine::assets {
SpriteGeometry extrudeSprite(std::span<const std::uint8_t> alpha, int width, int height) {
    if (width <= 0 || height <= 0 ||
        alpha.size() / static_cast<std::size_t>(width) != static_cast<std::size_t>(height) ||
        alpha.size() % static_cast<std::size_t>(width) != 0) {
        throw std::invalid_argument("Invalid sprite alpha dimensions");
    }
    const auto solid = [&](int x, int y) {
        // Matches the shader's 0.1 alpha cutoff, including at the texture borders.
        return x >= 0 && x < width && y >= 0 && y < height && alpha[static_cast<std::size_t>(y) * width + x] >= 26;
    };
    int left = width, right = 0, bottom = height, top = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (solid(x, y)) {
                left = std::min(left, x);
                right = std::max(right, x + 1);
                bottom = std::min(bottom, y);
                top = std::max(top, y + 1);
            }
        }
    }
    SpriteGeometry result;
    if (left >= right) {
        return result;
    }
    using namespace math;
    const Vec2 center = {(left * 0.5f + right * 0.5f) / width, (bottom * 0.5f + top * 0.5f) / height};
    const auto face = [&](std::array<Vec3, 4> points, std::array<Vec2, 4> uv, Vec3 normal) {
        const auto first = static_cast<unsigned int>(result.vertices.size() / SpriteGeometry::stride);
        for (int i = 0; i < 4; ++i) {
            result.vertices.insert(result.vertices.end(), {points[i].x - center.x, points[i].y - center.y, points[i].z,
                                                           uv[i].x, uv[i].y, normal.x, normal.y, normal.z});
        }
        result.indices.insert(result.indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
    };
    const float u0 = static_cast<float>(left) / width, u1 = static_cast<float>(right) / width;
    const float v0 = static_cast<float>(bottom) / height, v1 = static_cast<float>(top) / height;
    face({Vec3{u0, v0, .5f}, {u1, v0, .5f}, {u1, v1, .5f}, {u0, v1, .5f}}, {Vec2{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}},
         {0, 0, 1});
    face({Vec3{u1, v0, -.5f}, {u0, v0, -.5f}, {u0, v1, -.5f}, {u1, v1, -.5f}},
         {Vec2{u1, v0}, {u0, v0}, {u0, v1}, {u1, v1}}, {0, 0, -1});
    for (int y = bottom; y < top; ++y) {
        for (int x = left; x < right; ++x) {
            if (!solid(x, y)) {
                continue;
            }
            const float x0 = static_cast<float>(x) / width, x1 = static_cast<float>(x + 1) / width;
            const float y0 = static_cast<float>(y) / height, y1 = static_cast<float>(y + 1) / height;
            // Sample the opaque edge pixel's center across the wall's entire depth.
            // This prevents transparent padding bleeding onto the side as it rotates.
            const Vec2 sample = {(x + .5f) / width, (y + .5f) / height};
            const std::array<Vec2, 4> uv = {sample, sample, sample, sample};
            if (!solid(x - 1, y)) {
                face({Vec3{x0, y0, -.5f}, {x0, y0, .5f}, {x0, y1, .5f}, {x0, y1, -.5f}}, uv, {-1, 0, 0});
            }
            if (!solid(x + 1, y)) {
                face({Vec3{x1, y0, .5f}, {x1, y0, -.5f}, {x1, y1, -.5f}, {x1, y1, .5f}}, uv, {1, 0, 0});
            }
            if (!solid(x, y - 1)) {
                face({Vec3{x0, y0, -.5f}, {x1, y0, -.5f}, {x1, y0, .5f}, {x0, y0, .5f}}, uv, {0, -1, 0});
            }
            if (!solid(x, y + 1)) {
                face({Vec3{x0, y1, .5f}, {x1, y1, .5f}, {x1, y1, -.5f}, {x0, y1, -.5f}}, uv, {0, 1, 0});
            }
        }
    }
    return result;
}
} // namespace engine::assets
