#include "Game.h"
#include <SDL2/SDL.h>
#include <chrono>
#include <iomanip>
#include <sstream>

int Game::run() {
    if (!renderer_.initialize()) {
        return 1;
    }
    using Clock = std::chrono::steady_clock;
    resetGame();
    running_ = true;
    auto previousFrame = Clock::now();
    while (running_) {
        const auto now = Clock::now();
        const float elapsed = std::chrono::duration<float>(now - previousFrame).count();
        previousFrame = now;
        handleEvents();
        if (!running_) {
            break;
        }
        const bool wasGameOver = simulation_.state().gameOver;
        clock_.advance(elapsed, [this](float dt) {
            simulation_.update(dt);
            titleUpdateTimer_ -= dt;
        });
        const auto& state = simulation_.state();
        if (titleUpdateTimer_ <= 0 || state.gameOver != wasGameOver) {
            updateWindowTitle();
            titleUpdateTimer_ = 0.15f;
        }
        // Animate at presentation frequency, retaining the fixed timestep for game rules.
        // Freeze at the final simulation time on game over; resetGame also clears the remainder.
        const float visualTime = state.time + (state.gameOver ? 0 : clock_.remainder());
        renderer_.render(state.playerX, state.roadScroll, visualTime, state.entities, state.collectEffects,
                         clock_.remainder());
    }
    return 0;
}
void Game::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running_ = false;
            continue;
        }
        if (event.type != SDL_KEYDOWN || event.key.repeat) {
            continue;
        }
        switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
            running_ = false;
            break;
        case SDLK_LEFT:
        case SDLK_a:
            simulation_.moveLane(-1);
            break;
        case SDLK_RIGHT:
        case SDLK_d:
            simulation_.moveLane(1);
            break;
        case SDLK_r:
            resetGame();
            break;
        case SDLK_g:
            renderer_.toggleRetroMode();
            updateWindowTitle();
            break;
        case SDLK_n:
            renderer_.toggleN64Mode();
            updateWindowTitle();
            break;
        default:
            break;
        }
    }
}
void Game::resetGame() {
    simulation_.reset();
    clock_.reset();
    titleUpdateTimer_ = 0;
    updateWindowTitle();
}
void Game::updateWindowTitle() {
    const auto& state = simulation_.state();
    std::ostringstream title;
    title << GameConfig::kWindowTitle;
    title << " | " << engine::graphics::renderModeName(renderer_.renderMode());
    if (state.gameOver) {
        title << " | Game Over - Press R | Score " << state.score;
    } else {
        title << " | Score " << state.score << " | Health " << state.health << " | Speed " << std::fixed
              << std::setprecision(1) << state.speed << " | Purple " << state.purpleStreak << "/3";
    }
    renderer_.setWindowTitle(title.str());
}
