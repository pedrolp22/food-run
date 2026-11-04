#include "engine/graphics/RenderSettings.h"
#include <cmath>
#include <stdexcept>

namespace engine::graphics {
void RenderSettings::validate() const {
    if (!std::isfinite(renderScale) || renderScale < .5f || renderScale > 1) {
        throw std::invalid_argument("Render scale must be between 0.5 and 1");
    }
    if (retroSize.width <= 0 || retroSize.height <= 0 || shadowMapSize <= 0) {
        throw std::invalid_argument("Retro and shadow target dimensions must be positive");
    }
    if (worldReflectionMapSize < 0) {
        throw std::invalid_argument("World reflection map size must be nonnegative");
    }
    if (planarReflectionSize < 0 || maxPlanarReflections < 0 || maxDistantPlanarUpdatesPerFrame < 0 ||
        distantPlanarReflectionSize <= 0 || !std::isfinite(distantPlanarReflectionUpdateSeconds) ||
        distantPlanarReflectionUpdateSeconds < 0 || !std::isfinite(planarReflectionDistance) ||
        planarReflectionDistance <= 0 || !std::isfinite(planarReflectionFadeDistance) ||
        planarReflectionFadeDistance < 0 || !std::isfinite(planarReflectionBlendSeconds) ||
        planarReflectionBlendSeconds < 0 || !std::isfinite(nearPlanarReflectionDistance) ||
        nearPlanarReflectionDistance < 0 || !std::isfinite(distantPlanarReflectionDetailDistance) ||
        distantPlanarReflectionDetailDistance <= nearPlanarReflectionDistance) {
        throw std::invalid_argument("Invalid planar reflection size, distance, fade or update interval");
    }
    if (!std::isfinite(retroPixelStep) || retroPixelStep < 0 || !std::isfinite(modelAffineStrength) ||
        modelAffineStrength < 0 || modelAffineStrength > 1) {
        throw std::invalid_argument("Retro pixel step must be nonnegative and affine strength must be in [0, 1]");
    }
    if (!std::isfinite(retroSnapStrength) || retroSnapStrength < 0 || retroSnapStrength > 1 ||
        !std::isfinite(retroAffineStrength) || retroAffineStrength < 0 || retroAffineStrength > 1) {
        throw std::invalid_argument("Retro snap and affine intensities must be in [0, 1]");
    }
    if (!std::isfinite(psxFogStart) || psxFogStart < 0 || !std::isfinite(psxFogEnd) || psxFogEnd <= psxFogStart) {
        throw std::invalid_argument("PSX fog requires a nonnegative start and a greater finite end");
    }
    for (Color fog : {psxFogColor, psxNightFogColor}) {
        for (float channel : {fog.r, fog.g, fog.b}) {
            if (!std::isfinite(channel) || channel < 0 || channel > 1) {
                throw std::invalid_argument("PSX fog colors must be finite and in [0, 1]");
            }
        }
    }
    if (!std::isfinite(retroDitherStrength) || retroDitherStrength < 0 || retroDitherStrength > 1) {
        throw std::invalid_argument("Retro dither strength must be in [0, 1]");
    }
    if (!std::isfinite(psxDitheredShadowDarkness) || psxDitheredShadowDarkness < 0 || psxDitheredShadowDarkness > 1) {
        throw std::invalid_argument("Dithered shadow darkness must be in [0, 1]");
    }
    if (!std::isfinite(ambientOcclusionRadius) || ambientOcclusionRadius < 0.05f || ambientOcclusionRadius > 2.0f ||
        !std::isfinite(ambientOcclusionStrength) || ambientOcclusionStrength < 0 || ambientOcclusionStrength > 3 ||
        !std::isfinite(ambientOcclusionBias) || ambientOcclusionBias < 0 || ambientOcclusionBias > 0.25f) {
        throw std::invalid_argument("Ambient occlusion radius, strength or bias is outside its supported range");
    }
}
} // namespace engine::graphics
