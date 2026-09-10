#pragma once

#include <string_view>

namespace engine::graphics {
enum class RenderMode { Normal, Psx, N64 };

constexpr std::string_view renderModeName(RenderMode mode) {
    switch (mode) {
    case RenderMode::Normal:
        return "Normal";
    case RenderMode::Psx:
        return "PSX";
    case RenderMode::N64:
        return "N64";
    }
    return "Normal";
}
} // namespace engine::graphics
