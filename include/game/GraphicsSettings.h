#pragma once

#include "GameConfig.h"
#include "engine/graphics/RenderSettings.h"
#include <filesystem>

namespace game {
enum class GraphicsQuality { Off, Low, Medium, High };
enum class GraphicsPreset { Low, Medium, High, Custom };
enum class DisplayMode { Windowed, Borderless };

// User preferences, independent of the window and GPU resources.
struct GraphicsSettings {
    int renderScale = 100;
    GraphicsQuality shadows = GraphicsQuality::High;
    GraphicsQuality reflections = GraphicsQuality::High;
    bool ambientOcclusion = true;
    bool materialDetail = true;
    bool foliageParticles = true;
    bool antiAliasing = false;
    bool vsync = true;
    DisplayMode displayMode = DisplayMode::Windowed;
    bool retroMode = false;
    bool ditheredShadows = GameConfig::kPsxDitheredShadows;
    bool distanceFog = GameConfig::kPsxDistanceFog;

    void applyPreset(GraphicsPreset preset);
    GraphicsPreset preset() const;
    engine::graphics::RenderSettings renderSettings() const;
    static GraphicsSettings load(const std::filesystem::path& path);
    bool save(const std::filesystem::path& path) const;
};
} // namespace game
