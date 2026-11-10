#include "game/GraphicsSettings.h"
#include "GameConfig.h"
#include <array>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#ifdef _WIN32
#include <windows.h>
#endif

namespace game {
namespace {
std::string trim(std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
        return {};
    return std::string(text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1));
}
const char* qualityName(GraphicsQuality quality) {
    constexpr std::array names = {"off", "low", "medium", "high"};
    return names[static_cast<int>(quality)];
}
GraphicsQuality parseQuality(const std::string& text, GraphicsQuality fallback) {
    for (auto quality : {GraphicsQuality::Off, GraphicsQuality::Low, GraphicsQuality::Medium, GraphicsQuality::High}) {
        if (text == qualityName(quality))
            return quality;
    }
    return fallback;
}
auto qualityValues(const GraphicsSettings& settings) {
    return std::tie(settings.renderScale, settings.shadows, settings.reflections, settings.ambientOcclusion,
                    settings.materialDetail, settings.foliageParticles, settings.antiAliasing);
}
} // namespace

void GraphicsSettings::applyPreset(GraphicsPreset preset) {
    if (preset == GraphicsPreset::Custom)
        return;
    const bool low = preset == GraphicsPreset::Low;
    const bool high = preset == GraphicsPreset::High;
    renderScale = low ? 50 : high ? 100 : 75;
    shadows = low ? GraphicsQuality::Off : high ? GraphicsQuality::High : GraphicsQuality::Medium;
    reflections = low ? GraphicsQuality::Off : high ? GraphicsQuality::High : GraphicsQuality::Medium;
    ambientOcclusion = !low;
    materialDetail = !low;
    foliageParticles = !low;
    antiAliasing = false;
}
GraphicsPreset GraphicsSettings::preset() const {
    for (const auto preset : {GraphicsPreset::Low, GraphicsPreset::Medium, GraphicsPreset::High}) {
        GraphicsSettings candidate;
        candidate.applyPreset(preset);
        if (qualityValues(*this) == qualityValues(candidate))
            return preset;
    }
    return GraphicsPreset::Custom;
}
engine::graphics::RenderSettings GraphicsSettings::renderSettings() const {
    engine::graphics::RenderSettings result;
    result.retroSize = {GameConfig::kPsxInternalWidth, GameConfig::kPsxInternalHeight};
    result.renderScale = renderScale / 100.0f;
    result.antiAliasing = antiAliasing;
    result.shadows = shadows != GraphicsQuality::Off;
    constexpr std::array shadowSizes = {1, 512, 1024, 2048};
    result.shadowMapSize = shadowSizes[static_cast<int>(shadows)];
    result.screenSpaceAmbientOcclusion = ambientOcclusion;
    result.materialDetail = materialDetail;
    result.foliageParticles = foliageParticles;
    const int reflection = static_cast<int>(reflections);
    constexpr std::array reflectionSizes = {0, 128, 256, 512};
    constexpr std::array reflectionCounts = {0, 1, 3, 6};
    result.worldReflectionMapSize = reflection == 0 ? 0 : reflection == 3 ? 128 : 64;
    result.planarReflectionSize = reflectionSizes[reflection];
    result.maxPlanarReflections = reflectionCounts[reflection];
    result.distantPlanarReflectionSize = reflection <= 1 ? 64 : 128;
    result.maxDistantPlanarUpdatesPerFrame = reflection <= 1 ? 1 : reflection == 2 ? 2 : 4;
    result.retroDitherStrength = GameConfig::kPsxDitherStrength;
    result.psxDitheredShadows = ditheredShadows;
    result.psxDitheredShadowDarkness = GameConfig::kPsxDitheredShadowDarkness;
    result.psxDistanceFog = distanceFog;
    result.psxFogStart = GameConfig::kPsxFogStart;
    result.psxFogEnd = GameConfig::kPsxFogEnd;
    result.psxFogColor = {GameConfig::kPsxFogColor[0], GameConfig::kPsxFogColor[1], GameConfig::kPsxFogColor[2]};
    result.psxNightFogColor = {GameConfig::kPsxNightFogColor[0], GameConfig::kPsxNightFogColor[1],
                              GameConfig::kPsxNightFogColor[2]};
    result.psxFogFollowsSky = GameConfig::kPsxFogFollowsSky;
    return result;
}
GraphicsSettings GraphicsSettings::load(const std::filesystem::path& path) {
    GraphicsSettings result;
    std::ifstream input(path);
    std::map<std::string, std::string> values;
    for (std::string line; std::getline(input, line);) {
        const std::string_view content(line.data(), line.find('#') == std::string::npos ? line.size() : line.find('#'));
        if (const auto equals = content.find('='); equals != std::string_view::npos)
            values[trim(content.substr(0, equals))] = trim(content.substr(equals + 1));
    }
    const auto& preset = values["quality"];
    if (preset == "low")
        result.applyPreset(GraphicsPreset::Low);
    else if (preset == "medium")
        result.applyPreset(GraphicsPreset::Medium);
    else if (preset == "high")
        result.applyPreset(GraphicsPreset::High);
    const auto& scale = values["render_scale"];
    if (scale == "50" || scale == "75" || scale == "100")
        result.renderScale = scale == "50" ? 50 : scale == "75" ? 75 : 100;
    result.shadows = parseQuality(values["shadows"], result.shadows);
    result.reflections = parseQuality(values["reflections"], result.reflections);
    const auto boolean = [&](const char* key, bool& value) {
        const auto& text = values[key];
        if (text == "true" || text == "1" || text == "on")
            value = true;
        else if (text == "false" || text == "0" || text == "off")
            value = false;
    };
    boolean("ambient_occlusion", result.ambientOcclusion);
    boolean("material_detail", result.materialDetail);
    boolean("foliage_particles", result.foliageParticles);
    boolean("anti_aliasing", result.antiAliasing);
    boolean("vsync", result.vsync);
    if (values["display_mode"] == "borderless")
        result.displayMode = DisplayMode::Borderless;
    else if (values["display_mode"] == "windowed")
        result.displayMode = DisplayMode::Windowed;
    boolean("retro_mode", result.retroMode);
    boolean("dithered_shadows", result.ditheredShadows);
    boolean("distance_fog", result.distanceFog);
    return result;
}
bool GraphicsSettings::save(const std::filesystem::path& path) const {
    auto temporary = path;
    temporary += ".tmp";
    bool written = false;
    {
        std::ofstream output(temporary, std::ios::trunc);
        constexpr std::array presets = {"low", "medium", "high", "custom"};
        output << std::boolalpha << "# Food Run graphics settings\n"
               << "quality = " << presets[static_cast<int>(preset())] << '\n'
               << "render_scale = " << renderScale << '\n'
               << "shadows = " << qualityName(shadows) << '\n'
               << "reflections = " << qualityName(reflections) << '\n'
               << "ambient_occlusion = " << ambientOcclusion << '\n'
               << "material_detail = " << materialDetail << '\n'
               << "foliage_particles = " << foliageParticles << '\n'
               << "anti_aliasing = " << antiAliasing << '\n'
               << "vsync = " << vsync << '\n'
               << "display_mode = " << (displayMode == DisplayMode::Borderless ? "borderless" : "windowed") << '\n'
               << "retro_mode = " << retroMode << '\n'
               << "dithered_shadows = " << ditheredShadows << '\n'
               << "distance_fog = " << distanceFog << '\n';
        output.flush();
        written = static_cast<bool>(output);
    }
    if (!written) {
        std::error_code cleanup;
        std::filesystem::remove(temporary, cleanup);
        return false;
    }
#ifdef _WIN32
    if (MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return true;
#else
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (!error)
        return true;
#endif
    std::error_code cleanup;
    std::filesystem::remove(temporary, cleanup);
    return false;
}
} // namespace game
