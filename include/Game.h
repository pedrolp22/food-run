#pragma once

struct _SDL_GameController;

#include "Renderer.h"
#include "engine/time/FixedStep.h"
#include "game/Simulation.h"
#include <utility>

// Application shell: input, timing, presentation. Simulation contains the game rules.
class Game {
  public:
    explicit Game(std::filesystem::path settingsFile = {}, bool visible = true)
        : renderer_({}, std::move(settingsFile), visible) {}
    ~Game();
    int run();

  private:
    void handleEvents();
    void resetGame();
    void activateMenuSelection();
    void showMenuPage(Renderer::MenuPage page, int selection = 0);
    void backMenu();
    float controlsHelpAlpha() const;
    Renderer renderer_;
    game::Simulation simulation_;
    engine::time::FixedStep clock_;
    bool running_ = false;
    bool inMenu_ = true;
    Renderer::MenuPage menuPage_ = Renderer::MenuPage::Main;
    bool cameraTransitionActive_ = false;
    float cameraTransitionElapsed_ = 0;
    int menuSelection_ = 0;
    int menuMouseDown_ = -1;
    int menuMouseHover_ = -1;
    int developerIconDown_ = -1;
    int lastDeveloperIconClick_ = -1;
    int menuPendingActivation_ = -1;
    float menuClickRemaining_ = 0;
    _SDL_GameController* controller_ = nullptr;
    bool controlsHelpShown_ = false;
    float controlsHelpElapsed_ = 0;
    bool gameOverTransitionActive_ = false;
    bool gameOverInteractive_ = false;
    float gameOverElapsed_ = 0;
    float gameOverCameraBlend_ = 1;
};
