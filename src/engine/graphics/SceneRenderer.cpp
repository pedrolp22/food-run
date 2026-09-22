#include "engine/graphics/SceneRenderer.h"
#include "Programs.h"
#include "engine/graphics/Mesh.h"
#include "engine/graphics/Model.h"
#include "engine/graphics/RenderTarget.h"
#include "engine/graphics/SpriteOutline.h"
#include "engine/graphics/TextureSampler.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <glad/glad.h>
#include <stdexcept>

namespace engine::graphics {
using namespace math;
namespace {
RenderSettings checkedSettings(RenderSettings settings) {
    if (settings.n64Samples < 1 || settings.n64Size.width <= 0 || settings.n64Size.height <= 0) {
        throw std::invalid_argument("N64 sample count and dimensions must be positive");
    }
    if (!std::isfinite(settings.retroPixelStep) || settings.retroPixelStep < 0 ||
        !std::isfinite(settings.modelAffineStrength) || settings.modelAffineStrength < 0 ||
        settings.modelAffineStrength > 1) {
        throw std::invalid_argument("Retro pixel step must be nonnegative and affine strength must be in [0, 1]");
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
} // namespace
struct SceneRenderer::Impl {
    RenderSettings settings;
    detail::Programs programs;
    Mesh cube = makeCubeMesh();
    Mesh quad = makeQuadMesh();
    RenderTarget retro;
    RenderTarget shadow;
    std::unique_ptr<RenderTarget> n64;
    TextureSampler retroClamp;
    TextureSampler retroRepeat{true};
    RenderMode mode = RenderMode::Normal;
    Mat4 viewProjection;
    Mat4 lightProjection;
    Size renderSize;

    explicit Impl(RenderSettings config)
        : settings(config), retro(config.retroSize, RenderTarget::Kind::ColorDepth),
          shadow({config.shadowMapSize, config.shadowMapSize}, RenderTarget::Kind::Shadow) {}

    bool psx() const { return mode == RenderMode::Psx; }
    void snap(int uniform) const {
        glUniform3f(uniform, renderSize.width, renderSize.height, psx() ? settings.retroPixelStep : 0);
    }
    template <class Program> void surfaceTexture(const Program& program, const Texture& asset) const {
        texture(program.texture, asset.id());
        glBindSampler(0, psx() ? (asset.repeats() ? retroRepeat.id() : retroClamp.id()) : 0);
        glUniform1i(program.threePoint, mode == RenderMode::N64);
        glUniform1i(program.textureRepeat, asset.repeats());
        const auto size = asset.size();
        glUniform1i(program.maxTextureLod,
                    std::bit_width(static_cast<unsigned int>(std::max({size.width, size.height, 1}))) - 1);
    }
    template <class Program> void lit(const Program& program, const Mat4& model) const {
        program.shader.bind();
        matrix(program.mvp, multiply(viewProjection, model));
        matrix(program.model, model);
        matrix(program.lightViewProj, lightProjection);
        texture(program.shadowMap, shadow.texture(), 1);
        snap(program.snap);
    }
    template <class Program> void sunlight(const Program& program, const DirectionalLight& light) const {
        const Vec3 sun = normalize(light.direction);
        glUniform3f(program.sunDir, sun.x, sun.y, sun.z);
        glUniform1f(program.ambient, light.ambient);
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
        const float ambient = scene.light.ambientForNormal(command.normal);
        const float light = command.receivesShadow ? scene.light.lightForNormal(command.normal) : 1.0f;
        if (command.texture) {
            const auto& p = programs.road;
            lit(p, command.model);
            surfaceTexture(p, *command.texture);
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
            glUniform1f(p.affine, psx() ? std::clamp(command.affineStrength, 0.0f, 1.0f) : 0);
        } else {
            const auto& p = programs.plain;
            lit(p, command.model);
            color(p.color, command.color);
            glUniform1f(p.alpha, command.opacity);
            glUniform1f(p.light, light);
            glUniform1f(p.ambientLight, ambient);
            glUniform1i(p.receivesShadow, command.receivesShadow);
        }
        quad.draw();
    }
    void draw(const Cube& command, const Scene& scene) const {
        const auto& p = programs.cube;
        lit(p, command.model);
        sunlight(p, scene.light);
        color(p.color, command.color);
        cube.draw();
    }
    void draw(const ModelInstance& command, const Scene& scene) const {
        if (!command.model) {
            return;
        }
        const auto& p = programs.model;
        lit(p, command.transform);
        sunlight(p, scene.light);
        glUniform1f(p.affine, psx() ? settings.modelAffineStrength : 0);
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
        snap(p.snap);
        glUniform1f(p.affine, psx() ? std::clamp(command.affineStrength, 0.0f, 1.0f) : 0);
        glUniform1f(p.time, scene.time);
        glUniform1f(p.spawnTime, command.spawnTime);
        color(p.rippleColor, command.rippleColor);
        glUniform1f(p.aspect, command.aspect);
        surfaceTexture(p, *command.texture);
        const bool coverage = mode == RenderMode::N64 && n64->samples() > 1;
        if (coverage) {
            glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
            glDisable(GL_BLEND);
        }
        (command.profile ? *command.profile : quad).draw();
        if (coverage) {
            glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
            glEnable(GL_BLEND);
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
        snap(p.snap);
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
        matrix(sky.mvp, scale({2, 2, 1}));
        color(sky.topColor, scene.sky.top);
        color(sky.bottomColor, scene.sky.bottom);
        glUniform1f(sky.time, scene.time);
        glUniform1f(sky.nightBlend, scene.sky.nightBlend);
        quad.draw();

        if (scene.sky.sunSize <= 0) {
            glDepthMask(GL_TRUE);
            return;
        }
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        const Vec3 dir = normalize(scene.light.direction);
        const Vec3 position = {scene.camera.position.x + dir.x * scene.sky.sunDistance,
                               scene.camera.position.y + dir.y * scene.sky.sunDistance,
                               scene.camera.position.z + dir.z * scene.sky.sunDistance};
        const Mat4 model = multiply(translation(position), scale({scene.sky.sunSize, scene.sky.sunSize, 1}));
        const auto& sun = programs.sun;
        sun.shader.bind();
        matrix(sun.mvp, multiply(viewProjection, model));
        snap(sun.snap);
        color(sun.color, scene.sky.sunColor);
        glUniform1f(sun.core, scene.sky.sunCore);
        glUniform1f(sun.haze, scene.sky.sunHaze);
        quad.draw();
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_TRUE);
    }
    void menu(const MenuOverlay& overlay) const {
        if (!overlay.visible) {
            return;
        }
        glDisable(GL_DEPTH_TEST);
        const auto& program = programs.menu;
        program.shader.bind();
        matrix(program.mvp, scale({2, 2, 1}));
        glUniform1f(program.time, overlay.time);
        glUniform1i(program.selection, overlay.selection);
        quad.draw();
        glEnable(GL_DEPTH_TEST);
    }
    template <class Program> void present(Size drawable, const Program& p, unsigned int source) const {
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
        texture(p.texture, source);
        quad.draw();
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
    }
    void render(const Scene& scene, Size drawable) {
        if (drawable.width <= 0 || drawable.height <= 0) {
            return;
        }
        if (mode == RenderMode::N64 && !n64) {
            n64 = std::make_unique<RenderTarget>(settings.n64Size, RenderTarget::Kind::ColorDepth, settings.n64Samples);
        }
        renderSize = mode == RenderMode::Normal ? drawable
                     : mode == RenderMode::N64  ? settings.n64Size
                                                : settings.retroSize;
        viewProjection = scene.camera.viewProjection(static_cast<float>(renderSize.width) / renderSize.height);
        lightProjection = scene.light.viewProjection();

        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_FRAMEBUFFER_SRGB);
        if (mode == RenderMode::N64) {
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

        if (mode == RenderMode::N64) {
            n64->bind();
        } else if (psx()) {
            retro.bind();
        } else {
            RenderTarget::bindDefault(drawable);
        }
        glClearColor(scene.sky.bottom.r, scene.sky.bottom.g, scene.sky.bottom.b, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        background(scene);
        menu(scene.menu);
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
        if (mode == RenderMode::N64) {
            n64->resolve();
            present(drawable, programs.n64, n64->texture());
        } else if (psx()) {
            present(drawable, programs.retro, retro.texture());
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindVertexArray(0);
        glBindSampler(0, 0);
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
} // namespace engine::graphics
