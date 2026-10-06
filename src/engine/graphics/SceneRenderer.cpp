#include "engine/graphics/SceneRenderer.h"
#include "Programs.h"
#include "engine/graphics/Mesh.h"
#include "engine/graphics/Model.h"
#include "engine/graphics/ReflectionMap.h"
#include "engine/graphics/RenderTarget.h"
#include "engine/graphics/SpriteOutline.h"
#include "engine/graphics/TextureSampler.h"
#include <algorithm>
#include <cmath>
#include <glad/glad.h>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace engine::graphics {
using namespace math;
namespace {
RenderSettings checkedSettings(RenderSettings settings) {
    if (settings.worldReflectionMapSize < 0) {
        throw std::invalid_argument("World reflection map size must be nonnegative");
    }
    if (settings.planarReflectionSize < 0 || settings.maxPlanarReflections < 0 ||
        !std::isfinite(settings.planarReflectionDistance) || settings.planarReflectionDistance <= 0 ||
        !std::isfinite(settings.planarReflectionFadeDistance) || settings.planarReflectionFadeDistance < 0) {
        throw std::invalid_argument(
            "Planar reflection size/count/fade must be nonnegative and distance must be positive");
    }
    if (!std::isfinite(settings.retroPixelStep) || settings.retroPixelStep < 0 ||
        !std::isfinite(settings.modelAffineStrength) || settings.modelAffineStrength < 0 ||
        settings.modelAffineStrength > 1) {
        throw std::invalid_argument("Retro pixel step must be nonnegative and affine strength must be in [0, 1]");
    }
    if (!std::isfinite(settings.retroSnapStrength) || settings.retroSnapStrength < 0 ||
        settings.retroSnapStrength > 1 || !std::isfinite(settings.retroAffineStrength) ||
        settings.retroAffineStrength < 0 || settings.retroAffineStrength > 1) {
        throw std::invalid_argument("Retro snap and affine intensities must be in [0, 1]");
    }
    if (!std::isfinite(settings.psxFogStart) || settings.psxFogStart < 0 || !std::isfinite(settings.psxFogEnd) ||
        settings.psxFogEnd <= settings.psxFogStart) {
        throw std::invalid_argument("PSX fog requires a nonnegative start and a greater finite end");
    }
    for (Color fog : {settings.psxFogColor, settings.psxNightFogColor}) {
        for (float channel : {fog.r, fog.g, fog.b}) {
            if (!std::isfinite(channel) || channel < 0 || channel > 1) {
                throw std::invalid_argument("PSX fog colors must be finite and in [0, 1]");
            }
        }
    }
    if (!std::isfinite(settings.retroDitherStrength) || settings.retroDitherStrength < 0 ||
        settings.retroDitherStrength > 1) {
        throw std::invalid_argument("Retro dither strength must be in [0, 1]");
    }
    if (!std::isfinite(settings.psxDitheredShadowDarkness) || settings.psxDitheredShadowDarkness < 0 ||
        settings.psxDitheredShadowDarkness > 1) {
        throw std::invalid_argument("Dithered shadow darkness must be in [0, 1]");
    }
    return settings;
}
void matrix(int location, const Mat4& value) {
    glUniformMatrix4fv(location, 1, GL_FALSE, value.values.data());
}
void color(int location, Color value) {
    glUniform3f(location, value.r, value.g, value.b);
}
void texture(int location, unsigned int id, int unit = 0) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, id);
    glUniform1i(location, unit);
    glActiveTexture(GL_TEXTURE0);
}
bool reflectiveFacade(const Quad& surface) {
    return surface.texture && surface.opacity > 0 && surface.windowMaterial.worldReflections &&
           (surface.skyscraperWindows || (surface.buildingSurface && surface.materialTextures.glass));
}
Vec3 center(const Quad& surface) {
    return {surface.model.values[12], surface.model.values[13], surface.model.values[14]};
}
Vec3 mirrorPoint(Vec3 point, Vec3 normal, float planeOffset) {
    const float distance = 2 * (dot(normal, point) - planeOffset);
    return {point.x - normal.x * distance, point.y - normal.y * distance, point.z - normal.z * distance};
}
float smoothTransition(float start, float end, float value) {
    const float t = std::clamp((value - start) / (end - start), 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}
struct ScreenBounds {
    float left = 1, bottom = 1, right = -1, top = -1;
};
std::optional<ScreenBounds> facadeBounds(const Quad& surface, const Mat4& viewProjection) {
    const auto& m = multiply(viewProjection, surface.model).values;
    std::array<std::array<float, 4>, 4> corners{};
    int index = 0;
    for (float x : {-.5f, .5f}) {
        for (float y : {-.5f, .5f}) {
            corners[index++] = {m[0] * x + m[4] * y + m[12], m[1] * x + m[5] * y + m[13], m[2] * x + m[6] * y + m[14],
                                m[3] * x + m[7] * y + m[15]};
        }
    }
    // Reject the rectangle only if every corner is outside one frustum plane.
    for (int axis = 0; axis < 3; ++axis) {
        for (float sign : {-1.0f, 1.0f}) {
            if (std::all_of(corners.begin(), corners.end(),
                            [&](const auto& corner) { return corner[3] + sign * corner[axis] < 0; })) {
                return {};
            }
        }
    }
    ScreenBounds bounds;
    for (const auto& point : corners) {
        if (point[3] <= .001f) {
            return ScreenBounds{-1, -1, 1, 1}; // Conservatively retain near-plane crossings.
        }
        bounds.left = std::min(bounds.left, point[0] / point[3]);
        bounds.right = std::max(bounds.right, point[0] / point[3]);
        bounds.bottom = std::min(bounds.bottom, point[1] / point[3]);
        bounds.top = std::max(bounds.top, point[1] / point[3]);
    }
    bounds.left = std::max(bounds.left, -1.0f);
    bounds.right = std::min(bounds.right, 1.0f);
    bounds.bottom = std::max(bounds.bottom, -1.0f);
    bounds.top = std::min(bounds.top, 1.0f);
    if (bounds.right <= bounds.left || bounds.top <= bounds.bottom) {
        return {};
    }
    return bounds;
}
float facadeDistance(const Quad& surface, Vec3 point) {
    const auto& m = surface.model.values;
    const Vec3 x = {m[0], m[1], m[2]}, y = {m[4], m[5], m[6]};
    const float width = std::sqrt(dot(x, x)), height = std::sqrt(dot(y, y));
    const Vec3 tangent = normalize(x), bitangent = normalize(y);
    const auto offset = point - center(surface);
    const float u = std::clamp(dot(offset, tangent), -width * .5f, width * .5f);
    const float v = std::clamp(dot(offset, bitangent), -height * .5f, height * .5f);
    const Vec3 closestOffset = {offset.x - tangent.x * u - bitangent.x * v, offset.y - tangent.y * u - bitangent.y * v,
                                offset.z - tangent.z * u - bitangent.z * v};
    return std::sqrt(dot(closestOffset, closestOffset));
}
struct PlanarCapture {
    Vec3 normal;
    float offset = 0;
    ScreenBounds bounds;
    float score = 0;
    Mat4 viewProjection;
    RenderTarget* target = nullptr;
    float strength = 1;
};
Mesh subdividedQuad(int divisions) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve((divisions + 1) * (divisions + 1) * 5);
    indices.reserve(divisions * divisions * 6);
    for (int y = 0; y <= divisions; ++y) {
        for (int x = 0; x <= divisions; ++x) {
            const float u = static_cast<float>(x) / divisions, v = static_cast<float>(y) / divisions;
            vertices.insert(vertices.end(), {u - .5f, v - .5f, 0, u, v});
        }
    }
    for (int y = 0; y < divisions; ++y) {
        for (int x = 0; x < divisions; ++x) {
            const unsigned int lowerLeft = y * (divisions + 1) + x;
            const unsigned int upperLeft = lowerLeft + divisions + 1;
            indices.insert(indices.end(),
                           {lowerLeft, lowerLeft + 1, upperLeft + 1, lowerLeft, upperLeft + 1, upperLeft});
        }
    }
    return Mesh(vertices, 5, {{0, 3, 0}, {1, 2, 3}}, indices);
}
} // namespace
struct SceneRenderer::Impl {
    RenderSettings settings;
    detail::Programs programs;
    Mesh cube = makeCubeMesh();
    Mesh quad = makeQuadMesh();
    Mesh affineQuad2 = subdividedQuad(2);
    Mesh affineQuad4 = subdividedQuad(4);
    RenderTarget retro;
    RenderTarget shadow;
    std::unique_ptr<RenderTarget> cameraTarget;
    std::unique_ptr<ReflectionMap> reflectionMap;
    std::vector<std::unique_ptr<RenderTarget>> planarTargets;
    std::vector<PlanarCapture> planarCaptures;
    std::array<float, 4> reflectionClipPlane = {0, 0, 0, 1};
    ScreenBounds skyViewBounds = {-1, -1, 1, 1};
    float skyProjectionAspect = 0;
    bool capturingReflections = false;
    bool reflectionsReady = false;
    TextureSampler retroClamp;
    TextureSampler retroRepeat{true};
    TextureSampler materialClamp{false, true};
    TextureSampler materialRepeat{true, true};
    TextureSampler panoramaSampler;
    TextureSampler cloudSampler;
    RenderMode mode = RenderMode::Normal;
    Mat4 viewProjection;
    Mat4 lightProjection;
    Size renderSize;

