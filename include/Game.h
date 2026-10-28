#pragma once

struct _SDL_GameController;

#include "Renderer.h"
#include "engine/time/FixedStep.h"
#include "game/Simulation.h"

// Application shell: input, timing, presentation. Simulation contains the game rules.
class Game {
  public:
    Game() = default;
    ~Game();
    int run();

  private:
    void handleEvents();
    void resetGame();
    void updateWindowTitle();
    void activateMenuSelection();
    Renderer renderer_;
    game::Simulation simulation_;
    engine::time::FixedStep clock_;
    bool running_ = false;
    bool inMenu_ = true;
    bool cameraTransitionActive_ = false;
    float cameraTransitionElapsed_ = 0;
    int menuSelection_ = 0;
    int menuMouseDown_ = -1;
    int menuMouseHover_ = -1;
    int menuPendingActivation_ = -1;
    float menuClickRemaining_ = 0;
    _SDL_GameController* controller_ = nullptr;
    float titleUpdateTimer_ = 0;
};
