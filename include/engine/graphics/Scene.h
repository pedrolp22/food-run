#pragma once

#include "engine/graphics/Texture.h"
#include "engine/math/Math.h"
#include <variant>
#include <vector>

namespace engine::graphics {
class Model;
struct Camera {
    math::Vec3 position = {0, 5, 9};
    math::Vec3 target = {0, 0, 0};
    math::Vec3 up = {0, 1, 0};
    float verticalFov = 60;
    float nearPlane = 0.1f;
    float farPlane = 200;
    math::Mat4 viewProjection(float aspect) const;
};
struct ShadowVolume {
    math::Vec3 center = {0, 0, 0};
    float halfWidth = 76;
    float halfHeight = 88;
    float lightDistance = 100;
    float nearPlane = 1;
    float farPlane = 200;
};
struct DirectionalLight {
    math::Vec3 direction = {-1, 2, -1}; // From a surface toward the light.
    float ambient = 0.48f;
    ShadowVolume shadow;
    float ambientForNormal(math::Vec3 normal) const;
    float lightForNormal(math::Vec3 normal) const;
    math::Mat4 viewProjection() const;
};
struct Sky {
    Color top = {0.30f, 0.60f, 0.93f};
    Color bottom = {0.74f, 0.86f, 0.97f};
    Color sunColor = {1, 1, 1};
    float sunDistance = 100;
    float sunSize = 6;
    float sunCore = 6.5f;
    float sunHaze = 2.2f;
};
// Commands borrow assets. Keep those assets alive and unmoved until render() returns.
struct Quad {
    math::Mat4 model = math::identity();
    math::Vec3 normal = {0, 1, 0};
    Color color;
    const Texture* texture = nullptr;
    math::Vec2 textureScale = {1, 1};
    math::Vec2 textureOffset;
    math::Vec2 worldSize = {1, 1};
    float opacity = 1;
    float affineStrength = 0; // PSX only: opt in to affine warping with values up to 1.
    float heat = 0;
    bool receivesShadow = true;
    bool castsShadow = true;
};
struct Cube {
    math::Mat4 model = math::identity();
    Color color;
};
struct ModelInstance {
    math::Mat4 transform = math::identity();
    const Model* model = nullptr;
};
struct Sprite {
    math::Mat4 model = math::identity();
    const Texture* texture = nullptr;
    Color rippleColor;
    float spawnTime = 0;
    float aspect = 1;
    float affineStrength = 0;
};
using DrawCommand = std::variant<Quad, Cube, ModelInstance, Sprite>;
struct Scene {
    Camera camera;
    DirectionalLight light;
    Sky sky;
    float time = 0;
    std::vector<DrawCommand> objects;
    std::vector<Quad> effects; // Additive, depth-tested, no depth writes or shadow casting.
};
} // namespace engine::graphics
