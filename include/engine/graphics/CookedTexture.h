#pragma once

#include <array>
#include <cstdint>

namespace engine::graphics::detail {
inline constexpr std::array<char, 8> kCookedTextureMagic = {'S', 'W', 'Y', 'T', 'E', 'X', '1', '\0'};

#pragma pack(push, 1)
struct CookedTextureHeader {
    std::array<char, 8> magic = kCookedTextureMagic;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t mipLevels = 0;
    std::uint64_t sourceSize = 0;
    std::int64_t sourceTimestamp = 0;
};
#pragma pack(pop)
static_assert(sizeof(CookedTextureHeader) == 36);
} // namespace engine::graphics::detail
