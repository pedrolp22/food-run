#pragma once

#include "engine/math/Math.h"
#include <cstdint>
#include <span>
#include <vector>

namespace engine::assets {
struct AlphaDistanceField {
    int width = 0;
    int height = 0;
    math::Vec2 sourceCenter = {.5f, .5f}; // Visible bounds' center in original texture UVs.
    std::vector<float> pixels;            // Distance outside the silhouette in source texels; opaque texels are -0.5.
};
// Bottom-first alpha, with the same 0.1 cutoff as sprite rendering. Padding lets outlines
// expand past the original image bounds. Empty silhouettes produce an entirely inactive field.
// Optionally suppress isolated alpha specks without changing the original sprite's centering.
AlphaDistanceField makeAlphaDistanceField(std::span<const std::uint8_t> alpha, int width, int height, int padding,
                                          bool discardIsolatedPixels = false);
} // namespace engine::assets
