#pragma once

#include "GameConfig.h"
#include "engine/graphics/RenderMode.h"
#include "game/Entities.h"
#include "game/GraphicsSettings.h"
#include <filesystem>
#include <memory>
#include <span>
#include <string>

// Game-facing facade. Reusable rendering lives in engine/graphics/SceneRenderer.h.
class Renderer {
  public:
    enum class MenuPage { Main, Credits, Options, GameOver };
    using EntityType = game::EntityType;
    using Entity = game::Entity;
    using CollectEffect = game::CollectEffect;
    explicit Renderer(std::filesystem::path assetRoot = {}, std::filesystem::path settingsFile = {}, bool visible = true);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    bool initialize();
    int initSDL() { return initialize() ? 0 : -1; } // Compatibility with existing callers.
    void resizeViewport() const;
    void setWindowTitle(const std::string& title) const;
    bool ditheredShadows() const;
    bool distanceFog() const;
    engine::graphics::RenderMode renderMode() const;
    void setMenuPage(MenuPage page, int selection = 0) const;
    int menuItemCount() const;
    std::string menuItemValue(int selection) const;
    bool adjustMenuItem(int selection, int direction = 1) const;
    game::GraphicsSettings graphicsSettings() const;
    int menuHitTest(int x, int y) const;
    int developerIconHitTest(int x, int y) const;
    void clickDeveloperIcon(int icon, bool doubleClick = false) const;
    void renderMenu(float time, int selection, int pressed = -1, int score = 0, int healthyFoods = 0) const;
    void render(float playerX, float roadScroll, float gameTime, int score, std::span<const Entity> entities,
                std::span<const CollectEffect> collectEffects, float effectRemainder = 0, float nightBlend = 0,
                float playerSpeed = 0, float cameraBlend = 1, int health = GameConfig::kMaxHealth,
                float controlsHelpAlpha = 0, int healthyFoods = 0) const;

  private:
    struct Impl;
    std::filesystem::path assetRoot_;
    std::filesystem::path settingsFile_;
    bool visible_ = true;
    std::unique_ptr<Impl> impl_;
};
