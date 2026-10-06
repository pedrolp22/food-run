#include "Renderer.h"
#include "GameConfig.h"
#include "engine/graphics/SceneRenderer.h"
#include "engine/platform/Window.h"
#include "game/WorldScene.h"
#include <iostream>
#include <utility>

struct Renderer::Impl {
    // Destruction runs in reverse order: all GPU assets die before the window's GL context.
    engine::platform::Window window;
    engine::graphics::SceneRenderer graphics;
    game::GameAssets assets;
    game::WorldScene world;
    engine::graphics::Scene menuScene;
    engine::graphics::Size drawable;
    explicit Impl(const std::filesystem::path& root)
        : window({.title = GameConfig::kWindowTitle, .size = {.width = 720, .height = 1280}}),
          graphics({.retroSize = {.width = GameConfig::kPsxInternalWidth, .height = GameConfig::kPsxInternalHeight},
                    .retroDitherStrength = GameConfig::kPsxDitherStrength,
                    .psxDitheredShadows = GameConfig::kPsxDitheredShadows,
                    .psxDitheredShadowDarkness = GameConfig::kPsxDitheredShadowDarkness,
                    .psxDistanceFog = GameConfig::kPsxDistanceFog,
                    .psxFogStart = GameConfig::kPsxFogStart,
                    .psxFogEnd = GameConfig::kPsxFogEnd,
                    .psxFogColor = {GameConfig::kPsxFogColor[0], GameConfig::kPsxFogColor[1], GameConfig::kPsxFogColor[2]},
                    .psxNightFogColor = {GameConfig::kPsxNightFogColor[0], GameConfig::kPsxNightFogColor[1],
                                         GameConfig::kPsxNightFogColor[2]},
                    .psxFogFollowsSky = GameConfig::kPsxFogFollowsSky}),
          assets(root, GameConfig::kMipmappedTextures), world(assets),
          menuScene(world.build(0, 0, 0, {}, {})), drawable(window.drawableSize()) {}
};
Renderer::Renderer(std::filesystem::path assetRoot) : assetRoot_(std::move(assetRoot)) {
    if (assetRoot_.empty()) {
        assetRoot_ = SUBWAY_ASSET_DIR;
    }
}
Renderer::~Renderer() = default;
bool Renderer::initialize() {
    if (impl_) {
        return true;
    }
    try {
        impl_ = std::make_unique<Impl>(assetRoot_);
        return true;
    } catch (const std::exception& error) {
        std::cerr << "Renderer initialization failed: " << error.what() << '\n';
        return false;
    }
}
void Renderer::resizeViewport() const {
    if (impl_) {
        impl_->drawable = impl_->window.drawableSize();
    }
}
void Renderer::setWindowTitle(const std::string& title) const {
    if (impl_) {
        impl_->window.setTitle(title);
    }
}
void Renderer::toggleRetroMode() const {
    if (impl_) {
        impl_->graphics.setRetroMode(!impl_->graphics.retroMode());
    }
}
void Renderer::toggleDitheredShadows() const {
    if (impl_) {
        impl_->graphics.setPsxDitheredShadows(!impl_->graphics.psxDitheredShadows());
    }
}
bool Renderer::ditheredShadows() const {
    return impl_ && impl_->graphics.psxDitheredShadows();
}
void Renderer::toggleDistanceFog() const {
    if (impl_) {
        impl_->graphics.setPsxDistanceFog(!impl_->graphics.psxDistanceFog());
    }
}
bool Renderer::distanceFog() const {
    return impl_ && impl_->graphics.psxDistanceFog();
}
engine::graphics::RenderMode Renderer::renderMode() const {
    return impl_ ? impl_->graphics.mode() : engine::graphics::RenderMode::Normal;
}
void Renderer::renderMenu(const float time, const int selection) const {
    if (!impl_) {
        return;
    }
    resizeViewport();
    if (impl_->drawable.width <= 0 || impl_->drawable.height <= 0) {
        return;
    }
    // Keep the opening world frozen; only the menu's selection pulse uses UI time.
    impl_->menuScene.menu = {.visible = true, .selection = selection, .time = time};
    impl_->graphics.render(impl_->menuScene, impl_->drawable);
    impl_->window.swap();
}
void Renderer::render(const float playerX, const float roadScroll, const float gameTime,
                      [[maybe_unused]] const int score, const std::span<const Entity> entities,
                      const std::span<const CollectEffect> collectEffects, const float effectRemainder,
                      const float nightBlend) const {
    if (!impl_) {
        return;
    }
    resizeViewport();
    if (impl_->drawable.width <= 0 || impl_->drawable.height <= 0) {
        return;
    }
    impl_->graphics.render(
        impl_->world.build(playerX, roadScroll, gameTime, entities, collectEffects, effectRemainder, nightBlend),
        impl_->drawable);
    impl_->window.swap();
}
