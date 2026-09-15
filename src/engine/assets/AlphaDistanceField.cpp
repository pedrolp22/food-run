#include "engine/assets/AlphaDistanceField.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace engine::assets {
AlphaDistanceField makeAlphaDistanceField(std::span<const std::uint8_t> alpha, int width, int height, int padding,
                                          bool discardIsolatedPixels) {
    if (width <= 0 || height <= 0 || padding < 1 ||
        padding > (std::numeric_limits<int>::max() - std::max(width, height)) / 2 ||
        alpha.size() / static_cast<std::size_t>(width) != static_cast<std::size_t>(height) ||
        alpha.size() % static_cast<std::size_t>(width) != 0) {
        throw std::invalid_argument("Invalid alpha distance field dimensions");
    }
    AlphaDistanceField result;
    result.width = width + 2 * padding;
    result.height = height + 2 * padding;
    const float inactive = std::hypot(float(result.width), float(result.height)) + 1;
    result.pixels.assign(static_cast<std::size_t>(result.width) * result.height, inactive);
    const float infinity = std::numeric_limits<float>::infinity();
    std::vector<float> horizontal(result.pixels.size(), infinity);
    int left = width, right = 0, bottom = height, top = 0;
    bool seeded = false;
    const auto solid = [&](int x, int y) {
        return x >= 0 && x < width && y >= 0 && y < height && alpha[static_cast<std::size_t>(y) * width + x] >= 26;
    };
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (solid(x, y)) {
                left = std::min(left, x);
                right = std::max(right, x + 1);
                bottom = std::min(bottom, y);
                top = std::max(top, y + 1);
                if (!discardIsolatedPixels || solid(x - 1, y) || solid(x + 1, y) || solid(x, y - 1) ||
                    solid(x, y + 1)) {
                    horizontal[static_cast<std::size_t>(y + padding) * result.width + x + padding] = 0;
                    seeded = true;
                }
            }
        }
    }
    if (left >= right) {
        return result;
    }
    result.sourceCenter = {(left * .5f + right * .5f) / width, (bottom * .5f + top * .5f) / height};
    if (!seeded) {
        return result;
    }
    // Horizontal nearest-neighbor distances, squared for the vertical pass.
    for (int y = 0; y < result.height; ++y) {
        auto* row = horizontal.data() + static_cast<std::size_t>(y) * result.width;
        for (int x = 1; x < result.width; ++x) {
            row[x] = std::min(row[x], row[x - 1] + 1);
        }
        for (int x = result.width - 2; x >= 0; --x) {
            row[x] = std::min(row[x], row[x + 1] + 1);
        }
        for (int x = 0; x < result.width; ++x) {
            row[x] *= row[x];
        }
    }
    // Each source row contributes a parabola (y-row)^2 + horizontalDistance^2.
    // Its lower envelope gives exact Euclidean distances in linear time per column.
    std::vector<int> rows(result.height);
    std::vector<float> boundaries(static_cast<std::size_t>(result.height) + 1);
    for (int x = 0; x < result.width; ++x) {
        const auto cost = [&](int y) { return horizontal[static_cast<std::size_t>(y) * result.width + x]; };
        int last = -1;
        for (int y = 0; y < result.height; ++y) {
            if (!std::isfinite(cost(y))) {
                continue;
            }
            float start = -infinity;
            while (last >= 0) {
                const int previous = rows[last];
                start =
                    (cost(y) - cost(previous) + float(y) * y - float(previous) * previous) / (2.0f * (y - previous));
                if (start > boundaries[last]) {
                    break;
                }
                --last;
            }
            ++last;
            rows[last] = y;
            boundaries[last] = last == 0 ? -infinity : start;
            boundaries[last + 1] = infinity;
        }
        int nearest = 0;
        for (int y = 0; y < result.height; ++y) {
            while (boundaries[nearest + 1] < y) {
                ++nearest;
            }
            const float dy = y - rows[nearest];
            result.pixels[static_cast<std::size_t>(y) * result.width + x] =
                std::sqrt(dy * dy + cost(rows[nearest])) - .5f;
        }
    }
    return result;
}
} // namespace engine::assets
