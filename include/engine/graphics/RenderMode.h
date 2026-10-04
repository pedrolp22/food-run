#pragma once

#include <string_view>

namespace engine::graphics {
enum class RenderMode { Normal, Psx };

constexpr std::string_view renderModeName(RenderMode mode) {
    switch (mode) {
    case RenderMode::Normal:
        return "Normal";
    case RenderMode::Psx:
        return "PSX";
    }
    return "Normal";
}
} // namespace engine::graphics
