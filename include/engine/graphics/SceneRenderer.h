#pragma once

#include "engine/graphics/RenderMode.h"
#include "engine/graphics/Scene.h"
#include <memory>

namespace engine::graphics {
struct RenderSettings {
    Size retroSize = {240, 320}; // PSX raster.
    int shadowMapSize = 2048;
    float retroPixelStep = 1;      // PSX integer screen coordinates; zero disables snapping.
    float modelAffineStrength = 1; // PSX only, [0, 1]: full affine mapping by default.
    bool psxSubdivideQuads = true; // Stable geometry grids keep large roads/facades readable.
    float retroSnapStrength = .45f; // Blend toward the integer grid; 1 enables full hardware-style snapping.
    float retroAffineStrength = .35f; // Shared PSX UV-warp intensity, multiplied by each command's strength.
    float retroDitherStrength = 1; // PSX color/shade transitions and shadow-edge blends, [0, 1]; zero disables dither.
    bool psxDitheredShadows = false; // Saturn-style shadow transparency, independent of RGB dithering.
    float psxDitheredShadowDarkness = .8f; // Suppress ambient/sky light in covered pixels, [0, 1]; holes stay fully lit.
    bool psxDistanceFog = false; // Stored preference, applied only in PSX mode; the game enables it initially.
    float psxFogStart = 45; // Nonnegative camera-space start distance; leave nearby action clear.
    float psxFogEnd = 125; // Finite end distance greater than start.
    Color psxFogColor = {.74f, .86f, .97f}; // Custom daylight color when psxFogFollowsSky is false.
    Color psxNightFogColor = {.38f, .31f, .40f};
    bool psxFogFollowsSky = true; // Distance fade targets the regular horizon; does not recolor the sky/materials.
    int worldReflectionMapSize = 128;    // Cubemap face size for procedural skies; 0 disables all world reflections.
    int planarReflectionSize = 512;      // Full-view longest side, cropped to the visible facade. 0 disables planar.
    int maxPlanarReflections = 6;        // Capture the most visible nearby facade planes each frame.
    float planarReflectionDistance = 70; // Maximum distance from the camera to a facade's nearest point.
    float planarReflectionFadeDistance = 20; // Blend to the distant environment before the local distance limit.
};
// Requires a current OpenGL 3.3 context. Does not own a window or swap buffers.
class SceneRenderer {
  public:
    explicit SceneRenderer(RenderSettings settings = {});
    ~SceneRenderer();
    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;
    void render(const Scene& scene, Size drawableSize);
    void setMode(RenderMode mode);
    RenderMode mode() const;
    // Compatibility: these select/query the PSX style only.
    void setRetroMode(bool enabled);
    bool retroMode() const;
    void setPsxDitheredShadows(bool enabled);
    bool psxDitheredShadows() const; // Stored preference; applied only while PSX mode is active.
    void setPsxDistanceFog(bool enabled);
    bool psxDistanceFog() const; // Preserved across mode switches, independently of color/shadow dithering.

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace engine::graphics
