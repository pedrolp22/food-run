#include "TestSupport.h"
#include "game/GraphicsSettings.h"
#include <chrono>
#include <fstream>

void settingsTests() {
    using namespace game;
    const auto folder = std::filesystem::temp_directory_path() /
        ("subway-settings-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(folder);
    struct Cleanup {
        std::filesystem::path folder;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(folder, ignored); }
    } cleanup{folder};
    const auto path = folder / "game.cfg";
    const auto defaults = GraphicsSettings::load(path);
    CHECK(defaults.preset() == GraphicsPreset::High && defaults.renderScale == 100);
    GraphicsSettings settings;
    settings.applyPreset(GraphicsPreset::Low);
    settings.vsync = false;
    settings.retroMode = true;
    settings.ditheredShadows = true;
    settings.distanceFog = false;
    CHECK(settings.save(path));
    auto restored = GraphicsSettings::load(path);
    CHECK(restored.preset() == GraphicsPreset::Low && restored.renderScale == 50);
    CHECK(!restored.vsync && restored.retroMode && restored.ditheredShadows && !restored.distanceFog);
    auto low = restored.renderSettings();
    CHECK(!low.shadows && !low.screenSpaceAmbientOcclusion && !low.materialDetail && !low.foliageParticles);
    CHECK(low.planarReflectionSize == 0 && low.worldReflectionMapSize == 0 && near(low.renderScale, .5f));
    settings.applyPreset(GraphicsPreset::Medium);
    settings.antiAliasing = true;
    CHECK(settings.preset() == GraphicsPreset::Custom);
    CHECK(settings.save(path)); // Replacing an existing cfg must work on Windows too.
    restored = GraphicsSettings::load(path);
    CHECK(restored.preset() == GraphicsPreset::Custom && restored.antiAliasing && restored.renderScale == 75);
    CHECK(!restored.vsync && restored.retroMode); // Quality presets retain display/retro preferences.
    {
        std::ofstream output(path, std::ios::binary);
        output << "# Edited settings\r\nquality = medium\r\nrender_scale = 9000\r\n"
               << "shadows = invalid\r\nreflections = off # optional\r\n"
               << "ambient_occlusion = perhaps\r\nvsync = off\r\nunknown = ignored\r\n";
    }
    restored = GraphicsSettings::load(path);
    CHECK(restored.renderScale == 75 && restored.shadows == GraphicsQuality::Medium);
    CHECK(restored.reflections == GraphicsQuality::Off && restored.ambientOcclusion && !restored.vsync);
    CHECK(!restored.save(folder / "missing" / "game.cfg"));
}
