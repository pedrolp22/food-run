#pragma once

#include "engine/graphics/RenderMode.h"
#include "game/Entities.h"
#include <filesystem>
#include <memory>
#include <span>
#include <string>

// Game-facing facade. Reusable rendering lives in engine/graphics/SceneRenderer.h.
class Renderer {
  public:
    using EntityType = game::EntityType;
    using Entity = game::Entity;
    using CollectEffect = game::CollectEffect;
    explicit Renderer(std::filesystem::path assetRoot = {});
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    bool initialize();
    int initSDL() { return initialize() ? 0 : -1; } // Compatibility with existing callers.
    void resizeViewport() const;
    void setWindowTitle(const std::string& title) const;
    void toggleRetroMode() const;
    void toggleDitheredShadows() const;
    bool ditheredShadows() const;
    void toggleDistanceFog() const;
    bool distanceFog() const;
    engine::graphics::RenderMode renderMode() const;
    void renderMenu(float time, int selection) const;
    void render(float playerX, float roadScroll, float gameTime, int score, std::span<const Entity> entities,
                std::span<const CollectEffect> collectEffects, float effectRemainder = 0) const;

  private:
    struct Impl;
    std::filesystem::path assetRoot_;
    std::unique_ptr<Impl> impl_;
};
