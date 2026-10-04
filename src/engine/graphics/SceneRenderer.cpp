#include "engine/graphics/SceneRenderer.h"
#include "Programs.h"
#include "engine/graphics/Mesh.h"
#include "engine/graphics/Model.h"
#include "engine/graphics/RenderTarget.h"
#include "engine/graphics/SpriteOutline.h"
#include "engine/graphics/TextureSampler.h"
#include <algorithm>
#include <cmath>
#include <glad/glad.h>
#include <stdexcept>
#include <vector>

namespace engine::graphics {
using namespace math;
namespace {
RenderSettings checkedSettings(RenderSettings settings) {
    if (!std::isfinite(settings.retroPixelStep) || settings.retroPixelStep < 0 ||
        !std::isfinite(settings.modelAffineStrength) || settings.modelAffineStrength < 0 ||
        settings.modelAffineStrength > 1) {
        throw std::invalid_argument("Retro pixel step must be nonnegative and affine strength must be in [0, 1]");
    }
    if (!std::isfinite(settings.retroSnapStrength) || settings.retroSnapStrength < 0 || settings.retroSnapStrength > 1 ||
        !std::isfinite(settings.retroAffineStrength) || settings.retroAffineStrength < 0 || settings.retroAffineStrength > 1) {
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
    TextureSampler retroClamp;
    TextureSampler retroRepeat{true};
    TextureSampler materialClamp{false, true};
    TextureSampler materialRepeat{true, true};
    RenderMode mode = RenderMode::Normal;
    Mat4 viewProjection;
    Mat4 lightProjection;
    Size renderSize;

    explicit Impl(RenderSettings config)
        : settings(config), retro(config.retroSize, RenderTarget::Kind::ColorDepth),
          shadow({config.shadowMapSize, config.shadowMapSize}, RenderTarget::Kind::Shadow) {}

    bool psx() const { return mode == RenderMode::Psx; }
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
        glUniform2f(program.fogRange, distanceFog() ? settings.psxFogStart : 0,
                    distanceFog() ? settings.psxFogEnd : 0);
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
        glUniform1f(u.occlusionStrength, std::isfinite(maps.occlusionStrength)
                                           ? std::clamp(maps.occlusionStrength, 0.0f, 1.0f) : 0);
    }
    template <class Program> void materialEnvironment(const Program& p, const Scene& scene) const {
        const auto sun = normalize(scene.light.direction);
        glUniform3f(p.sunDir, sun.x, sun.y, sun.z);
        glUniform3f(p.cameraPosition, scene.camera.position.x, scene.camera.position.y, scene.camera.position.z);
        const float night = std::floor(std::clamp(scene.sky.nightBlend, 0.0f, 1.0f) * 4.0f + 0.5f) / 4.0f;
        color(p.skyColor, {scene.sky.top.r * (1 - night) + 0.045f * night,
                           scene.sky.top.g * (1 - night) + 0.055f * night,
                           scene.sky.top.b * (1 - night) + 0.14f * night});
        color(p.hazeColor, {scene.sky.bottom.r * (1 - night) + 0.38f * night,
                            scene.sky.bottom.g * (1 - night) + 0.31f * night,
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
        glUniform1f(p.haze, 0);
        glUniform1f(p.time, scene.time);
        glUniform1f(p.heat, 0);
    }
    float mipLevel(const Quad& command, const Camera& camera) const {
        const Vec3 offset =
            Vec3{command.model.values[12], command.model.values[13], command.model.values[14]} - camera.position;
        const float distance = std::sqrt(dot(offset, offset));
        const float viewHalfHeight = std::max(distance, 0.1f) * std::tan(radians(camera.verticalFov) * 0.5f);
        const auto size = command.texture->size();
        const float worldPerTexel = std::max(command.worldSize.x / (size.width * command.textureScale.x),
                                             command.worldSize.y / (size.height * command.textureScale.y));
        const float pixelsPerTexel = worldPerTexel * renderSize.height / (viewHalfHeight * 2.0f);
        return std::clamp(std::ceil(std::log2(1.0f / pixelsPerTexel)), 0.0f, 8.0f);
    }
    void shadowDraw(const Mat4& model, unsigned int textureId = 0, float opacity = 1) const {
        const auto& p = programs.shadow;
        p.shader.bind();
        matrix(p.mvp, multiply(lightProjection, model));
        glUniform1i(p.textured, textureId != 0);
        glUniform1f(p.opacity, opacity);
        texture(p.texture, textureId);
    }
    void cast(const Quad& command) const {
        if (!command.castsShadow) {
            return;
        }
        shadowDraw(command.model, 0, command.opacity);
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
        shadowDraw(command.transform);
        for (const auto& primitive : command.model->primitives()) {
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
                glUniform1f(p.lod, mode != RenderMode::Normal ? 0 : mipLevel(command, scene.camera));
            }
            glUniform2f(p.texScale, command.textureScale.x, command.textureScale.y);
            glUniform2f(p.texOffset, command.textureOffset.x, command.textureOffset.y);
            glUniform1i(p.receivesShadow, command.receivesShadow);
            glUniform1f(p.alpha, command.opacity);
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
                glUniform1i(p.receivesShadow, command.receivesShadow);
                glUniform2f(p.texScale, command.textureScale.x, command.textureScale.y);
                glUniform2f(p.texOffset, command.textureOffset.x, command.textureOffset.y);
                glUniform1f(p.lod, mode != RenderMode::Normal ? 0 : mipLevel(command, scene.camera));
                glUniform1f(p.ambientLight, ambient);
                glUniform1f(p.alpha, command.opacity);
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
                glUniform1f(p.lod, mode != RenderMode::Normal ? 0 : mipLevel(command, scene.camera));
                glUniform1f(p.light, light);
                glUniform1f(p.ambientLight, ambient);
                glUniform1f(p.alpha, command.opacity);
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
        (command.profile ? *command.profile : quad).draw();
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
        quad.draw();
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
        color(sky.topColor, scene.sky.top);
        color(sky.bottomColor, scene.sky.bottom);
        glUniform1f(sky.time, scene.time);
        glUniform1f(sky.nightBlend, scene.sky.nightBlend);
        const Vec3 sunDirection = normalize(scene.light.direction);
        glUniform3f(sky.sunDir, sunDirection.x, sunDirection.y, sunDirection.z);
        glUniform1f(sky.aspect, static_cast<float>(renderSize.width) / renderSize.height);
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
        glDepthMask(GL_TRUE);
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
        matrix(program.mvp, scale({renderSize.width * fit / drawable.width * 2,
                                   renderSize.height * fit / drawable.height * 2, 1}));
        glUniform1f(program.time, overlay.time);
        glUniform1i(program.selection, overlay.selection);
        quad.draw();
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }
    template <class Program> void present(Size drawable, const Program& p, unsigned int source,
                                         float lensStrength) const {
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
        const float lensStrength = !std::isfinite(scene.camera.fisheyeStrength)
                                       ? 0.0f
                                       : std::clamp(scene.camera.fisheyeStrength, 0.0f, 0.25f);
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

        if (psx()) {
            retro.bind();
        } else if (cameraPass) {
            cameraTarget->bind();
        } else {
            RenderTarget::bindDefault(drawable);
        }
        glClearColor(scene.sky.bottom.r, scene.sky.bottom.g, scene.sky.bottom.b, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        background(scene);
        for (const auto& command : scene.objects) {
            std::visit([&](const auto& item) { draw(item, scene); }, command);
        }
        glDepthMask(GL_FALSE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
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
