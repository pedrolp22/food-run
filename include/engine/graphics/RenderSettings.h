#pragma once

#include "engine/graphics/Types.h"

namespace engine::graphics {
struct RenderSettings {
    Size retroSize = {240, 320}; // PSX raster.
    int shadowMapSize = 2048;
    float retroPixelStep = 1;         // PSX integer screen coordinates; zero disables snapping.
    float modelAffineStrength = 1;    // PSX only, [0, 1]: full affine mapping by default.
    bool psxSubdivideQuads = true;    // Stable geometry grids keep large roads/facades readable.
    float retroSnapStrength = .45f;   // Blend toward the integer grid; 1 enables full hardware-style snapping.
    float retroAffineStrength = .35f; // Shared PSX UV-warp intensity, multiplied by each command's strength.
    float retroDitherStrength = 1; // PSX color/shade transitions and shadow-edge blends, [0, 1]; zero disables dither.
    bool psxDitheredShadows = false; // Saturn-style shadow transparency, independent of RGB dithering.
    float psxDitheredShadowDarkness =
        .8f;                     // Suppress ambient/sky light in covered pixels, [0, 1]; holes stay fully lit.
    bool screenSpaceAmbientOcclusion = true;
    float ambientOcclusionRadius = .65f;   // View-space neighborhood size in world units.
    float ambientOcclusionStrength = 1.35f; // Screen-space darkening, [0, 3].
    float ambientOcclusionBias = .025f;    // Ignore small depth/normal reconstruction errors, in world units.
    bool psxDistanceFog = false; // Stored preference, applied only in PSX mode; the game enables it initially.
    float psxFogStart = 45;      // Nonnegative camera-space start distance; leave nearby action clear.
    float psxFogEnd = 125;       // Finite end distance greater than start.
    Color psxFogColor = {.74f, .86f, .97f}; // Custom daylight color when psxFogFollowsSky is false.
    Color psxNightFogColor = {.38f, .31f, .40f};
    bool psxFogFollowsSky = true;     // Distance fade targets the regular horizon; does not recolor the sky/materials.
    int worldReflectionMapSize = 128; // Cubemap face size for procedural skies; 0 disables all world reflections.
    int planarReflectionSize = 512;   // Full-view longest side, cropped to the visible facade. 0 disables planar.
    int maxPlanarReflections = 6; // Full-detail priority planes; other visible faces increase detail as they approach.
    int distantPlanarReflectionSize = 128; // Full-view longest side for captures outside the detail budget.
    float distantPlanarReflectionUpdateSeconds =
        .2f; // Stagger distant refreshes; nearby faces still update every frame.
    int maxDistantPlanarUpdatesPerFrame =
        4;                                // Spread cached distant refreshes; 0 is unlimited. Buildings stay together.
    float planarReflectionDistance = 120; // Maximum distance to a facade, or its nearest related building face.
    float planarReflectionFadeDistance = 30;   // Blend to the distant environment before the local distance limit.
    float planarReflectionBlendSeconds = .25f; // Shared building activation/detail changes; zero switches immediately.
    float nearPlanarReflectionDistance = 35;   // Always refresh nearby faces, independently of the distant budget.
    float distantPlanarReflectionDetailDistance = 90; // End of the gradual detail transition for other faces.
    void validate() const;                            // Throws for invalid settings; no OpenGL context required.
};
} // namespace engine::graphics
