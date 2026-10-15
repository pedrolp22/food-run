#pragma once

#include "engine/graphics/Material.h"
#include "engine/graphics/Texture.h"
#include "engine/math/Math.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace engine::assets {
class ModelPose;
}
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
    float fisheyeStrength = 0; // Presentation lens: 0 is rectilinear; clamped to [0, 0.25].
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
inline constexpr std::size_t kMaxPointLights = 16; // Also supplies the material shader array capacity at compilation.
struct PointLight {
    math::Vec3 position;
    Color color = {1, .78f, .36f}; // sRGB; uploaded as linear radiance for the shared material shading.
    float intensity = 1;
    float radius = 11; // Smooth finite falloff; local lights do not add shadow-map passes.
};
struct SkyPalette {
    Color top;
    Color horizon;
    Color cloudTint;
};
struct SkyCloudLayer {
    const Texture* texture = nullptr;
    float widthDegrees = 26;
    float elevationDegrees = 7;
    float repeatDegrees = 90;    // Use a divisor of 360 for a seamless full dome.
    float scrollSpeed = 0.0006f; // World turns per second.
    float offset = 0;            // World turns.
    float opacity = 1;
    float horizontalScatter = 0; // Fraction of slot spacing, [0, .45].
    float elevationScatterDegrees = 0;
    float sizeVariation = 0; // Fractional scale variation, [0, .5].
    float curvature = 0;     // Concave texture bow; 0 retains an undeformed cloud.
    float warpStrength = 0;
    float warpSpeed = .14f; // Radians per second.
};
struct SkyStars {
    float density = 0; // Authored cloud skies only; zero disables stars.
    float sizeDegrees = .11f;
    float brightness = .9f;
    float twinkleSpeed = 1.4f;
    float twinkleStrength = .6f;
};
struct SkyMoon {
    const Texture* texture = nullptr;         // Borrowed alpha sprite, behind clouds in an authored sky.
    math::Vec3 direction = {0.14f, .18f, -1}; // World direction; also used for the game's night light.
    float sizeDegrees = 6.5f;                 // Angular width; source aspect determines height.
    float brightness = 1.35f;
    float glowStrength = .16f;
};
struct Sky {
    // Sky/cloud shading is continuous; retro modes apply their native color precision.
    Color top = {0.30f, 0.60f, 0.93f};
    Color bottom = {0.74f, 0.86f, 0.97f};
    Color sunColor = {1, 1, 1};
    float sunDistance = 100; // Together with sunSize, sets the sun's angular diameter.
    float sunSize = 5.5f;    // Procedural fallback supplies a flat-shaded moon.
    float sunCore = 6.5f;
    float sunHaze = 2.2f;
    float nightBlend = 0;
    float cloudCoverage = 0.40f;       // Procedural cloud volumes only, from clear (0) to overcast (1).
    const Texture* panorama = nullptr; // Borrowed upper-hemisphere panorama; bypasses procedural clouds when present.
    float panoramaScrollSpeed = 0;     // Horizontal texture widths per second; zero keeps the panorama stationary.
    std::array<SkyCloudLayer, 3> texturedClouds{}; // Borrowed alpha sprites over a gradient; bypasses sky generation.
    SkyPalette dusk = {{.54f, .28f, .51f}, {.98f, .62f, .67f}, {1, .67f, .73f}};
    SkyPalette night = {{.025f, .035f, .10f}, {.12f, .16f, .29f}, {.24f, .28f, .44f}};
    unsigned int layoutSeed = 0; // Stable cloud scatter and star positions in world directions.
    SkyStars stars{};
    SkyMoon moon{};
    bool hasTexturedClouds() const;
    SkyPalette palette() const; // Continuous day (0), rose dusk (.5), night (1).
};
struct MenuOverlay {
    bool visible = false;
    int selection = 0;
    float time = 0;
};
// Commands borrow assets and evaluated poses. Keep them alive, unmoved and unchanged until render() returns.
struct Quad {
    math::Mat4 model = math::identity();
    math::Vec3 normal = {0, 1, 0};
    Color color;
    const Texture* texture = nullptr;
    math::Vec2 textureScale = {1, 1};
    math::Vec2 textureOffset;
    math::Vec2 worldSize = {1, 1};
    float opacity = 1;
    float affineStrength = 1; // PSX only: 1 is console affine mapping; 0 opts out.
    float heat = 0;
    bool skyscraperWindows = false; // Blue glass uses WindowMaterial; frames use SurfaceMaterial.
    bool receivesShadow = true;
    bool castsShadow = true;
    WindowMaterial windowMaterial;
    bool buildingSurface = false; // PBR-like facade/roof shading, also when there is no glass.
    float haze = 0;               // Atmospheric blend without making a distant surface transparent.
    SurfaceMaterial surfaceMaterial{};
    bool pbrSurface = false;                       // Enable the shared material on textured or colored quads.
    std::optional<float> psxMinificationLodBias{}; // Opt in to nearest mip texels; empty preserves base-level sampling.
    MaterialTextures
        materialTextures{}; // PBR surface data; maps follow the same tiling/affine UVs as the color texture.
    math::Vec2 facadeVerticalRange = {0, 1}; // Normalized bottom/top in the whole facade, for foundation occlusion.
    float alphaCutoff = 0;                   // Cutout coverage, shared by color, shadow and reflection passes.
    bool doubleSided = false;                // Flip lighting normals on the back of thin surfaces.
    std::uint64_t reflectionId =
        0; // Optional stable facade identity across moving/rebuilt scenes; zero uses its plane.
    std::uint64_t reflectionGroupId = 0; // Related building faces share distance and activation timing.
};
struct Cube {
    math::Mat4 model = math::identity();
    Color color;
    SurfaceMaterial surfaceMaterial{};
    bool pbrSurface = false;
};
struct ModelInstance {
    math::Mat4 transform = math::identity();
    const Model* model = nullptr;
    bool pbrSurface = false;                           // Use each primitive's imported material factors.
    std::optional<SurfaceMaterial> materialOverride{}; // Optional finish override for this instance only.
    float emissiveScale = 1;                 // PBR emission multiplier; zero switches off the imported luminous texels.
    const assets::ModelPose* pose = nullptr; // Borrowed per-instance pose; null selects the model's bind pose.
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
    float affineStrength = 1;      // PSX only; other modes retain perspective-correct UVs.
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
    std::vector<EffectCommand> effects;                  // Additive, depth-tested, no depth writes or shadow casting.
    std::optional<math::Vec3> reflectionProbePosition{}; // Empty captures the world at the camera position.
    std::vector<Quad> particles; // Alpha-blended surfaces, sorted per camera; no depth writes or shadow casting.
    std::vector<PointLight> pointLights; // Up to kMaxPointLights active local lights on PBR surfaces, in every view.
};
} // namespace engine::graphics
