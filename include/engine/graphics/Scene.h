#pragma once

#include "engine/graphics/Texture.h"
#include "engine/math/Math.h"
#include <variant>
#include <vector>

namespace engine::graphics {
class Model;
class Mesh;
class SpriteOutline;
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
    // The sky shader quantizes these into a small, dithered palette.
    Color top = {0.30f, 0.60f, 0.93f};
    Color bottom = {0.74f, 0.86f, 0.97f};
    Color sunColor = {1, 1, 1};
    float sunDistance = 100;
    float sunSize = 5.5f; // Night sky supplies its own flat-shaded moon.
    float sunCore = 6.5f;
    float sunHaze = 2.2f;
    float nightBlend = 0;
};
struct MenuOverlay {
    bool visible = false;
    int selection = 0;
    float time = 0;
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
struct SpriteRipple {
    const SpriteOutline* shape = nullptr;
    float period = 1.2f; // Seconds between outgoing waves.
    float widthPixels = 2;
    float opacity = .85f;
};
struct Sprite {
    math::Mat4 model = math::identity();
    const Texture* texture = nullptr;
    Color rippleColor; // Outer ripple color; also tints the sprite's solid outline.
    float spawnTime = 0;
    float aspect = 1;
    float affineStrength = 0;
    const Mesh* profile = nullptr; // Optional extruded silhouette; null draws a flat sprite.
    float outlinePixels = 2;
    SpriteRipple ripple{};
};
using DrawCommand = std::variant<Quad, Cube, ModelInstance, Sprite>;
struct SpriteBurst {
    math::Mat4 model = math::identity(); // Snapshot of the collected sprite's pose.
    const SpriteOutline* shape = nullptr;
    Color color;
    float progress = 0; // One-shot lifetime, 0 (immediate flash) to 1 (fully faded).
    float initialPhase = 0;
    float widthPixels = 3.5f;
    float opacity = 1;
};
using EffectCommand = std::variant<Quad, SpriteBurst>;
struct Scene {
    Camera camera;
    DirectionalLight light;
    Sky sky;
    MenuOverlay menu;
    float time = 0;
    std::vector<DrawCommand> objects;
    std::vector<EffectCommand> effects; // Additive, depth-tested, no depth writes or shadow casting.
};
} // namespace engine::graphics