    explicit Impl(RenderSettings config)
        : settings(config), retro(config.retroSize, RenderTarget::Kind::ColorDepthAlpha),
          shadow({config.shadowMapSize, config.shadowMapSize}, RenderTarget::Kind::Shadow) {
        // The supplied panorama need not have matching left/right edges. Its
        // mirrored horizontal wrap joins its edges; the horizon/pole stay clamped.
        // Keep the sampler's nearest-neighbor filtering and use only the source level.
        glSamplerParameteri(panoramaSampler.id(), GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
        glSamplerParameteri(panoramaSampler.id(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glSamplerParameterf(panoramaSampler.id(), GL_TEXTURE_MAX_LOD, 0);
    }

    bool psx() const { return mode == RenderMode::Psx; }
    float panoramaOffset(const Scene& scene) const {
        const double offset = static_cast<double>(scene.time) * scene.sky.panoramaScrollSpeed;
        // Mirrored wrapping repeats every two texture widths. Keep the shared
        // offset small without introducing a seam or tying motion to frame rate.
        return std::isfinite(offset) ? static_cast<float>(std::fmod(offset, 2.0)) : 0;
    }
    void cloudSky(const detail::CloudSkyUniforms& p, const Scene& scene, bool allow = true) const {
        const bool enabled = allow && scene.sky.hasTexturedClouds();
        glUniform1i(p.enabled, enabled);
        const auto palette = scene.sky.palette();
        color(p.topColor, palette.top);
        color(p.horizonColor, palette.horizon);
        color(p.cloudTint, palette.cloudTint);
        const auto finiteRange = [](float value, float low, float high, float fallback = 0) {
            return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
        };
        glUniform1ui(p.seed, scene.sky.layoutSeed);
        glUniform1f(p.time, std::isfinite(scene.time) ? scene.time : 0);
        const auto& stars = scene.sky.stars;
        const float starFade = smoothTransition(.6f, .95f, finiteRange(scene.sky.nightBlend, 0, 1));
        glUniform4f(p.stars, finiteRange(stars.density, 0, 1), radians(finiteRange(stars.sizeDegrees, .01f, .5f, .11f)),
                    finiteRange(stars.brightness, 0, 1), starFade);
        glUniform2f(p.twinkle, finiteRange(stars.twinkleSpeed, 0, 10), finiteRange(stars.twinkleStrength, 0, 1));
        for (std::size_t index = 0; index < scene.sky.texturedClouds.size(); ++index) {
            const auto& layer = scene.sky.texturedClouds[index];
            const bool present = enabled && layer.texture && layer.texture->id();
            const int unit = 9 + static_cast<int>(index);
            texture(p.textures[index], present ? layer.texture->id() : 0, unit);
            glBindSampler(unit, present ? cloudSampler.id() : 0);
            const float width = std::isfinite(layer.widthDegrees) ? std::clamp(layer.widthDegrees, .1f, 180.0f) : 26;
            const float requestedSpacing =
                std::isfinite(layer.repeatDegrees) ? std::clamp(layer.repeatDegrees, width, 360.0f) : 90;
            // Whole slots close the dome even when a caller supplies a non-divisor spacing.
            const float spacing = 360 / std::max(1.0f, std::floor(360 / requestedSpacing));
            const float elevation =
                std::isfinite(layer.elevationDegrees) ? std::clamp(layer.elevationDegrees, 0.0f, 90.0f) : 7;
            const float aspect = present ? layer.texture->aspect() : 1;
            glUniform4f(p.layouts[index], width / 360, spacing / 360, radians(elevation), radians(width / aspect));
            const double offset = static_cast<double>(scene.time) * layer.scrollSpeed + layer.offset;
            // Each slot now has its own identity; wrap a full turn rather than repeating one slot's artwork.
            const float phase = std::isfinite(offset) ? static_cast<float>(std::fmod(offset, 1.0)) : 0;
            const float opacity = present && std::isfinite(layer.opacity) ? std::clamp(layer.opacity, 0.0f, 1.0f) : 0;
            glUniform2f(p.settings[index], phase, opacity);
            glUniform4f(p.variation[index], finiteRange(layer.horizontalScatter, 0, .45f),
                        radians(finiteRange(layer.elevationScatterDegrees, 0, 30)),
                        finiteRange(layer.sizeVariation, 0, .5f), finiteRange(layer.curvature, 0, .4f));
            glUniform2f(p.warp[index], finiteRange(layer.warpStrength, 0, .12f), finiteRange(layer.warpSpeed, 0, 5));
        }
    }
    void clearSkyColor(const Scene& scene) const {
        const Color horizon = scene.sky.hasTexturedClouds() ? scene.sky.palette().horizon : scene.sky.bottom;
        glClearColor(horizon.r, horizon.g, horizon.b, 1);
    }
    bool distanceFog() const { return psx() && settings.psxDistanceFog; }
    const Mesh& surfaceMesh(const Quad& command) const {
        if (!psx() || !settings.psxSubdivideQuads || settings.retroAffineStrength <= 0 || !command.texture ||
            command.affineStrength <= 0) {
            return quad;
        }
        // Choose topology from the authored surface size, not its camera depth.
        // Moving a facade must not switch triangle grids and pop its UV mapping.
        const float extent = std::max(std::abs(command.worldSize.x), std::abs(command.worldSize.y));
        return extent > 6 ? affineQuad4 : extent > 2 ? affineQuad2 : quad;
    }
    template <class Program> void snap(const Program& program) const {
        glUniform3f(program.snap, renderSize.width, renderSize.height, psx() ? settings.retroPixelStep : 0);
        glUniform1f(program.snapStrength, psx() ? settings.retroSnapStrength : 0);
        glUniform2f(program.fogRange, distanceFog() ? settings.psxFogStart : 0, distanceFog() ? settings.psxFogEnd : 0);
        if constexpr (requires { program.reflectionClipPlane; }) {
            glUniform4fv(program.reflectionClipPlane, 1, reflectionClipPlane.data());
        }
    }
    float affineStrength(float commandStrength) const {
        return psx() ? std::clamp(commandStrength, 0.0f, 1.0f) * settings.retroAffineStrength : 0;
    }
    template <class Program> void surfaceTexture(const Program& program, const Texture& asset) const {
        texture(program.texture, asset.id());
        glBindSampler(0, psx() ? (asset.repeats() ? retroRepeat.id() : retroClamp.id()) : 0);
    }
    template <class Program> void psxMinification(const Program& program, const Quad& command) const {
        const float bias = command.psxMinificationLodBias.value_or(0);
        glUniform1i(program.textureRepeat, command.texture->repeats());
        glUniform1i(program.maxTextureLod, command.texture->maxMipLevel());
        glUniform1i(program.psxMinification, psx() && command.psxMinificationLodBias && std::isfinite(bias));
        glUniform1f(program.psxLodBias, std::isfinite(bias) ? std::clamp(bias, -2.0f, 2.0f) : 0);
    }
    Color fogColor(const Scene& scene) const {
        if (settings.psxFogFollowsSky && scene.sky.hasTexturedClouds()) {
            return scene.sky.palette().horizon;
        }
        const float night = std::floor(std::clamp(scene.sky.nightBlend, 0.0f, 1.0f) * 4.0f + 0.5f) / 4.0f;
        const Color day = settings.psxFogFollowsSky ? scene.sky.bottom : settings.psxFogColor;
        const Color dark = settings.psxFogFollowsSky ? Color{.38f, .31f, .40f} : settings.psxNightFogColor;
        return {day.r * (1 - night) + dark.r * night, day.g * (1 - night) + dark.g * night,
                day.b * (1 - night) + dark.b * night};
    }
    template <class Program> void lit(const Program& program, const Mat4& model, const Scene& scene) const {
        program.shader.bind();
        matrix(program.mvp, multiply(viewProjection, model));
        matrix(program.model, model);
        matrix(program.lightViewProj, lightProjection);
        texture(program.shadowMap, shadow.texture(), 1);
        glUniform1f(program.shadowDitherStrength, psx() ? settings.retroDitherStrength : 0);
        glUniform1i(program.ditheredShadows, psx() && settings.psxDitheredShadows);
        glUniform1f(program.ditheredShadowDarkness, settings.psxDitheredShadowDarkness);
        color(program.fogColor, fogColor(scene));
        snap(program);
    }
    template <class Program> void sunlight(const Program& program, const DirectionalLight& light) const {
        const Vec3 sun = normalize(light.direction);
        glUniform3f(program.sunDir, sun.x, sun.y, sun.z);
        glUniform1f(program.ambient, light.ambient);
    }
    template <class Program> void materialFinish(const Program& p, const SurfaceMaterial& material) const {
        glUniform1f(p.surfaceRoughness, std::clamp(material.roughness, 0.0f, 1.0f));
        glUniform1f(p.surfaceMetallic, std::clamp(material.metallic, 0.0f, 1.0f));
        glUniform1f(p.surfaceReflectivity, std::clamp(material.reflectivity, 0.0f, 1.0f));
    }
    template <class Program> void materialMaps(const Program& p, const MaterialTextures& maps) const {
        const auto& u = p.maps;
        const auto bind = [&](int location, int present, int maxLod, const Texture* image, int unit) {
            const bool enabled = image && image->id();
            texture(location, enabled ? image->id() : 0, unit);
            glBindSampler(unit, enabled && psx() ? (image->repeats() ? materialRepeat.id() : materialClamp.id()) : 0);
            glUniform1i(present, enabled);
            glUniform1i(maxLod, enabled ? image->maxMipLevel() : 0);
        };
        bind(u.properties, u.hasProperties, u.maxPropertiesLod, maps.properties, 2);
        bind(u.normal, u.hasNormal, u.maxNormalLod, maps.normal, 3);
        bind(u.occlusion, u.hasOcclusion, u.maxOcclusionLod, maps.occlusion, 4);
        if constexpr (requires { p.glassMap; }) {
            bind(p.glassMap, p.hasGlassMap, p.maxGlassLod, maps.glass, 5);
        }
        glUniform1i(u.packedReflectivity, maps.packedReflectivity);
        glUniform1i(u.retro, psx());
        glUniform1f(u.normalScale, std::isfinite(maps.normalScale) ? maps.normalScale : 0);
        glUniform1f(u.occlusionStrength,
                    std::isfinite(maps.occlusionStrength) ? std::clamp(maps.occlusionStrength, 0.0f, 1.0f) : 0);
    }
    template <class Program> void materialEnvironment(const Program& p, const Scene& scene) const {
        const auto sun = normalize(scene.light.direction);
        glUniform3f(p.sunDir, sun.x, sun.y, sun.z);
        glUniform3f(p.cameraPosition, scene.camera.position.x, scene.camera.position.y, scene.camera.position.z);
        if (scene.sky.hasTexturedClouds()) {
            const auto palette = scene.sky.palette();
            color(p.skyColor, palette.top);
            color(p.hazeColor, palette.horizon);
            color(p.sunColor, scene.sky.sunColor);
            return;
        }
        const float night = std::floor(std::clamp(scene.sky.nightBlend, 0.0f, 1.0f) * 4.0f + 0.5f) / 4.0f;
        color(p.skyColor,
              {scene.sky.top.r * (1 - night) + 0.045f * night, scene.sky.top.g * (1 - night) + 0.055f * night,
               scene.sky.top.b * (1 - night) + 0.14f * night});
        color(p.hazeColor,
              {scene.sky.bottom.r * (1 - night) + 0.38f * night, scene.sky.bottom.g * (1 - night) + 0.31f * night,
               scene.sky.bottom.b * (1 - night) + 0.40f * night});
        color(p.sunColor, scene.sky.sunColor);
    }
    void beginMaterial(const Mat4& model, const Scene& scene, const SurfaceMaterial& finish) const {
        const auto& p = programs.material;
        lit(p, model, scene);
        materialEnvironment(p, scene);
        materialFinish(p, finish);
        materialMaps(p, {});
        glUniform1f(p.ambient, scene.light.ambient);
        glUniform1i(p.vertexLighting, mode != RenderMode::Normal);
        glUniform1i(p.quad, false);
        glUniform3f(p.faceNormal, 0, 1, 0);
        glUniform1i(p.hasTexture, false);
        texture(p.texture, 0);
        glBindSampler(0, 0);
        glUniform1i(p.textureRepeat, false);
        glUniform1i(p.maxTextureLod, 0);
        glUniform1i(p.psxMinification, false);
        glUniform1f(p.psxLodBias, 0);
        glUniform2f(p.texScale, 1, 1);
        glUniform2f(p.texOffset, 0, 0);
        glUniform1f(p.lod, -1);
        glUniform1i(p.colorIsLinear, false);
        glUniform1i(p.receivesShadow, true);
        glUniform1f(p.affine, 0);
        glUniform1f(p.alpha, 1);
        glUniform1f(p.alphaCutoff, 0);
        glUniform1f(p.baseAlpha, 1);
        glUniform1i(p.doubleSided, false);
        glUniform1f(p.haze, 0);
        glUniform1f(p.time, scene.time);
        glUniform1f(p.heat, 0);
    }
    void shadowDraw(const Mat4& model, unsigned int textureId = 0, float opacity = 1, float alphaCutoff = .1f,
                    bool modelMesh = false, Vec2 texScale = {1, 1}, Vec2 texOffset = {}, float baseAlpha = 1) const {
        const auto& p = programs.shadow;
        p.shader.bind();
        matrix(p.mvp, multiply(lightProjection, model));
        glUniform1i(p.textured, textureId != 0);
        glUniform1f(p.opacity, opacity);
        glUniform1f(p.alphaCutoff, alphaCutoff);
        glUniform1f(p.baseAlpha, baseAlpha);
        glUniform1i(p.modelMesh, modelMesh);
        glUniform2f(p.texScale, texScale.x, texScale.y);
        glUniform2f(p.texOffset, texOffset.x, texOffset.y);
        texture(p.texture, textureId);
        glBindSampler(0, 0);
    }
    void cast(const Quad& command) const {
        if (!command.castsShadow) {
            return;
        }
        shadowDraw(command.model, command.alphaCutoff > 0 && command.texture ? command.texture->id() : 0,
                   command.opacity, command.alphaCutoff, false, command.textureScale, command.textureOffset);
        quad.draw();
    }
    void cast(const Cube& command) const {
        shadowDraw(command.model);
        cube.draw();
    }
    void cast(const ModelInstance& command) const {
        if (!command.model) {
            return;
        }
        for (const auto& primitive : command.model->primitives()) {
            const auto& material = primitive.material;
            shadowDraw(command.transform, material.alphaCutoff > 0 ? material.texture.id() : 0, 1, material.alphaCutoff,
                       true, {1, 1}, {}, material.baseColor[3]);
            primitive.mesh.draw();
        }
    }
    void cast(const Sprite& command) const {
        if (!command.texture) {
            return;
        }
        shadowDraw(command.model, command.texture->id());
        (command.profile ? *command.profile : quad).draw();
    }
    void draw(const Quad& command, const Scene& scene) const {
        const Vec3 normal = normalize(command.normal);
        const float ambient = scene.light.ambientForNormal(normal);
        const float light = command.receivesShadow ? scene.light.lightForNormal(normal) : 1.0f;
        if (command.pbrSurface && !command.skyscraperWindows && !command.buildingSurface) {
            const auto& p = programs.material;
            beginMaterial(command.model, scene, command.surfaceMaterial);
            materialMaps(p, command.materialTextures);
            glUniform1i(p.quad, true);
            glUniform3f(p.faceNormal, normal.x, normal.y, normal.z);
            color(p.color, command.color);
            if (command.texture) {
                surfaceTexture(p, *command.texture);
                psxMinification(p, command);
                glUniform1i(p.hasTexture, true);
                // Use the pixel's UV footprint in Normal mode; a single rounded
                // LOD for an entire curved segment loses near asphalt detail.
                glUniform1f(p.lod, mode != RenderMode::Normal ? 0 : -1);
            }
            glUniform2f(p.texScale, command.textureScale.x, command.textureScale.y);
            glUniform2f(p.texOffset, command.textureOffset.x, command.textureOffset.y);
            glUniform1i(p.receivesShadow, command.receivesShadow);
            glUniform1f(p.alpha, command.opacity);
            glUniform1f(p.alphaCutoff, command.alphaCutoff);
            glUniform1i(p.doubleSided, command.doubleSided);
            glUniform1f(p.haze, std::clamp(command.haze, 0.0f, 1.0f));
            glUniform1f(p.affine, affineStrength(command.affineStrength));
            glUniform1f(p.heat, mode == RenderMode::Normal ? command.heat : 0);
            surfaceMesh(command).draw();
            return;
        }
        if (command.texture) {
            if (command.skyscraperWindows || command.buildingSurface) {
                const auto& p = programs.skyscraper;
                lit(p, command.model, scene);
                surfaceTexture(p, *command.texture);
                psxMinification(p, command);
                color(p.color, command.color);
                glUniform3f(p.faceNormal, normal.x, normal.y, normal.z);
                materialEnvironment(p, scene);
                glUniform1i(p.hasWindows, command.skyscraperWindows);
                glUniform1f(p.haze, std::clamp(command.haze, 0.0f, 1.0f));
                glUniform1f(p.windowRoughness, std::clamp(command.windowMaterial.roughness, 0.0f, 1.0f));
                glUniform1f(p.windowReflectivity, std::clamp(command.windowMaterial.reflectivity, 0.0f, 1.0f));
                materialFinish(p, command.surfaceMaterial);
                materialMaps(p, command.materialTextures);
                // Unit 6 is distinct from albedo, shadow and linear material data.
                const bool worldReflection =
                    reflectionsReady && !capturingReflections && command.windowMaterial.worldReflections;
                glActiveTexture(GL_TEXTURE6);
                glBindTexture(GL_TEXTURE_CUBE_MAP, worldReflection ? reflectionMap->texture() : 0);
                glBindSampler(6, 0);
                glUniform1i(p.worldReflection, 6);
                glUniform1i(p.hasWorldReflection, worldReflection);
                glUniform1f(p.worldReflectionMaxLod, worldReflection ? reflectionMap->maxMipLevel() : 0);
                glActiveTexture(GL_TEXTURE0);
                const PlanarCapture* planar = nullptr;
                if (!capturingReflections && command.windowMaterial.worldReflections) {
                    const float offset = dot(normal, center(command));
                    for (const auto& capture : planarCaptures) {
                        if (dot(normal, capture.normal) > .9999f && std::abs(offset - capture.offset) < .01f) {
                            planar = &capture;
                            break;
                        }
                    }
                }
                texture(p.planarReflection, planar ? planar->target->texture() : 0, 7);
                glBindSampler(7, 0);
                glUniform1i(p.hasPlanarReflection, planar != nullptr);
                float planarStrength = planar ? planar->strength : 0;
                if (planar && settings.planarReflectionFadeDistance > 0) {
                    const float fadeStart =
                        std::max(0.0f, settings.planarReflectionDistance - settings.planarReflectionFadeDistance);
                    planarStrength *= 1 - smoothTransition(fadeStart, settings.planarReflectionDistance,
                                                           facadeDistance(command, scene.camera.position));
                }
                glUniform1f(p.planarReflectionStrength, planarStrength);
                matrix(p.planarReflectionViewProjection, planar ? planar->viewProjection : identity());
                glUniform1f(p.planarReflectionMaxLod, planar ? planar->target->maxMipLevel() : 0);
                glUniform1f(p.windowReflectance, std::isfinite(command.windowMaterial.reflectance)
                                                     ? std::clamp(command.windowMaterial.reflectance, 0.0f, 1.0f)
                                                     : 0);
                glUniform1f(p.windowBackingTint, std::isfinite(command.windowMaterial.backingTint)
                                                     ? std::clamp(command.windowMaterial.backingTint, 0.0f, 1.0f)
                                                     : 0);
                const auto* panorama = scene.sky.panorama;
                const bool skyReflection = !capturingReflections && settings.worldReflectionMapSize &&
                                           command.windowMaterial.worldReflections && panorama && panorama->id();
                texture(p.panorama, skyReflection ? panorama->id() : 0, 8);
                glBindSampler(8, skyReflection ? panoramaSampler.id() : 0);
                glUniform1f(p.panoramaOffset, panoramaOffset(scene));
                glUniform1i(p.hasSkyPanorama, skyReflection);
                glUniform1f(p.skyPanoramaNight, std::floor(std::clamp(scene.sky.nightBlend, 0.0f, 1.0f) * 4 + .5f) / 4);
                cloudSky(p.clouds, scene,
                         !capturingReflections && settings.worldReflectionMapSize &&
                             command.windowMaterial.worldReflections);
                glUniform1i(p.receivesShadow, command.receivesShadow);
                glUniform2f(p.texScale, command.textureScale.x, command.textureScale.y);
                glUniform2f(p.texOffset, command.textureOffset.x, command.textureOffset.y);
                glUniform2f(p.facadeVerticalRange, command.facadeVerticalRange.x, command.facadeVerticalRange.y);
                glUniform1f(p.lod, mode != RenderMode::Normal ? 0 : -1);
                glUniform1f(p.ambientLight, ambient);
                glUniform1f(p.alpha, command.opacity);
                glUniform1f(p.alphaCutoff, command.alphaCutoff);
                glUniform1f(p.affine, affineStrength(command.affineStrength));
            } else {
                const auto& p = programs.road;
                lit(p, command.model, scene);
                surfaceTexture(p, *command.texture);
                psxMinification(p, command);
                color(p.color, command.color);
                glUniform1i(p.receivesShadow, command.receivesShadow);
                glUniform2f(p.texScale, command.textureScale.x, command.textureScale.y);
                glUniform2f(p.texOffset, command.textureOffset.x, command.textureOffset.y);
                glUniform1f(p.lod, mode != RenderMode::Normal ? 0 : -1);
                glUniform1f(p.light, light);
                glUniform1f(p.ambientLight, ambient);
                glUniform1f(p.alpha, command.opacity);
                glUniform1f(p.alphaCutoff, command.alphaCutoff);
                glUniform1f(p.time, scene.time);
                glUniform1f(p.heat, mode != RenderMode::Normal ? 0 : command.heat);
                glUniform1f(p.affine, affineStrength(command.affineStrength));
            }
        } else {
            const auto& p = programs.plain;
            lit(p, command.model, scene);
            color(p.color, command.color);
            glUniform1f(p.alpha, command.opacity);
            glUniform1f(p.light, light);
            glUniform1f(p.ambientLight, ambient);
            glUniform1i(p.receivesShadow, command.receivesShadow);
        }
        surfaceMesh(command).draw();
    }
    void draw(const Cube& command, const Scene& scene) const {
        if (command.pbrSurface) {
            beginMaterial(command.model, scene, command.surfaceMaterial);
            color(programs.material.color, command.color);
            cube.draw();
            return;
        }
        const auto& p = programs.cube;
        lit(p, command.model, scene);
        sunlight(p, scene.light);
        glUniform1i(p.vertexLighting, mode != RenderMode::Normal);
        color(p.color, command.color);
        cube.draw();
    }
    void draw(const ModelInstance& command, const Scene& scene) const {
        if (!command.model) {
            return;
        }
        if (command.pbrSurface) {
            const auto& p = programs.material;
            beginMaterial(command.transform, scene, SurfaceMaterial{});
            glUniform1i(p.colorIsLinear, true);
            glUniform1f(p.affine, affineStrength(settings.modelAffineStrength));
            for (const auto& primitive : command.model->primitives()) {
                const auto& material = primitive.material;
                materialFinish(p, command.materialOverride.value_or(material.surfaceMaterial));
                materialMaps(p, material.textures());
                glUniform3f(p.color, material.baseColor[0], material.baseColor[1], material.baseColor[2]);
                glUniform1i(p.hasTexture, material.texture.id() != 0);
                glUniform1f(p.alphaCutoff, material.alphaCutoff);
                glUniform1f(p.baseAlpha, material.baseColor[3]);
                glUniform1i(p.doubleSided, material.doubleSided);
                surfaceTexture(p, material.texture);
                primitive.mesh.draw();
            }
            return;
        }
        const auto& p = programs.model;
        lit(p, command.transform, scene);
        sunlight(p, scene.light);
        glUniform1i(p.vertexLighting, mode != RenderMode::Normal);
        glUniform1f(p.affine, affineStrength(settings.modelAffineStrength));
        for (const auto& primitive : command.model->primitives()) {
            const auto& material = primitive.material;
            glUniform1f(p.hasTexture, material.texture.id() != 0);
            glUniform1f(p.alphaCutoff, material.alphaCutoff);
            glUniform1f(p.baseAlpha, material.baseColor[3]);
            glUniform1i(p.doubleSided, material.doubleSided);
            glUniform3f(p.baseColor, material.baseColor[0], material.baseColor[1], material.baseColor[2]);
            surfaceTexture(p, material.texture);
            primitive.mesh.draw();
        }
    }
    void draw(const Sprite& command, const Scene& scene) const {
        if (!command.texture) {
            return;
        }
        const auto& p = programs.sprite;
        p.shader.bind();
        matrix(p.mvp, multiply(viewProjection, command.model));
        matrix(p.model, command.model);
        glUniform1i(p.extruded, command.profile != nullptr);
        glUniform1f(p.outlinePixels, command.outlinePixels);
        const auto lightDirection = normalize(scene.light.direction);
        glUniform3f(p.sunDir, lightDirection.x, lightDirection.y, lightDirection.z);
        snap(p);
        color(p.fogColor, fogColor(scene));
        glUniform1f(p.affine, affineStrength(command.affineStrength));
        glUniform1f(p.time, scene.time);
        glUniform1f(p.spawnTime, command.spawnTime);
        color(p.rippleColor, command.rippleColor);
        glUniform1f(p.aspect, command.aspect);
        surfaceTexture(p, *command.texture);
        if (psx()) {
            // Alpha marks dither eligibility in the retro target. Keep normal
            // sprite color blending while clearing the mask only where depth/cutout tests pass.
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ZERO);
        }
        (command.profile ? *command.profile : quad).draw();
        if (psx()) {
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }
    }
    void drawRippleShape(const Mat4& model, const SpriteOutline* shape, Color tint, float width, float opacity,
                         float phase, float burstProgress = -1) const {
        if (!shape || !shape->distanceField().id() || opacity <= 0 || width <= 0) {
            return;
        }
        const auto& p = programs.ripple;
        p.shader.bind();
        matrix(p.mvp, multiply(viewProjection, multiply(model, shape->transform())));
        snap(p);
        // The field stores distances, not color texels: retain linear reconstruction in every mode.
        glBindSampler(0, 0);
        texture(p.distanceField, shape->distanceField().id());
        color(p.color, tint);
        glUniform1f(p.radius, shape->radius());
        glUniform1f(p.phase, phase);
        glUniform1f(p.width, width);
        glUniform1f(p.opacity, std::clamp(opacity, 0.0f, 1.0f));
        glUniform1i(p.burst, burstProgress >= 0);
        glUniform1f(p.burstProgress, std::max(burstProgress, 0.0f));
        if (psx()) {
            // Sprite ripples and collection bursts also bypass color dithering.
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ZERO);
        }
        quad.draw();
        if (psx()) {
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
        }
    }
    void drawRipple(const Sprite& sprite, const Scene& scene) const {
        const auto& ripple = sprite.ripple;
        if (!std::isfinite(ripple.period) || ripple.period <= 0) {
            return;
        }
        const float phase = std::fmod(std::max(scene.time - sprite.spawnTime, 0.0f), ripple.period) / ripple.period;
        drawRippleShape(sprite.model, ripple.shape, sprite.rippleColor, ripple.widthPixels, ripple.opacity, phase);
    }
    void draw(const SpriteBurst& burst, const Scene&) const {
        if (!std::isfinite(burst.progress) || burst.progress < 0 || burst.progress >= 1) {
            return;
        }
        drawRippleShape(burst.model, burst.shape, burst.color, burst.widthPixels, burst.opacity, burst.initialPhase,
                        burst.progress);
    }
    void background(const Scene& scene) const {
        glDepthMask(GL_FALSE);
        const auto& sky = programs.sky;
        sky.shader.bind();
        cloudSky(sky.clouds, scene);
        const bool panorama = scene.sky.panorama && scene.sky.panorama->id();
        texture(sky.panorama, panorama ? scene.sky.panorama->id() : 0);
        glBindSampler(0, panorama ? panoramaSampler.id() : 0);
        glUniform1f(sky.panoramaOffset, panoramaOffset(scene));
        glUniform1i(sky.hasPanorama, panorama);
        color(sky.topColor, scene.sky.top);
        color(sky.bottomColor, scene.sky.bottom);
        glUniform1f(sky.time, scene.time);
        glUniform1f(sky.nightBlend, scene.sky.nightBlend);
        const Vec3 sunDirection = normalize(scene.light.direction);
        glUniform3f(sky.sunDir, sunDirection.x, sunDirection.y, sunDirection.z);
        glUniform1f(sky.aspect, skyProjectionAspect > 0 ? skyProjectionAspect
                                                        : static_cast<float>(renderSize.width) / renderSize.height);
        glUniform4f(sky.viewBounds, skyViewBounds.left, skyViewBounds.bottom, skyViewBounds.right, skyViewBounds.top);
        glUniform2f(sky.rasterSize, static_cast<float>(renderSize.width), static_cast<float>(renderSize.height));
        glUniform1f(sky.cloudCoverage, std::clamp(scene.sky.cloudCoverage, 0.0f, 1.0f));
        const Vec3 forward = normalize(scene.camera.target - scene.camera.position);
        const Vec3 right = normalize(cross(forward, scene.camera.up));
        const Vec3 up = cross(right, forward);
        glUniform3f(sky.cameraForward, forward.x, forward.y, forward.z);
        glUniform3f(sky.cameraRight, right.x, right.y, right.z);
        glUniform3f(sky.cameraUp, up.x, up.y, up.z);
        glUniform3f(sky.cameraPosition, scene.camera.position.x, scene.camera.position.y, scene.camera.position.z);
        glUniform1f(sky.tanHalfFov, std::tan(radians(scene.camera.verticalFov) * .5f));
        color(sky.sunColor, scene.sky.sunColor);
        // Angular discs keep their shape at every bearing and can sit behind
        // the cloud layers instead of being added over the completed sky.
        const float radius = std::max(scene.sky.sunSize, 0.0f) * .5f / std::max(scene.sky.sunDistance, .001f);
        glUniform2f(sky.sunRadii, radius * std::clamp(.38f + scene.sky.sunCore * .015f, .38f, .54f),
                    radius * std::clamp(.60f + scene.sky.sunHaze * .05f, .62f, .76f));
        quad.draw();
        glBindSampler(0, 0);
        glDepthMask(GL_TRUE);
    }
    bool reflectionBoxVisible(const Mat4& model, Vec3 halfExtent) const {
        const auto& world = model.values;
        const auto& plane = reflectionClipPlane;
        const float signedDistance = plane[0] * world[12] + plane[1] * world[13] + plane[2] * world[14] + plane[3];
        const float planeRadius =
            std::abs(plane[0] * world[0] + plane[1] * world[1] + plane[2] * world[2]) * halfExtent.x +
            std::abs(plane[0] * world[4] + plane[1] * world[5] + plane[2] * world[6]) * halfExtent.y +
            std::abs(plane[0] * world[8] + plane[1] * world[9] + plane[2] * world[10]) * halfExtent.z;
        if (signedDistance + planeRadius < 0) {
            return false;
        }
        const auto& clip = multiply(viewProjection, model).values;
        for (int axis = 0; axis < 3; ++axis) {
            for (float sign : {-1.0f, 1.0f}) {
                const float distance = clip[15] + sign * clip[12 + axis];
                const float radius = std::abs(clip[3] + sign * clip[axis]) * halfExtent.x +
                                     std::abs(clip[7] + sign * clip[4 + axis]) * halfExtent.y +
                                     std::abs(clip[11] + sign * clip[8 + axis]) * halfExtent.z;
                if (distance + radius < 0) {
                    return false;
                }
            }
        }
        return true;
    }
    bool reflectionVisible(const Quad& surface) const {
        return surface.opacity > 0 && reflectionBoxVisible(surface.model, {.5f, .5f, 0});
    }
    bool reflectionVisible(const Cube& object) const { return reflectionBoxVisible(object.model, {.5f, .5f, .5f}); }
    bool reflectionVisible(const Sprite& object) const { return reflectionBoxVisible(object.model, {.6f, .6f, .6f}); }
    bool reflectionVisible(const ModelInstance&) const {
        return true; // Imported models may extend beyond a unit box; never truncate props with guessed bounds.
    }
    void drawReflectionWorld(const Scene& world, const Scene& camera) const {
        for (const auto& object : world.objects) {
            std::visit(
                [&](const auto& item) {
                    if (reflectionVisible(item)) {
                        draw(item, camera);
                    }
                },
                object);
        }
    }
    void drawParticles(const Scene& world, const Scene& camera) const {
        if (world.particles.empty()) {
            return;
        }
        std::vector<const Quad*> ordered;
        ordered.reserve(world.particles.size());
        for (const auto& particle : world.particles) {
            if (particle.opacity > .001f && (!capturingReflections || reflectionVisible(particle))) {
                ordered.push_back(&particle);
            }
        }
        const auto forward = normalize(camera.camera.target - camera.camera.position);
        const auto depth = [&](const Quad* particle) {
            return dot(center(*particle) - camera.camera.position, forward);
        };
        std::stable_sort(ordered.begin(), ordered.end(),
                         [&](const auto* a, const auto* b) { return depth(a) > depth(b); });
        glDepthMask(GL_FALSE);
        // Texture particles use ordinary alpha blending. Their sprite pixels reduce PSX dither coverage as they fade.
        if (psx()) {
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
        }
        for (const auto* particle : ordered) {
            draw(*particle, camera);
        }
        glDepthMask(GL_TRUE);
        if (psx()) {
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }
    }
    void renderReflectionWorld(const Scene& world, const Scene& camera) const {
        // Only opaque quads/cubes contribute to this depth prepass. Sprites and
        // imported props keep their normal alpha behavior over the captured sky.
        const auto& depth = programs.plain;
        depth.shader.bind();
        snap(depth);
        glUniform1i(depth.receivesShadow, false);
        glEnable(GL_CLIP_DISTANCE0);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        for (const auto& object : world.objects) {
            std::visit(
                [&](const auto& item) {
                    using T = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<T, Quad> || std::is_same_v<T, Cube>) {
                        if constexpr (std::is_same_v<T, Quad>) {
                            if (item.opacity < 1 || item.alphaCutoff > 0) {
                                return;
                            }
                        }
                        if (!reflectionVisible(item)) {
                            return;
                        }
                        matrix(depth.mvp, multiply(viewProjection, item.model));
                        matrix(depth.model, item.model);
                        if constexpr (std::is_same_v<T, Quad>) {
                            quad.draw();
                        } else {
                            cube.draw();
                        }
                    }
                },
                object);
        }
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDisable(GL_CLIP_DISTANCE0);
        glDepthFunc(GL_LEQUAL);
        glDepthRange(1, 1);
        background(camera);
        glDepthRange(0, 1);
        glEnable(GL_CLIP_DISTANCE0);
        drawReflectionWorld(world, camera);
        drawParticles(world, camera);
        glDisable(GL_CLIP_DISTANCE0);
        glDepthFunc(GL_LESS);
    }
    void captureReflections(const Scene& scene) {
        reflectionsReady = false;
        // Authored skies are sampled directly by distant panes. Only local
        // planar views render geometry; no six-face sky/world bake is needed.
        if (!settings.worldReflectionMapSize || scene.sky.hasTexturedClouds() ||
            (scene.sky.panorama && scene.sky.panorama->id())) {
            return;
        }
        const bool hasGlass = std::any_of(scene.objects.begin(), scene.objects.end(), [](const auto& object) {
            const auto* surface = std::get_if<Quad>(&object);
            return surface && surface->texture && surface->opacity > 0 && surface->windowMaterial.worldReflections &&
                   (surface->skyscraperWindows || (surface->buildingSurface && surface->materialTextures.glass));
        });
        if (!hasGlass) {
            return;
        }
        if (!reflectionMap) {
            reflectionMap = std::make_unique<ReflectionMap>(settings.worldReflectionMapSize);
        }
        // The six cameras share the current sun shadow. Capture only the world:
        // no screen-space lens, fog, console snapping, menu or additive effects.
        Scene probe;
        probe.camera = scene.camera;
        probe.light = scene.light;
        probe.sky = scene.sky;
        probe.time = scene.time;
        probe.camera.position = scene.reflectionProbePosition.value_or(scene.camera.position);
        probe.camera.verticalFov = 90;
        probe.camera.nearPlane = .1f;
        const auto savedMode = mode;
        const auto savedSize = renderSize;
        const auto savedProjection = viewProjection;
        mode = RenderMode::Normal;
        renderSize = {reflectionMap->faceSize(), reflectionMap->faceSize()};
        capturingReflections = true;
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        glBindSampler(6, 0);
        glActiveTexture(GL_TEXTURE0);
        constexpr Vec3 directions[] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        constexpr Vec3 up[] = {{0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}};
        for (int face = 0; face < 6; ++face) {
            const auto origin = probe.camera.position;
            probe.camera.target = {origin.x + directions[face].x, origin.y + directions[face].y,
                                   origin.z + directions[face].z};
            probe.camera.up = up[face];
            viewProjection = probe.camera.viewProjection(1);
            reflectionMap->bindFace(face);
            clearSkyColor(scene);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderReflectionWorld(scene, probe);
        }
        reflectionMap->generateMipmaps();
        mode = savedMode;
        renderSize = savedSize;
        viewProjection = savedProjection;
        capturingReflections = false;
        reflectionsReady = true;
    }
    void capturePlanarReflections(const Scene& scene) {
        planarCaptures.clear();
        if (!settings.worldReflectionMapSize || !settings.planarReflectionSize || !settings.maxPlanarReflections) {
            return;
        }
        for (const auto& object : scene.objects) {
            const auto* surface = std::get_if<Quad>(&object);
            if (!surface || !reflectiveFacade(*surface)) {
                continue;
            }
            const auto normal = normalize(surface->normal);
            const float offset = dot(normal, center(*surface));
            if (dot(normal, scene.camera.position) - offset <= .05f) {
                continue; // The facade must face the viewer.
            }
            const float distance = facadeDistance(*surface, scene.camera.position);
            if (distance > settings.planarReflectionDistance) {
                continue;
            }
            const auto bounds = facadeBounds(*surface, viewProjection);
            if (!bounds) {
                continue;
            }
            const float score = (bounds->right - bounds->left) * (bounds->top - bounds->bottom) *
                                (1 - std::clamp(surface->haze, 0.0f, 1.0f)) *
                                (1 - .6f * distance / settings.planarReflectionDistance);
            auto samePlane = std::find_if(planarCaptures.begin(), planarCaptures.end(), [&](const auto& plane) {
                return dot(normal, plane.normal) > .9999f && std::abs(offset - plane.offset) < .01f;
            });
            if (samePlane != planarCaptures.end()) {
                samePlane->score += score;
                samePlane->bounds.left = std::min(samePlane->bounds.left, bounds->left);
                samePlane->bounds.bottom = std::min(samePlane->bounds.bottom, bounds->bottom);
                samePlane->bounds.right = std::max(samePlane->bounds.right, bounds->right);
                samePlane->bounds.top = std::max(samePlane->bounds.top, bounds->top);
            } else {
                planarCaptures.push_back({normal, offset, *bounds, score, {}});
            }
        }
        std::stable_sort(planarCaptures.begin(), planarCaptures.end(),
                         [](const auto& a, const auto& b) { return a.score > b.score; });
        const auto count = std::min(planarCaptures.size(), static_cast<std::size_t>(settings.maxPlanarReflections));
        // Competing planes fade out before losing their capture slot, avoiding
        // an abrupt change when similarly visible facades exchange priority.
        const float cutoff = count < planarCaptures.size() ? planarCaptures[count].score : 0;
        planarCaptures.resize(count);
        if (cutoff > 0) {
            for (auto& capture : planarCaptures) {
                capture.strength = smoothTransition(cutoff, cutoff * 1.5f, capture.score);
            }
        }
        if (planarCaptures.empty()) {
            return;
        }
        const auto savedMode = mode;
        const auto savedSize = renderSize;
        const auto savedProjection = viewProjection;
        const float aspect = static_cast<float>(savedSize.width) / savedSize.height;
        const float density =
            static_cast<float>(settings.planarReflectionSize) / std::max(savedSize.width, savedSize.height);
        mode = RenderMode::Normal;
        capturingReflections = true;
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindSampler(7, 0);
        glActiveTexture(GL_TEXTURE0);
        Scene reflected;
        reflected.light = scene.light;
        reflected.sky = scene.sky;
        reflected.time = scene.time;
        for (std::size_t index = 0; index < planarCaptures.size(); ++index) {
            auto& capture = planarCaptures[index];
            // Mirroring reverses handedness. lookAt's right-handed basis flips
            // screen X, so reflect the visible facade rectangle before cropping.
            skyViewBounds = {-capture.bounds.right, capture.bounds.bottom, -capture.bounds.left, capture.bounds.top};
            skyProjectionAspect = aspect;
            const float width = skyViewBounds.right - skyViewBounds.left;
            const float height = skyViewBounds.top - skyViewBounds.bottom;
            renderSize = {std::max(1, static_cast<int>(std::ceil(width * .5f * savedSize.width * density))),
                          std::max(1, static_cast<int>(std::ceil(height * .5f * savedSize.height * density)))};
            // Small viewport changes during motion must not reallocate six
            // framebuffer textures on every frame. Keep resolution in buckets.
            renderSize.width = (renderSize.width + 31) / 32 * 32;
            renderSize.height = (renderSize.height + 31) / 32 * 32;
            if (index >= planarTargets.size()) {
                planarTargets.push_back(nullptr);
            }
            if (!planarTargets[index] || planarTargets[index]->size().width != renderSize.width ||
                planarTargets[index]->size().height != renderSize.height) {
                planarTargets[index] = std::make_unique<RenderTarget>(renderSize, RenderTarget::Kind::Reflection);
            }
            capture.target = planarTargets[index].get();
            reflected.camera = scene.camera;
            reflected.camera.position = mirrorPoint(scene.camera.position, capture.normal, capture.offset);
            reflected.camera.target = mirrorPoint(scene.camera.target, capture.normal, capture.offset);
            reflected.camera.up = mirrorPoint(scene.camera.up, capture.normal, 0);
            reflected.camera.nearPlane = .1f;
            Mat4 crop = identity();
            crop.values[0] = 2 / width;
            crop.values[5] = 2 / height;
            crop.values[12] = (capture.bounds.left + capture.bounds.right) / width;
            crop.values[13] = -(capture.bounds.top + capture.bounds.bottom) / height;
            // Sampling and rendering must use this same mirrored, cropped
            // projection so objects stay attached to their correct facade positions.
            viewProjection = multiply(crop, reflected.camera.viewProjection(aspect));
            capture.viewProjection = viewProjection;
            reflectionClipPlane = {capture.normal.x, capture.normal.y, capture.normal.z, -capture.offset - .02f};
            capture.target->bind();
            clearSkyColor(scene);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderReflectionWorld(scene, reflected);
            capture.target->generateMipmaps();
        }
        mode = savedMode;
        renderSize = savedSize;
        viewProjection = savedProjection;
        reflectionClipPlane = {0, 0, 0, 1};
        skyViewBounds = {-1, -1, 1, 1};
        skyProjectionAspect = 0;
        capturingReflections = false;
    }
    void menu(const MenuOverlay& overlay, Size drawable) const {
        if (!overlay.visible) {
            return;
        }
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        const auto& program = programs.menu;
        program.shader.bind();
        const float fit = std::min(static_cast<float>(drawable.width) / renderSize.width,
                                   static_cast<float>(drawable.height) / renderSize.height);
        matrix(program.mvp,
               scale({renderSize.width * fit / drawable.width * 2, renderSize.height * fit / drawable.height * 2, 1}));
        glUniform1f(program.time, overlay.time);
        glUniform1i(program.selection, overlay.selection);
        quad.draw();
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }
    template <class Program>
    void present(Size drawable, const Program& p, unsigned int source, float lensStrength) const {
        glBindSampler(0, 0);
        RenderTarget::bindDefault(drawable);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        const float fit = std::min(static_cast<float>(drawable.width) / renderSize.width,
                                   static_cast<float>(drawable.height) / renderSize.height);
        p.shader.bind();
        glUniform2f(p.scale, renderSize.width * fit / drawable.width * 2,
                    renderSize.height * fit / drawable.height * 2);
        glUniform1f(p.lensStrength, lensStrength);
        glUniform1f(p.lensAspect, static_cast<float>(renderSize.width) / renderSize.height);
        texture(p.texture, source);
        quad.draw();
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
    }
    void render(const Scene& scene, Size drawable) {
        if (drawable.width <= 0 || drawable.height <= 0) {
            return;
        }
        const float lensStrength =
            !std::isfinite(scene.camera.fisheyeStrength) ? 0.0f : std::clamp(scene.camera.fisheyeStrength, 0.0f, 0.25f);
        const bool cameraPass = mode == RenderMode::Normal && lensStrength > 0;
        if (cameraPass && (!cameraTarget || cameraTarget->size().width != drawable.width ||
                           cameraTarget->size().height != drawable.height)) {
            cameraTarget = std::make_unique<RenderTarget>(drawable, RenderTarget::Kind::ColorDepth);
        }
        renderSize = psx() ? settings.retroSize : drawable;
        viewProjection = scene.camera.viewProjection(static_cast<float>(renderSize.width) / renderSize.height);
        lightProjection = scene.light.viewProjection();

        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_FRAMEBUFFER_SRGB);
        glDisable(GL_CLIP_DISTANCE0);
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        if (mode != RenderMode::Normal) {
            // Console dithering is deterministic GLSL, independent of the driver.
            glDisable(GL_DITHER);
        } else {
            glEnable(GL_DITHER);
        }
        glEnable(GL_MULTISAMPLE);
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        glDisable(GL_SAMPLE_COVERAGE);
        glDisable(GL_SAMPLE_MASK);
        glDepthFunc(GL_LESS);
        glDepthRange(0, 1);
        glClearDepth(1);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glBlendEquation(GL_FUNC_ADD);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glBindSampler(0, 0);
        glBindSampler(1, 0);
        glBindSampler(2, 0);
        glBindSampler(3, 0);
        glBindSampler(4, 0);
        glBindSampler(5, 0);
        glBindSampler(6, 0);
        glBindSampler(7, 0);
        glBindSampler(8, 0);
        for (GLuint unit = 9; unit <= 11; ++unit) {
            glBindSampler(unit, 0);
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        shadow.bind();
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.5f, 2.0f);
        glClear(GL_DEPTH_BUFFER_BIT);
        for (const auto& command : scene.objects) {
            std::visit([this](const auto& item) { cast(item); }, command);
        }
        glDisable(GL_POLYGON_OFFSET_FILL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        captureReflections(scene);
        capturePlanarReflections(scene);

        if (psx()) {
            retro.bind();
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        } else if (cameraPass) {
            cameraTarget->bind();
        } else {
            RenderTarget::bindDefault(drawable);
        }
        clearSkyColor(scene);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        background(scene);
        for (const auto& command : scene.objects) {
            std::visit([&](const auto& item) { draw(item, scene); }, command);
        }
        drawParticles(scene, scene);
        glDepthMask(GL_FALSE);
        // Additive world effects preserve the visible objects' dither mask.
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
        for (const auto& command : scene.objects) {
            if (const auto* sprite = std::get_if<Sprite>(&command)) {
                drawRipple(*sprite, scene);
            }
        }
        for (const auto& command : scene.effects) {
            std::visit([&](const auto& effect) { draw(effect, scene); }, command);
        }
        glDepthMask(GL_TRUE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (psx()) {
            programs.retro.shader.bind();
            glUniform1f(programs.retro.ditherStrength, settings.retroDitherStrength);
            present(drawable, programs.retro, retro.texture(), lensStrength);
        } else if (cameraPass) {
            present(drawable, programs.present, cameraTarget->texture(), lensStrength);
        }
        // Composite UI over the completed game image, after retro/lens effects.
        menu(scene.menu, drawable);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindVertexArray(0);
        glBindSampler(0, 0);
        glBindSampler(2, 0);
        glBindSampler(3, 0);
        glBindSampler(4, 0);
        glBindSampler(5, 0);
        glBindSampler(6, 0);
        glBindSampler(7, 0);
        glBindSampler(8, 0);
        for (GLuint unit = 9; unit <= 11; ++unit) {
            glBindSampler(unit, 0);
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        glActiveTexture(GL_TEXTURE0);
        glUseProgram(0);
    }
};
SceneRenderer::SceneRenderer(RenderSettings settings) : impl_(std::make_unique<Impl>(checkedSettings(settings))) {}
SceneRenderer::~SceneRenderer() = default;
void SceneRenderer::render(const Scene& scene, Size drawable) {
    impl_->render(scene, drawable);
}
void SceneRenderer::setMode(RenderMode mode) {
    impl_->mode = mode;
}
RenderMode SceneRenderer::mode() const {
    return impl_->mode;
}
void SceneRenderer::setRetroMode(bool enabled) {
    setMode(enabled ? RenderMode::Psx : RenderMode::Normal);
}
bool SceneRenderer::retroMode() const {
    return mode() == RenderMode::Psx;
}
void SceneRenderer::setPsxDitheredShadows(bool enabled) {
    impl_->settings.psxDitheredShadows = enabled;
}
bool SceneRenderer::psxDitheredShadows() const {
    return impl_->settings.psxDitheredShadows;
}
void SceneRenderer::setPsxDistanceFog(bool enabled) {
    impl_->settings.psxDistanceFog = enabled;
}
bool SceneRenderer::psxDistanceFog() const {
    return impl_->settings.psxDistanceFog;
}
} // namespace engine::graphics
