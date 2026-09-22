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
    engine::graphics::Size drawable;
    explicit Impl(const std::filesystem::path& root)
        : window({GameConfig::kWindowTitle, {720, 1280}}),
          graphics({.retroSize = {GameConfig::kPsxInternalWidth, GameConfig::kPsxInternalHeight},
                    .n64Size = {GameConfig::kN64InternalWidth, GameConfig::kN64InternalHeight}}),
          assets(root, GameConfig::kMipmappedTextures), world(assets), drawable(window.drawableSize()) {}
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
void Renderer::resizeViewport() {
    if (impl_) {
        impl_->drawable = impl_->window.drawableSize();
    }
}
void Renderer::setWindowTitle(const std::string& title) const {
    if (impl_) {
        impl_->window.setTitle(title);
    }
}
void Renderer::toggleRetroMode() {
    if (impl_) {
        impl_->graphics.setRetroMode(!impl_->graphics.retroMode());
    }
}
void Renderer::toggleN64Mode() {
    if (impl_) {
        using engine::graphics::RenderMode;
        impl_->graphics.setMode(impl_->graphics.mode() == RenderMode::N64 ? RenderMode::Normal : RenderMode::N64);
    }
}
engine::graphics::RenderMode Renderer::renderMode() const {
    return impl_ ? impl_->graphics.mode() : engine::graphics::RenderMode::Normal;
}
void Renderer::renderMenu(float time, int selection) {
    if (!impl_) {
        return;
    }
    resizeViewport();
    if (impl_->drawable.width <= 0 || impl_->drawable.height <= 0) {
        return;
    }
    engine::graphics::Scene menuScene;
    menuScene.time = time;
    menuScene.menu = {.visible = true, .selection = selection, .time = time};
    impl_->graphics.render(menuScene, impl_->drawable);
    impl_->window.swap();
}
void Renderer::render(float playerX, float roadScroll, float gameTime, int score, std::span<const Entity> entities,
                      std::span<const CollectEffect> effects, float effectRemainder) {
    if (!impl_) {
        return;
    }
    resizeViewport();
    if (impl_->drawable.width <= 0 || impl_->drawable.height <= 0) {
        return;
    }
    impl_->graphics.render(impl_->world.build(playerX, roadScroll, gameTime, score, entities, effects, effectRemainder),
                           impl_->drawable);
    impl_->window.swap();
}
