#pragma once

namespace engine::math {
inline unsigned int hashSeed(unsigned int seed) {
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return seed;
}

inline float hashFloat(unsigned int seed, unsigned int index) {
    return static_cast<float>(hashSeed(seed * 137u + index * 7919u) & 0xFFFFu) / 65535.0f;
}
} // namespace engine::math
