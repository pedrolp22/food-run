#include "engine/graphics/SceneRenderer.h"
#include "FrameLighting.h"
#include "PlanarReflectionPass.h"
#include "Programs.h"
#include "RenderState.h"
#include "SceneVisibility.h"
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
using detail::ScreenBounds;
namespace {
RenderSettings checkedSettings(RenderSettings settings) {
    settings.validate();
    return settings;
}
void matrix(int location, const Mat4& value) {
    glUniformMatrix4fv(location, 1, GL_FALSE, value.values.data());
}
Vec3 center(const Quad& surface) {
    return {surface.model.values[12], surface.model.values[13], surface.model.values[14]};
}
float smoothTransition(float start, float end, float value) {
    const float t = std::clamp((value - start) / (end - start), 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}
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
    mutable detail::RenderState state;
    RenderSettings settings;
    detail::Programs programs;
    mutable detail::SkinningBuffer skinning;
    Mesh cube = makeCubeMesh();
    Mesh quad = makeQuadMesh();
    Mesh affineQuad2 = subdividedQuad(2);
    Mesh affineQuad4 = subdividedQuad(4);
    RenderTarget retro;
    RenderTarget shadow;
    std::unique_ptr<RenderTarget> cameraTarget;
    std::unique_ptr<ReflectionMap> reflectionMap;
    detail::PlanarReflectionPass planarReflections;
    mutable detail::SceneVisibility visibility;
    struct ParticleEntry {
        const Quad* surface;
        float depth;
    };
    mutable std::vector<ParticleEntry> orderedParticles;
    detail::FrameLighting lighting;
    mutable std::vector<unsigned int> viewUniformsReady;
    mutable std::vector<unsigned int> environmentUniformsReady;
    mutable std::array<bool, 2> cloudUniformsReady{};
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
          shadow({config.shadows ? config.shadowMapSize : 1, config.shadows ? config.shadowMapSize : 1},
                 RenderTarget::Kind::Shadow), planarReflections(config) {
        // The supplied panorama need not have matching left/right edges. Its
        // mirrored horizontal wrap joins its edges; the horizon/pole stay clamped.
        // Keep the sampler's nearest-neighbor filtering and use only the source level.
        glSamplerParameteri(panoramaSampler.id(), GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
        glSamplerParameteri(panoramaSampler.id(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glSamplerParameterf(panoramaSampler.id(), GL_TEXTURE_MAX_LOD, 0);
    }

    void color(int location, Color value) const { state.uniform3f(location, value.r, value.g, value.b); }
    void texture(int location, unsigned int id, int unit = 0) const {
        state.activeTexture(GL_TEXTURE0 + unit);
        state.bindTexture(GL_TEXTURE_2D, id);
        state.uniform1i(location, unit);
        state.activeTexture(GL_TEXTURE0);
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
        state.uniform1i(p.enabled, enabled);
        // Every view in a frame shares these directional sky settings. Upload
        // them once per program, rather than again for every reflected facade.
        auto& ready = cloudUniformsReady[&p == &programs.sky.clouds ? 0 : 1];
        if (ready) {
            return;
        }
        ready = true;
        const auto palette = scene.sky.palette();
        color(p.topColor, palette.top);
        color(p.horizonColor, palette.horizon);
        color(p.cloudTint, palette.cloudTint);
        const auto finiteRange = [](float value, float low, float high, float fallback = 0) {
            return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
        };
        state.uniform1ui(p.seed, scene.sky.layoutSeed);
        state.uniform1f(p.time, std::isfinite(scene.time) ? scene.time : 0);
        const auto& stars = scene.sky.stars;
        const float starFade = smoothTransition(.6f, .95f, finiteRange(scene.sky.nightBlend, 0, 1));
        state.uniform4f(p.stars, finiteRange(stars.density, 0, 1),
                        radians(finiteRange(stars.sizeDegrees, .01f, .5f, .11f)), finiteRange(stars.brightness, 0, 1),
                        starFade);
        state.uniform2f(p.twinkle, finiteRange(stars.twinkleSpeed, 0, 10), finiteRange(stars.twinkleStrength, 0, 1));
        const auto& moon = scene.sky.moon;
        const bool moonPresent = scene.sky.hasTexturedClouds() && moon.texture && moon.texture->id();
        texture(p.moonTexture, moonPresent ? moon.texture->id() : 0, 12);
        state.bindSampler(12, moonPresent ? cloudSampler.id() : 0);
        const Vec3 moonDirection = normalize(moon.direction);
        state.uniform3f(p.moonDirection, moonDirection.x, moonDirection.y, moonDirection.z);
        const float moonWidth = std::tan(radians(finiteRange(moon.sizeDegrees, .1f, 30, 6.5f)) * .5f);
        state.uniform2f(p.moonSize, moonWidth, moonWidth / (moonPresent ? moon.texture->aspect() : 1));
        const float moonFade = moonPresent ? smoothTransition(.5f, .9f, finiteRange(scene.sky.nightBlend, 0, 1)) : 0;
        state.uniform3f(p.moonLight, moonFade, finiteRange(moon.brightness, 0, 3, 1.35f),
                        finiteRange(moon.glowStrength, 0, 1, .16f));
        for (std::size_t index = 0; index < scene.sky.texturedClouds.size(); ++index) {
            const auto& layer = scene.sky.texturedClouds[index];
            const bool present = layer.texture && layer.texture->id();
            const int unit = 9 + static_cast<int>(index);
            texture(p.textures[index], present ? layer.texture->id() : 0, unit);
            state.bindSampler(unit, present ? cloudSampler.id() : 0);
            const float width = std::isfinite(layer.widthDegrees) ? std::clamp(layer.widthDegrees, .1f, 180.0f) : 26;
            const float requestedSpacing =
                std::isfinite(layer.repeatDegrees) ? std::clamp(layer.repeatDegrees, width, 360.0f) : 90;
            // Whole slots close the dome even when a caller supplies a non-divisor spacing.
            const float spacing = 360 / std::max(1.0f, std::floor(360 / requestedSpacing));
            const float elevation =
                std::isfinite(layer.elevationDegrees) ? std::clamp(layer.elevationDegrees, 0.0f, 90.0f) : 7;
            const float aspect = present ? layer.texture->aspect() : 1;
            state.uniform4f(p.layouts[index], width / 360, spacing / 360, radians(elevation), radians(width / aspect));
            const double offset = static_cast<double>(scene.time) * layer.scrollSpeed + layer.offset;
            // Each slot now has its own identity; wrap a full turn rather than repeating one slot's artwork.
            const float phase = std::isfinite(offset) ? static_cast<float>(std::fmod(offset, 1.0)) : 0;
            const float opacity = present && std::isfinite(layer.opacity) ? std::clamp(layer.opacity, 0.0f, 1.0f) : 0;
            state.uniform2f(p.settings[index], phase, opacity);
            state.uniform4f(p.variation[index], finiteRange(layer.horizontalScatter, 0, .45f),
                            radians(finiteRange(layer.elevationScatterDegrees, 0, 30)),
                            finiteRange(layer.sizeVariation, 0, .5f), finiteRange(layer.curvature, 0, .4f));
            state.uniform2f(p.warp[index], finiteRange(layer.warpStrength, 0, .12f),
                            finiteRange(layer.warpSpeed, 0, 5));
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
        state.uniform3f(program.snap, renderSize.width, renderSize.height, psx() ? settings.retroPixelStep : 0);
        state.uniform1f(program.snapStrength, psx() ? settings.retroSnapStrength : 0);
        state.uniform2f(program.fogRange, distanceFog() ? settings.psxFogStart : 0,
                        distanceFog() ? settings.psxFogEnd : 0);
        if constexpr (requires { program.reflectionClipPlane; }) {
            state.uniform4f(program.reflectionClipPlane, reflectionClipPlane[0], reflectionClipPlane[1],
                            reflectionClipPlane[2], reflectionClipPlane[3]);
        }
    }
    float affineStrength(float commandStrength) const {
        return psx() ? std::clamp(commandStrength, 0.0f, 1.0f) * settings.retroAffineStrength : 0;
    }
    template <class Program> void surfaceTexture(const Program& program, const Texture& asset) const {
        texture(program.texture, asset.id());
        state.bindSampler(0, psx() ? (asset.repeats() ? retroRepeat.id() : retroClamp.id()) : 0);
    }
    template <class Program> void psxMinification(const Program& program, const Quad& command) const {
        const float bias = command.psxMinificationLodBias.value_or(0);
        state.uniform1i(program.textureRepeat, command.texture->repeats());
        state.uniform1i(program.maxTextureLod, command.texture->maxMipLevel());
        state.uniform1i(program.psxMinification, psx() && command.psxMinificationLodBias && std::isfinite(bias));
        state.uniform1f(program.psxLodBias, std::isfinite(bias) ? std::clamp(bias, -2.0f, 2.0f) : 0);
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
        state.bindProgram(program.shader.id());
        matrix(program.mvp, multiply(viewProjection, model));
        matrix(program.model, model);
        if (std::find(viewUniformsReady.begin(), viewUniformsReady.end(), program.shader.id()) !=
            viewUniformsReady.end()) {
            return;
        }
        viewUniformsReady.push_back(program.shader.id());
        matrix(program.lightViewProj, lightProjection);
        texture(program.shadowMap, shadow.texture(), 1);
        state.uniform1f(program.shadowDitherStrength, psx() ? settings.retroDitherStrength : 0);
        state.uniform1i(program.ditheredShadows, psx() && settings.psxDitheredShadows);
        state.uniform1f(program.ditheredShadowDarkness, settings.psxDitheredShadowDarkness);
        color(program.fogColor, fogColor(scene));
        snap(program);
        if constexpr (requires { program.pointLights; }) {
            state.uniform1i(program.pointLights.count, lighting.pointCount);
            if (lighting.pointCount > 0) {
                glUniform4fv(program.pointLights.positions, lighting.pointCount, lighting.pointPositions.data());
                glUniform3fv(program.pointLights.radiance, lighting.pointCount, lighting.pointRadiance.data());
            }
        }
    }
    template <class Program> void sunlight(const Program& program, const DirectionalLight& light) const {
        const Vec3 sun = normalize(light.direction);
        state.uniform3f(program.sunDir, sun.x, sun.y, sun.z);
        state.uniform1f(program.ambient, light.ambient);
    }
    template <class Program> void materialFinish(const Program& p, const SurfaceMaterial& material) const {
        const auto factor = [](float value, float fallback) {
            return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
        };
        const SurfaceMaterial defaults;
        state.uniform1f(p.surfaceRoughness, factor(material.roughness, defaults.roughness));
        state.uniform1f(p.surfaceMetallic, factor(material.metallic, defaults.metallic));
        state.uniform1f(p.surfaceReflectivity, factor(material.reflectivity, defaults.reflectivity));
        state.uniform1f(p.surfaceReflectance, factor(material.reflectance, defaults.reflectance));
    }
    template <class Program> void materialMaps(const Program& p, const MaterialTextures& maps) const {
        const auto& u = p.maps;
        const auto bind = [&](int location, int present, int maxLod, const Texture* image, int unit) {
            const bool enabled = image && image->id() && (settings.materialDetail || unit == 2 || unit == 5);
            texture(location, enabled ? image->id() : 0, unit);
            state.bindSampler(unit,
                              enabled && psx() ? (image->repeats() ? materialRepeat.id() : materialClamp.id()) : 0);
            state.uniform1i(present, enabled);
            state.uniform1i(maxLod, enabled ? image->maxMipLevel() : 0);
        };
        bind(u.properties, u.hasProperties, u.maxPropertiesLod, maps.properties, 2);
        bind(u.normal, u.hasNormal, u.maxNormalLod, maps.normal, 3);
        bind(u.occlusion, u.hasOcclusion, u.maxOcclusionLod, maps.occlusion, 4);
        if constexpr (requires { p.glassMap; }) {
            bind(p.glassMap, p.hasGlassMap, p.maxGlassLod, maps.glass, 5);
        }
        state.uniform1i(u.packedReflectivity, maps.packedReflectivity);
        state.uniform1i(u.retro, psx());
        state.uniform1f(u.normalScale, std::isfinite(maps.normalScale) ? maps.normalScale : 0);
        state.uniform1f(u.occlusionStrength,
                        std::isfinite(maps.occlusionStrength) ? std::clamp(maps.occlusionStrength, 0.0f, 1.0f) : 0);
    }
    template <class Program> void materialEnvironment(const Program& p, const Scene& scene) const {
        if (std::find(environmentUniformsReady.begin(), environmentUniformsReady.end(), p.shader.id()) !=
            environmentUniformsReady.end()) {
            return;
        }
        environmentUniformsReady.push_back(p.shader.id());
        const auto sun = lighting.sunDirection;
        state.uniform3f(p.sunDir, sun.x, sun.y, sun.z);
        state.uniform3f(p.cameraPosition, scene.camera.position.x, scene.camera.position.y, scene.camera.position.z);
        color(p.skyColor, lighting.skyRadiance);
        color(p.horizonRadiance, lighting.horizonRadiance);
        color(p.hazeColor, lighting.hazeColor);
        color(p.sunColor, lighting.sunRadiance);
    }
    void beginMaterial(const Mat4& model, const Scene& scene, const SurfaceMaterial& finish, bool quadSurface = false,
                       bool modelSurface = false) const {
        const auto& p = programs.material;
        lit(p, model, scene);
        materialEnvironment(p, scene);
        materialFinish(p, finish);
        state.uniform1f(p.ambient, scene.light.ambient);
        state.uniform1i(p.vertexLighting, mode != RenderMode::Normal);
        state.uniform1f(p.time, scene.time);
        state.uniform1i(p.quad, quadSurface);
        state.uniform1i(p.skin.enabled, false);
        state.uniform1i(p.colorIsLinear, modelSurface);
        if (!quadSurface) {
            state.uniform1i(p.psxMinification, false);
            state.uniform2f(p.texScale, 1, 1);
            state.uniform2f(p.texOffset, 0, 0);
            state.uniform1f(p.lod, -1);
            state.uniform1i(p.receivesShadow, true);
            state.uniform1f(p.alpha, 1);
            state.uniform1f(p.haze, 0);
            state.uniform1f(p.heat, 0);
        }
        if (!modelSurface) {
            state.uniform1f(p.baseAlpha, 1);
            state.uniform3f(p.emissiveFactor, 0, 0, 0);
            state.uniform1i(p.hasEmissiveTexture, false);
            if (!quadSurface) {
                state.uniform1i(p.hasTexture, false);
                state.uniform1f(p.affine, 0);
                state.uniform1f(p.alphaCutoff, 0);
                state.uniform1i(p.doubleSided, false);
            }
        }
    }
    void shadowDraw(const Mat4& model, unsigned int textureId = 0, float opacity = 1, float alphaCutoff = .1f,
                    bool modelMesh = false, Vec2 texScale = {1, 1}, Vec2 texOffset = {}, float baseAlpha = 1) const {
        const auto& p = programs.shadow;
        state.bindProgram(p.shader.id());
        matrix(p.mvp, multiply(lightProjection, model));
        state.uniform1i(p.skin.enabled, false);
        state.uniform1i(p.textured, textureId != 0);
        state.uniform1f(p.opacity, opacity);
        state.uniform1f(p.alphaCutoff, alphaCutoff);
        state.uniform1f(p.baseAlpha, baseAlpha);
        state.uniform1i(p.modelMesh, modelMesh);
        state.uniform2f(p.texScale, texScale.x, texScale.y);
        state.uniform2f(p.texOffset, texOffset.x, texOffset.y);
        texture(p.texture, textureId);
        state.bindSampler(0, 0);
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
    void usePose(const detail::SkinningUniforms& uniforms, const assets::ModelPose* pose, std::size_t primitive) const {
        const bool skinned = pose && !pose->jointMatrices(primitive).empty();
        state.uniform1i(uniforms.enabled, skinned);
        if (skinned)
            skinning.bind(*pose, primitive);
    }
    static Mat4 posedTransform(const ModelInstance& command, const assets::ModelPose* pose, std::size_t primitive) {
        return pose && pose->jointMatrices(primitive).empty()
                   ? multiply(command.transform, pose->primitiveTransform(primitive))
                   : command.transform;
    }
    void cast(const ModelInstance& command) const {
        if (!command.model) {
            return;
        }
        const auto* pose = command.pose ? command.pose : command.model->bindPose();
        const auto& primitives = command.model->primitives();
        for (std::size_t i = 0; i < primitives.size(); ++i) {
            const auto& primitive = primitives[i];
            const auto& material = primitive.material;
            shadowDraw(posedTransform(command, pose, i), material.alphaCutoff > 0 ? material.texture.id() : 0, 1,
                       material.alphaCutoff, true, {1, 1}, {}, material.baseColor[3]);
            usePose(programs.shadow.skin, pose, i);
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
            beginMaterial(command.model, scene, command.surfaceMaterial, true);
            materialMaps(p, command.materialTextures);
            state.uniform3f(p.faceNormal, normal.x, normal.y, normal.z);
            color(p.color, command.color);
            state.uniform1i(p.hasTexture, command.texture != nullptr);
            state.uniform1i(p.psxMinification, false);
            state.uniform1f(p.lod, mode != RenderMode::Normal && command.texture ? 0 : -1);
            if (command.texture) {
                surfaceTexture(p, *command.texture);
                psxMinification(p, command);
                // Use the pixel's UV footprint in Normal mode; a single rounded
                // LOD for an entire curved segment loses near asphalt detail.
            }
            state.uniform2f(p.texScale, command.textureScale.x, command.textureScale.y);
            state.uniform2f(p.texOffset, command.textureOffset.x, command.textureOffset.y);
            state.uniform1i(p.receivesShadow, command.receivesShadow);
            state.uniform1f(p.alpha, command.opacity);
            state.uniform1f(p.alphaCutoff, command.alphaCutoff);
            state.uniform1i(p.doubleSided, command.doubleSided);
            state.uniform1f(p.haze, std::clamp(command.haze, 0.0f, 1.0f));
            state.uniform1f(p.affine, affineStrength(command.affineStrength));
            state.uniform1f(p.heat, mode == RenderMode::Normal ? command.heat : 0);
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
                state.uniform3f(p.faceNormal, normal.x, normal.y, normal.z);
                materialEnvironment(p, scene);
                state.uniform1i(p.hasWindows, command.skyscraperWindows);
                state.uniform1f(p.haze, std::clamp(command.haze, 0.0f, 1.0f));
                state.uniform1f(p.windowRoughness, std::clamp(command.windowMaterial.roughness, 0.0f, 1.0f));
                state.uniform1f(p.windowReflectivity, std::clamp(command.windowMaterial.reflectivity, 0.0f, 1.0f));
                materialFinish(p, command.surfaceMaterial);
                materialMaps(p, command.materialTextures);
                // Unit 6 is distinct from albedo, shadow and linear material data.
                const bool worldReflection =
                    reflectionsReady && !capturingReflections && command.windowMaterial.worldReflections;
                state.activeTexture(GL_TEXTURE6);
                state.bindTexture(GL_TEXTURE_CUBE_MAP, worldReflection ? reflectionMap->texture() : 0);
                state.bindSampler(6, 0);
                state.uniform1i(p.worldReflection, 6);
                state.uniform1i(p.hasWorldReflection, worldReflection);
                state.uniform1f(p.worldReflectionMaxLod, worldReflection ? reflectionMap->maxMipLevel() : 0);
                state.activeTexture(GL_TEXTURE0);
                const auto planar = capturingReflections ? std::optional<detail::PlanarReflectionBinding>{}
                                                         : planarReflections.binding(command, scene.camera.position);
                texture(p.planarReflection, planar ? planar->texture : 0, 7);
                state.bindSampler(7, 0);
                state.uniform1i(p.hasPlanarReflection, planar.has_value());
                state.uniform1f(p.planarReflectionStrength, planar ? planar->strength : 0);
                matrix(p.planarReflectionViewProjection, planar ? planar->viewProjection : identity());
                state.uniform2f(p.planarReflectionTextureScale, planar ? planar->textureScale.x : 1,
                                planar ? planar->textureScale.y : 1);
                state.uniform1f(p.planarReflectionMaxLod, planar ? planar->maxLod : 0);
                state.uniform1f(p.windowReflectance, std::isfinite(command.windowMaterial.reflectance)
                                                         ? std::clamp(command.windowMaterial.reflectance, 0.0f, 1.0f)
                                                         : 0);
                state.uniform1f(p.windowBackingTint, std::isfinite(command.windowMaterial.backingTint)
                                                         ? std::clamp(command.windowMaterial.backingTint, 0.0f, 1.0f)
                                                         : 0);
                const auto* panorama = scene.sky.panorama;
                const bool skyReflection = !capturingReflections && settings.worldReflectionMapSize &&
                                           command.windowMaterial.worldReflections && panorama && panorama->id();
                texture(p.panorama, skyReflection ? panorama->id() : 0, 8);
                state.bindSampler(8, skyReflection ? panoramaSampler.id() : 0);
                state.uniform1f(p.panoramaOffset, panoramaOffset(scene));
                state.uniform1i(p.hasSkyPanorama, skyReflection);
                state.uniform1f(p.skyPanoramaNight,
                                std::floor(std::clamp(scene.sky.nightBlend, 0.0f, 1.0f) * 4 + .5f) / 4);
                cloudSky(p.clouds, scene,
                         !capturingReflections && settings.worldReflectionMapSize &&
                             command.windowMaterial.worldReflections);
                state.uniform1i(p.receivesShadow, command.receivesShadow);
                state.uniform2f(p.texScale, command.textureScale.x, command.textureScale.y);
                state.uniform2f(p.texOffset, command.textureOffset.x, command.textureOffset.y);
                state.uniform2f(p.facadeVerticalRange, command.facadeVerticalRange.x, command.facadeVerticalRange.y);
                state.uniform1f(p.lod, mode != RenderMode::Normal ? 0 : -1);
                state.uniform1f(p.ambientLight, ambient);
                state.uniform1f(p.alpha, command.opacity);
                state.uniform1f(p.alphaCutoff, command.alphaCutoff);
                state.uniform1f(p.affine, affineStrength(command.affineStrength));
            } else {
                const auto& p = programs.road;
                lit(p, command.model, scene);
                surfaceTexture(p, *command.texture);
                psxMinification(p, command);
                color(p.color, command.color);
                state.uniform1i(p.receivesShadow, command.receivesShadow);
                state.uniform2f(p.texScale, command.textureScale.x, command.textureScale.y);
                state.uniform2f(p.texOffset, command.textureOffset.x, command.textureOffset.y);
                state.uniform1f(p.lod, mode != RenderMode::Normal ? 0 : -1);
                state.uniform1f(p.light, light);
                state.uniform1f(p.ambientLight, ambient);
                state.uniform1f(p.alpha, command.opacity);
                state.uniform1f(p.alphaCutoff, command.alphaCutoff);
                state.uniform1f(p.time, scene.time);
                state.uniform1f(p.heat, mode != RenderMode::Normal ? 0 : command.heat);
                state.uniform1f(p.affine, affineStrength(command.affineStrength));
            }
        } else {
            const auto& p = programs.plain;
            lit(p, command.model, scene);
            color(p.color, command.color);
            state.uniform1f(p.alpha, command.opacity);
            state.uniform1f(p.light, light);
            state.uniform1f(p.ambientLight, ambient);
            state.uniform1i(p.receivesShadow, command.receivesShadow);
        }
        surfaceMesh(command).draw();
    }
    void draw(const Cube& command, const Scene& scene) const {
        if (command.pbrSurface) {
            beginMaterial(command.model, scene, command.surfaceMaterial);
            materialMaps(programs.material, {});
            color(programs.material.color, command.color);
            cube.draw();
            return;
        }
        const auto& p = programs.cube;
        lit(p, command.model, scene);
        sunlight(p, scene.light);
        state.uniform1i(p.vertexLighting, mode != RenderMode::Normal);
        color(p.color, command.color);
        cube.draw();
    }
    void draw(const ModelInstance& command, const Scene& scene) const {
        if (!command.model) {
            return;
        }
        const auto* pose = command.pose ? command.pose : command.model->bindPose();
        const auto& primitives = command.model->primitives();
        if (command.pbrSurface) {
            const auto& p = programs.material;
            beginMaterial(command.transform, scene, SurfaceMaterial{}, false, true);
            state.uniform1f(p.affine, affineStrength(settings.modelAffineStrength));
            for (std::size_t i = 0; i < primitives.size(); ++i) {
                const auto& primitive = primitives[i];
                if (pose)
                    lit(p, posedTransform(command, pose, i), scene);
                usePose(p.skin, pose, i);
                const auto& material = primitive.material;
                materialFinish(p, command.materialOverride.value_or(material.surfaceMaterial));
                materialMaps(p, material.textures());
                const float emission =
                    std::isfinite(command.emissiveScale) ? std::clamp(command.emissiveScale, 0.0f, 64.0f) : 0;
                state.uniform3f(p.emissiveFactor, std::min(material.emissiveFactor[0] * emission, 64.0f),
                                std::min(material.emissiveFactor[1] * emission, 64.0f),
                                std::min(material.emissiveFactor[2] * emission, 64.0f));
                const bool emissiveTexture = emission > 0 && material.emissive.id();
                state.uniform1i(p.hasEmissiveTexture, emissiveTexture);
                texture(p.emissiveTexture, emissiveTexture ? material.emissive.id() : 0, 13);
                state.bindSampler(13, emissiveTexture && psx()
                                          ? (material.emissive.repeats() ? retroRepeat.id() : retroClamp.id())
                                          : 0);
                state.uniform3f(p.color, material.baseColor[0], material.baseColor[1], material.baseColor[2]);
                state.uniform1i(p.hasTexture, material.texture.id() != 0);
                state.uniform1f(p.alphaCutoff, material.alphaCutoff);
                const float opacity = std::isfinite(command.opacity) ? std::clamp(command.opacity, 0.0f, 1.0f) : 0;
                state.uniform1f(p.baseAlpha, material.baseColor[3] * opacity);
                state.uniform1i(p.doubleSided, material.doubleSided);
                surfaceTexture(p, material.texture);
                primitive.mesh.draw();
            }
            return;
        }
        const auto& p = programs.model;
        lit(p, command.transform, scene);
        sunlight(p, scene.light);
        state.uniform1i(p.vertexLighting, mode != RenderMode::Normal);
        state.uniform1f(p.affine, affineStrength(settings.modelAffineStrength));
        for (std::size_t i = 0; i < primitives.size(); ++i) {
            const auto& primitive = primitives[i];
            if (pose)
                lit(p, posedTransform(command, pose, i), scene);
            usePose(p.skin, pose, i);
            const auto& material = primitive.material;
            state.uniform1f(p.hasTexture, material.texture.id() != 0);
            state.uniform1f(p.alphaCutoff, material.alphaCutoff);
            const float opacity = std::isfinite(command.opacity) ? std::clamp(command.opacity, 0.0f, 1.0f) : 0;
            state.uniform1f(p.baseAlpha, material.baseColor[3] * opacity);
            state.uniform1i(p.doubleSided, material.doubleSided);
            state.uniform3f(p.baseColor, material.baseColor[0], material.baseColor[1], material.baseColor[2]);
            surfaceTexture(p, material.texture);
            primitive.mesh.draw();
        }
    }
    void draw(const Sprite& command, const Scene& scene) const {
        if (!command.texture) {
            return;
        }
        const auto& p = programs.sprite;
        state.bindProgram(p.shader.id());
        matrix(p.mvp, multiply(viewProjection, command.model));
        matrix(p.model, command.model);
        state.uniform1i(p.extruded, command.profile != nullptr);
        state.uniform1f(p.outlinePixels, command.outlinePixels);
        const auto lightDirection = normalize(scene.light.direction);
        state.uniform3f(p.sunDir, lightDirection.x, lightDirection.y, lightDirection.z);
        snap(p);
        color(p.fogColor, fogColor(scene));
        state.uniform1f(p.affine, affineStrength(command.affineStrength));
        state.uniform1f(p.time, scene.time);
        state.uniform1f(p.spawnTime, command.spawnTime);
        color(p.rippleColor, command.rippleColor);
        state.uniform1f(p.aspect, command.aspect);
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
        state.bindProgram(p.shader.id());
        matrix(p.mvp, multiply(viewProjection, multiply(model, shape->transform())));
        snap(p);
        // The field stores distances, not color texels: retain linear reconstruction in every mode.
        state.bindSampler(0, 0);
        texture(p.distanceField, shape->distanceField().id());
        color(p.color, tint);
        state.uniform1f(p.radius, shape->radius());
        state.uniform1f(p.phase, phase);
        state.uniform1f(p.width, width);
        state.uniform1f(p.opacity, std::clamp(opacity, 0.0f, 1.0f));
        state.uniform1i(p.burst, burstProgress >= 0);
        state.uniform1f(p.burstProgress, std::max(burstProgress, 0.0f));
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
        state.bindProgram(sky.shader.id());
        cloudSky(sky.clouds, scene);
        const bool panorama = scene.sky.panorama && scene.sky.panorama->id();
        texture(sky.panorama, panorama ? scene.sky.panorama->id() : 0);
        state.bindSampler(0, panorama ? panoramaSampler.id() : 0);
        state.uniform1f(sky.panoramaOffset, panoramaOffset(scene));
        state.uniform1i(sky.hasPanorama, panorama);
        color(sky.topColor, scene.sky.top);
        color(sky.bottomColor, scene.sky.bottom);
        state.uniform1f(sky.time, scene.time);
        state.uniform1f(sky.nightBlend, scene.sky.nightBlend);
        const Vec3 sunDirection = normalize(scene.light.direction);
        state.uniform3f(sky.sunDir, sunDirection.x, sunDirection.y, sunDirection.z);
        state.uniform1f(sky.aspect, skyProjectionAspect > 0 ? skyProjectionAspect
                                                            : static_cast<float>(renderSize.width) / renderSize.height);
        state.uniform4f(sky.viewBounds, skyViewBounds.left, skyViewBounds.bottom, skyViewBounds.right,
                        skyViewBounds.top);
        state.uniform2f(sky.rasterSize, static_cast<float>(renderSize.width), static_cast<float>(renderSize.height));
        state.uniform1f(sky.cloudCoverage, std::clamp(scene.sky.cloudCoverage, 0.0f, 1.0f));
        const Vec3 forward = normalize(scene.camera.target - scene.camera.position);
        const Vec3 right = normalize(cross(forward, scene.camera.up));
        const Vec3 up = cross(right, forward);
        state.uniform3f(sky.cameraForward, forward.x, forward.y, forward.z);
        state.uniform3f(sky.cameraRight, right.x, right.y, right.z);
        state.uniform3f(sky.cameraUp, up.x, up.y, up.z);
        state.uniform3f(sky.cameraPosition, scene.camera.position.x, scene.camera.position.y, scene.camera.position.z);
        state.uniform1f(sky.tanHalfFov, std::tan(radians(scene.camera.verticalFov) * .5f));
        color(sky.sunColor, scene.sky.sunColor);
        // Angular discs keep their shape at every bearing and can sit behind
        // the cloud layers instead of being added over the completed sky.
        const float radius = std::max(scene.sky.sunSize, 0.0f) * .5f / std::max(scene.sky.sunDistance, .001f);
        state.uniform2f(sky.sunRadii, radius * std::clamp(.38f + scene.sky.sunCore * .015f, .38f, .54f),
                        radius * std::clamp(.60f + scene.sky.sunHaze * .05f, .62f, .76f));
        quad.draw();
        state.bindSampler(0, 0);
        glDepthMask(GL_TRUE);
    }
    void drawReflectionWorld(const Scene& world, const Scene& camera) const {
        for (std::size_t i = 0; i < world.objects.size(); ++i) {
            if (visibility.objectVisible(i)) {
                std::visit([&](const auto& item) { draw(item, camera); }, world.objects[i]);
            }
        }
    }
    void drawParticles(const Scene& world, const Scene& camera) const {
        if (!settings.foliageParticles || world.particles.empty()) {
            return;
        }
        orderedParticles.clear();
        orderedParticles.reserve(world.particles.size());
        const auto forward = normalize(camera.camera.target - camera.camera.position);
        for (const auto& particle : world.particles) {
            if (particle.opacity > .001f && visibility.surfaceVisible(particle)) {
                orderedParticles.push_back({&particle, dot(center(particle) - camera.camera.position, forward)});
            }
        }
        // Preserve source order for equal depths without allocating a stable-sort buffer for every reflection.
        std::sort(orderedParticles.begin(), orderedParticles.end(), [](const auto& a, const auto& b) {
            return a.depth == b.depth ? a.surface < b.surface : a.depth > b.depth;
        });
        glDepthMask(GL_FALSE);
        // Texture particles use ordinary alpha blending. Their sprite pixels reduce PSX dither coverage as they fade.
        if (psx()) {
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
        }
        for (const auto& particle : orderedParticles) {
            draw(*particle.surface, camera);
        }
        glDepthMask(GL_TRUE);
        if (psx()) {
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }
    }
    void renderReflectionWorld(const Scene& world, const Scene& camera) const {
        viewUniformsReady.clear();
        environmentUniformsReady.clear();
        visibility.setView(viewProjection, reflectionClipPlane);
        if (world.sky.hasTexturedClouds() || (world.sky.panorama && world.sky.panorama->id())) {
            // The authored sky is cheap enough to draw directly. Avoid drawing
            // every opaque surface twice just to reject its background pixels.
            background(camera);
            glEnable(GL_CLIP_DISTANCE0);
            drawReflectionWorld(world, camera);
            drawParticles(world, camera);
            glDisable(GL_CLIP_DISTANCE0);
            return;
        }
        // Only opaque quads/cubes contribute to this depth prepass. Sprites and
        // imported props keep their normal alpha behavior over the captured sky.
        const auto& depth = programs.plain;
        state.bindProgram(depth.shader.id());
        snap(depth);
        state.uniform1i(depth.receivesShadow, false);
        glEnable(GL_CLIP_DISTANCE0);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        for (std::size_t i = 0; i < world.objects.size(); ++i) {
            if (!visibility.objectVisible(i)) {
                continue;
            }
            std::visit(
                [&](const auto& item) {
                    using T = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<T, Quad> || std::is_same_v<T, Cube>) {
                        if constexpr (std::is_same_v<T, Quad>) {
                            if (item.opacity < 1 || item.alphaCutoff > 0) {
                                return;
                            }
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
                world.objects[i]);
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
        probe.pointLights = scene.pointLights;
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
        state.activeTexture(GL_TEXTURE6);
        state.bindTexture(GL_TEXTURE_CUBE_MAP, 0);
        state.bindSampler(6, 0);
        state.activeTexture(GL_TEXTURE0);
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
        state.forgetTextureBinding(GL_TEXTURE_CUBE_MAP);
        mode = savedMode;
        renderSize = savedSize;
        viewProjection = savedProjection;
        capturingReflections = false;
        reflectionsReady = true;
    }
    void capturePlanarReflections(const Scene& scene) {
        const detail::ReflectionFrame frame{viewProjection, renderSize, mode};
        state.activeTexture(GL_TEXTURE6);
        state.bindTexture(GL_TEXTURE_CUBE_MAP, 0);
        state.activeTexture(GL_TEXTURE7);
        state.bindTexture(GL_TEXTURE_2D, 0);
        state.bindSampler(7, 0);
        state.activeTexture(GL_TEXTURE0);
        const auto views = planarReflections.update(scene, frame, state);
        if (views.empty()) {
            return;
        }
        mode = RenderMode::Normal;
        capturingReflections = true;
        Scene reflected;
        reflected.light = scene.light;
        reflected.pointLights = scene.pointLights;
        reflected.sky = scene.sky;
        reflected.time = scene.time;
        for (const auto& view : views) {
            reflected.camera = view.camera;
            renderSize = view.size;
            viewProjection = view.viewProjection;
            reflectionClipPlane = view.clipPlane;
            skyViewBounds = view.skyBounds;
            skyProjectionAspect = view.aspect;
            view.target->bind();
            glViewport(0, 0, renderSize.width, renderSize.height);
            clearSkyColor(scene);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderReflectionWorld(scene, reflected);
            view.target->generateMipmaps();
            state.forgetTextureBinding(GL_TEXTURE_2D);
        }
        mode = frame.mode;
        renderSize = frame.size;
        viewProjection = frame.viewProjection;
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
        state.bindProgram(program.shader.id());
        const float fit = std::min(static_cast<float>(drawable.width) / renderSize.width,
                                   static_cast<float>(drawable.height) / renderSize.height);
        matrix(program.mvp,
               scale({renderSize.width * fit / drawable.width * 2, renderSize.height * fit / drawable.height * 2, 1}));
        state.uniform1f(program.time, overlay.time);
        state.uniform1i(program.selection, overlay.selection);
        quad.draw();
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }
    void drawOverlayObjects(const Scene& scene, Size drawable) {
        if (scene.overlayObjects.empty())
            return;
        const Mat4 savedViewProjection = viewProjection;
        const float aspect = static_cast<float>(renderSize.width) / renderSize.height;
        const float fit = std::min(static_cast<float>(drawable.width) / renderSize.width,
                                   static_cast<float>(drawable.height) / renderSize.height);
        const float width = renderSize.width * fit / drawable.width;
        const float height = renderSize.height * fit / drawable.height;
        viewProjection = multiply(scale({width, height, 1}), orthographic(aspect, 1, -1, 1));
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (const auto& item : scene.overlayObjects)
            std::visit([this, &scene](const auto& command) { draw(command, scene); }, item);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        viewProjection = savedViewProjection;
    }
    template <class Program>
    void present(Size drawable, const Program& p, unsigned int source, unsigned int depthTexture,
                 const Camera& camera, float lensStrength) const {
        state.bindSampler(0, 0);
        RenderTarget::bindDefault(drawable);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        const float fit = std::min(static_cast<float>(drawable.width) / renderSize.width,
                                   static_cast<float>(drawable.height) / renderSize.height);
        state.bindProgram(p.shader.id());
        state.uniform2f(p.scale, renderSize.width * fit / drawable.width * 2,
                        renderSize.height * fit / drawable.height * 2);
        state.uniform1f(p.lensStrength, lensStrength);
        state.uniform1f(p.lensAspect, static_cast<float>(renderSize.width) / renderSize.height);
        if constexpr (requires { p.antiAliasing; })
            state.uniform1i(p.antiAliasing, settings.antiAliasing);
        state.uniform1i(p.aoEnabled, settings.screenSpaceAmbientOcclusion && depthTexture != 0);
        state.uniform3f(p.aoCamera, camera.nearPlane, camera.farPlane,
                        std::tan(radians(camera.verticalFov) * 0.5f));
        state.uniform3f(p.aoParameters, settings.ambientOcclusionRadius, settings.ambientOcclusionStrength,
                        settings.ambientOcclusionBias);
        texture(p.texture, source);
        texture(p.aoDepth, depthTexture, 1);
        quad.draw();
        state.bindSampler(1, 0);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
    }
    void render(const Scene& scene, Size drawable, Size retroRaster) {
        if (retroRaster.width == 0 && retroRaster.height == 0)
            retroRaster = settings.retroSize;
        if (retroRaster.width <= 0 || retroRaster.height <= 0)
            throw std::invalid_argument("Retro raster dimensions must be positive");
        state.reset();
        state.activeTexture(GL_TEXTURE0);
        cloudUniformsReady = {};
        if (drawable.width <= 0 || drawable.height <= 0) {
            return;
        }
        const float lensStrength =
            !std::isfinite(scene.camera.fisheyeStrength) ? 0.0f : std::clamp(scene.camera.fisheyeStrength, 0.0f, 0.25f);
        const Size scaled{std::max(1, static_cast<int>(std::lround(drawable.width * settings.renderScale))),
                          std::max(1, static_cast<int>(std::lround(drawable.height * settings.renderScale)))};
        const bool cameraPass = mode == RenderMode::Normal &&
            (lensStrength > 0 || settings.screenSpaceAmbientOcclusion || settings.antiAliasing || settings.renderScale < 1);
        if (cameraPass && (!cameraTarget || cameraTarget->size().width != scaled.width ||
                           cameraTarget->size().height != scaled.height)) {
            cameraTarget = std::make_unique<RenderTarget>(scaled, RenderTarget::Kind::ColorDepth);
        }
        if (psx() && (retro.size().width != retroRaster.width || retro.size().height != retroRaster.height))
            retro = RenderTarget(retroRaster, RenderTarget::Kind::ColorDepthAlpha);
        renderSize = psx() ? retroRaster : scaled;
        viewProjection = scene.camera.viewProjection(static_cast<float>(renderSize.width) / renderSize.height);
        lightProjection = scene.light.viewProjection();
        visibility.prepare(scene);
        lighting.prepare(scene);
        skinning.beginFrame();
        visibility.setView(lightProjection);

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
        state.bindSampler(0, 0);
        state.bindSampler(1, 0);
        state.bindSampler(2, 0);
        state.bindSampler(3, 0);
        state.bindSampler(4, 0);
        state.bindSampler(5, 0);
        state.bindSampler(6, 0);
        state.bindSampler(7, 0);
        state.bindSampler(8, 0);
        for (GLuint unit = 9; unit <= 13; ++unit) {
            state.bindSampler(unit, 0);
            state.activeTexture(GL_TEXTURE0 + unit);
            state.bindTexture(GL_TEXTURE_2D, 0);
        }
        state.activeTexture(GL_TEXTURE8);
        state.bindTexture(GL_TEXTURE_2D, 0);
        state.activeTexture(GL_TEXTURE7);
        state.bindTexture(GL_TEXTURE_2D, 0);
        state.activeTexture(GL_TEXTURE6);
        state.bindTexture(GL_TEXTURE_CUBE_MAP, 0);
        state.activeTexture(GL_TEXTURE1);
        state.bindTexture(GL_TEXTURE_2D, 0);
        state.activeTexture(GL_TEXTURE0);
        shadow.bind();
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.5f, 2.0f);
        glClear(GL_DEPTH_BUFFER_BIT);
        for (std::size_t i = 0; settings.shadows && i < scene.objects.size(); ++i) {
            // Sprite meshes can include authored extrusion beyond their unit quad.
            if (std::holds_alternative<Sprite>(scene.objects[i]) || visibility.objectVisible(i)) {
                std::visit([this](const auto& item) { cast(item); }, scene.objects[i]);
            }
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
        viewUniformsReady.clear();
        environmentUniformsReady.clear();
        // Leave a small margin for console vertex snapping at the viewport edges.
        visibility.setView(viewProjection, reflectionClipPlane,
                           psx() ? 2 * settings.retroPixelStep / std::min(renderSize.width, renderSize.height) : 0);
        background(scene);
        for (std::size_t i = 0; i < scene.objects.size(); ++i) {
            if (std::holds_alternative<Sprite>(scene.objects[i]) || visibility.objectVisible(i)) {
                std::visit([&](const auto& item) { draw(item, scene); }, scene.objects[i]);
            }
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
            state.bindProgram(programs.retro.shader.id());
            state.uniform1f(programs.retro.ditherStrength, settings.retroDitherStrength);
            present(drawable, programs.retro, retro.texture(), retro.depthTexture(), scene.camera, lensStrength);
        } else if (cameraPass) {
            present(drawable, programs.present, cameraTarget->texture(), cameraTarget->depthTexture(), scene.camera,
                    lensStrength);
        }
        // Composite screen-space lettering and UI after retro/lens effects.
        drawOverlayObjects(scene, drawable);
        menu(scene.menu, drawable);
        state.activeTexture(GL_TEXTURE0);
        state.bindTexture(GL_TEXTURE_2D, 0);
        skinning.finishFrame();
        glBindVertexArray(0);
        state.bindSampler(0, 0);
        state.bindSampler(2, 0);
        state.bindSampler(3, 0);
        state.bindSampler(4, 0);
        state.bindSampler(5, 0);
        state.bindSampler(6, 0);
        state.bindSampler(7, 0);
        state.bindSampler(8, 0);
        for (GLuint unit = 9; unit <= 13; ++unit) {
            state.bindSampler(unit, 0);
            state.activeTexture(GL_TEXTURE0 + unit);
            state.bindTexture(GL_TEXTURE_2D, 0);
        }
        state.activeTexture(GL_TEXTURE8);
        state.bindTexture(GL_TEXTURE_2D, 0);
        state.activeTexture(GL_TEXTURE7);
        state.bindTexture(GL_TEXTURE_2D, 0);
        state.activeTexture(GL_TEXTURE6);
        state.bindTexture(GL_TEXTURE_CUBE_MAP, 0);
        state.activeTexture(GL_TEXTURE0);
        glUseProgram(0);
    }
};
SceneRenderer::SceneRenderer(RenderSettings settings) : impl_(std::make_unique<Impl>(checkedSettings(settings))) {}
SceneRenderer::~SceneRenderer() = default;
void SceneRenderer::render(const Scene& scene, Size drawable, Size retroRaster) {
    impl_->render(scene, drawable, retroRaster);
}
void SceneRenderer::setSettings(const RenderSettings& settings) {
    settings.validate();
    const int shadowSize = settings.shadows ? settings.shadowMapSize : 1;
    if (impl_->shadow.size().width != shadowSize)
        impl_->shadow = RenderTarget({shadowSize, shadowSize}, RenderTarget::Kind::Shadow);
    if (impl_->retro.size().width != settings.retroSize.width || impl_->retro.size().height != settings.retroSize.height)
        impl_->retro = RenderTarget(settings.retroSize, RenderTarget::Kind::ColorDepthAlpha);
    if (settings.worldReflectionMapSize != impl_->settings.worldReflectionMapSize)
        impl_->reflectionMap.reset();
    impl_->planarReflections.setSettings(settings);
    impl_->settings = settings;
}
RenderSettings SceneRenderer::settings() const {
    return impl_->settings;
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
