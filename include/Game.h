#pragma once

#include "Renderer.h"
#include "engine/time/FixedStep.h"
#include "game/Simulation.h"

// Application shell: input, timing, presentation. Simulation contains the game rules.
class Game {
  public:
    Game() = default;
    int run();

  private:
    void handleEvents();
    void resetGame();
    void updateWindowTitle();
    Renderer renderer_;
    game::Simulation simulation_;
    engine::time::FixedStep clock_;
    bool running_ = false;
    float titleUpdateTimer_ = 0;
};
