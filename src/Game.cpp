#include "Game.h"
#include "GameConfig.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

Game::~Game() {
    if (controller_) {
        SDL_GameControllerClose(controller_);
    }
}
int Game::run() {
    if (!renderer_.initialize()) {
        return 1;
    }
    using Clock = std::chrono::steady_clock;
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0 && SDL_NumJoysticks() > 0 && SDL_IsGameController(0)) {
        controller_ = SDL_GameControllerOpen(0);
    }
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
        if (inMenu_) {
            renderer_.renderMenu(std::chrono::duration<float>(now.time_since_epoch()).count(), menuSelection_);
            continue;
        }
        float cameraBlend = 1;
        if (cameraTransitionActive_) {
            cameraTransitionElapsed_ = std::min(cameraTransitionElapsed_ + std::max(elapsed, 0.0f),
                                                GameConfig::kCameraTransitionSeconds);
            cameraBlend = cameraTransitionElapsed_ / GameConfig::kCameraTransitionSeconds;
            cameraTransitionActive_ = cameraBlend < 1;
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
        renderer_.render(state.playerX, state.roadScroll, visualTime, state.score, state.entities, state.collectEffects,
                         clock_.remainder(), state.dayNight.nightBlend(state.gameOver ? 0 : clock_.remainder()),
                         state.gameOver ? 0 : state.speed, cameraBlend);
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
            if (inMenu_ && event.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    menuSelection_ = (menuSelection_ + 3) % 4;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    menuSelection_ = (menuSelection_ + 1) % 4;
                    break;
                case SDL_CONTROLLER_BUTTON_A:
                    activateMenuSelection();
                    break;
                default:
                    break;
                }
            } else if (!controller_ && event.type == SDL_CONTROLLERDEVICEADDED &&
                       SDL_IsGameController(event.cdevice.which)) {
                controller_ = SDL_GameControllerOpen(event.cdevice.which);
            }
            continue;
        }
        if (inMenu_) {
            switch (event.key.keysym.sym) {
            case SDLK_UP:
            case SDLK_w:
                menuSelection_ = (menuSelection_ + 3) % 4;
                break;
            case SDLK_DOWN:
            case SDLK_s:
                menuSelection_ = (menuSelection_ + 1) % 4;
                break;
            case SDLK_RETURN:
            case SDLK_SPACE:
                activateMenuSelection();
                break;
            case SDLK_ESCAPE:
                running_ = false;
                break;
            default:
                break;
            }
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
        case SDLK_f:
            renderer_.toggleDistanceFog();
            updateWindowTitle();
            break;
        case SDLK_h:
            renderer_.toggleDitheredShadows();
            updateWindowTitle();
            break;
        default:
            break;
        }
    }
}
void Game::activateMenuSelection() {
    if (menuSelection_ == 0) {
        resetGame();
        cameraTransitionElapsed_ = 0;
        cameraTransitionActive_ = true;
        inMenu_ = false;
    } else if (menuSelection_ == 3) {
        running_ = false;
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
    if (renderer_.renderMode() == engine::graphics::RenderMode::Psx && renderer_.ditheredShadows()) {
        title << " (Dithered Shadows)";
    }
    if (renderer_.renderMode() == engine::graphics::RenderMode::Psx && renderer_.distanceFog()) {
        title << " (Fog)";
    }
    if (state.gameOver) {
        title << " | Game Over - Press R | Score " << state.score;
    } else {
        title << " | Score " << state.score << " | Health " << state.health << " | Speed " << std::fixed
              << std::setprecision(1) << state.speed << " | Purple " << state.purpleStreak << "/3";
    }
    renderer_.setWindowTitle(title.str());
}
