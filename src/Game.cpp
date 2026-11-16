#include "Game.h"
#include "GameConfig.h"
#include <SDL.h>
#include <algorithm>
#include <chrono>

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
    float menuTime = 0;
    while (running_) {
        const auto now = Clock::now();
        const float elapsed = std::chrono::duration<float>(now - previousFrame).count();
        previousFrame = now;
        handleEvents();
        if (!running_) {
            break;
        }
        if (controlsHelpShown_ && controlsHelpElapsed_ < 7.0f)
            controlsHelpElapsed_ = std::min(7.0f, controlsHelpElapsed_ + std::max(elapsed, 0.0f));
        if (gameOverTransitionActive_) {
            const float transitionDuration = GameConfig::kGameOverHudFadeSeconds + GameConfig::kGameOverPanelDropSeconds;
            gameOverElapsed_ = std::min(gameOverElapsed_ + std::max(elapsed, 0.0f), transitionDuration);
            if (gameOverElapsed_ >= transitionDuration) {
                gameOverInteractive_ = true;
                inMenu_ = true;
            }
            if (inMenu_ && menuPendingActivation_ >= 0) {
                menuClickRemaining_ -= std::max(elapsed, 0.0f);
                if (menuClickRemaining_ <= 0) {
                    menuSelection_ = menuPendingActivation_;
                    menuPendingActivation_ = -1;
                    menuMouseDown_ = -1;
                    activateMenuSelection();
                    if (!gameOverTransitionActive_ || !running_)
                        continue;
                }
            }
            const auto& state = simulation_.state();
            renderer_.renderGameOver(state.playerX, state.roadScroll, state.time, state.score, state.entities,
                                     state.collectEffects, state.dayNight.nightBlend(0), gameOverCameraBlend_, state.health,
                                     state.healthyFoods, controlsHelpAlpha(), gameOverElapsed_, menuSelection_,
                                     menuMouseDown_ == menuMouseHover_ ? menuMouseDown_ : -1);
            continue;
        }
        if (inMenu_) {
            menuTime += std::max(elapsed, 0.0f);
            if (menuPendingActivation_ >= 0) {
                menuClickRemaining_ -= std::max(elapsed, 0.0f);
                if (menuClickRemaining_ <= 0) {
                    menuSelection_ = menuPendingActivation_;
                    menuPendingActivation_ = -1;
                    menuMouseDown_ = -1;
                    activateMenuSelection();
                    if (!inMenu_ || !running_)
                        continue;
                }
            }
            renderer_.renderMenu(menuTime, menuSelection_,
                                 menuMouseDown_ == menuMouseHover_ ? menuMouseDown_ : -1,
                                 simulation_.state().score, simulation_.state().healthyFoods);
            continue;
        }
        float cameraBlend = 1;
        if (cameraTransitionActive_) {
            cameraTransitionElapsed_ = std::min(cameraTransitionElapsed_ + std::max(elapsed, 0.0f),
                                                GameConfig::kCameraTransitionSeconds);
            cameraBlend = cameraTransitionElapsed_ / GameConfig::kCameraTransitionSeconds;
            cameraTransitionActive_ = cameraBlend < 1;
        }
        clock_.advance(elapsed, [this](float dt) { simulation_.update(dt); });
        const auto& state = simulation_.state();
        if (state.gameOver) {
            gameOverTransitionActive_ = true;
            gameOverInteractive_ = false;
            gameOverElapsed_ = 0;
            gameOverCameraBlend_ = cameraBlend;
            showMenuPage(Renderer::MenuPage::GameOver);
            continue;
        }
        // Animate at presentation frequency, retaining the fixed timestep for game rules.
        // Freeze at the final simulation time on game over; resetGame also clears the remainder.
        const float visualTime = state.time + (state.gameOver ? 0 : clock_.remainder());
        renderer_.render(state.playerX, state.roadScroll, visualTime, state.score, state.entities, state.collectEffects,
                         clock_.remainder(), state.dayNight.nightBlend(state.gameOver ? 0 : clock_.remainder()),
                         state.gameOver ? 0 : state.speed, cameraBlend, state.health,
                         controlsHelpAlpha(), state.healthyFoods);
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
        if (gameOverTransitionActive_ && !gameOverInteractive_)
            continue;
        if (inMenu_ && (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN ||
                        event.type == SDL_MOUSEBUTTONUP)) {
            if (menuPendingActivation_ >= 0)
                continue;
            if (event.type == SDL_MOUSEMOTION) {
                const int hit = renderer_.menuHitTest(event.motion.x, event.motion.y);
                menuMouseHover_ = hit;
                if (hit >= 0)
                    menuSelection_ = hit;
            } else if (event.button.button == SDL_BUTTON_LEFT) {
                const int icon = renderer_.developerIconHitTest(event.button.x, event.button.y);
                if (event.type == SDL_MOUSEBUTTONDOWN) {
                    developerIconDown_ = icon;
                    if (icon >= 0) {
                        menuMouseDown_ = -1;
                        continue;
                    }
                } else if (developerIconDown_ >= 0) {
                    if (icon == developerIconDown_) {
                        renderer_.clickDeveloperIcon(icon, event.button.clicks >= 2 && lastDeveloperIconClick_ == icon);
                        lastDeveloperIconClick_ = icon;
                    }
                    developerIconDown_ = -1;
                    continue;
                }
                const int hit = renderer_.menuHitTest(event.button.x, event.button.y);
                menuMouseHover_ = hit;
                if (event.type == SDL_MOUSEBUTTONDOWN) {
                    menuMouseDown_ = hit;
                    if (hit >= 0)
                        menuSelection_ = hit;
                } else {
                    if (hit >= 0 && hit == menuMouseDown_) {
                        menuPendingActivation_ = hit;
                        menuClickRemaining_ = .14f;
                    } else {
                        menuMouseDown_ = -1;
                    }
                }
            }
            continue;
        }
        if (inMenu_ && menuPendingActivation_ >= 0)
            continue;
        if (inMenu_ && event.type == SDL_MOUSEWHEEL) {
            const int count = renderer_.menuItemCount();
            const int direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
            const int steps = (event.wheel.y * direction) % count;
            menuSelection_ = (menuSelection_ - steps + count) % count;
            menuMouseDown_ = menuMouseHover_ = -1;
            continue;
        }
        if (event.type == SDL_KEYDOWN && event.key.repeat) {
            const auto key = event.key.keysym.sym;
            if (!inMenu_ || (key != SDLK_UP && key != SDLK_DOWN && key != SDLK_w && key != SDLK_s))
                continue;
        }
        if (event.type != SDL_KEYDOWN) {
            if (inMenu_ && event.type == SDL_CONTROLLERBUTTONDOWN) {
                const int count = renderer_.menuItemCount();
                switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    menuSelection_ = (menuSelection_ + count - 1) % count;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    menuSelection_ = (menuSelection_ + 1) % count;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                    renderer_.adjustMenuItem(menuSelection_, -1);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                    renderer_.adjustMenuItem(menuSelection_, 1);
                    break;
                case SDL_CONTROLLER_BUTTON_A:
                    activateMenuSelection();
                    break;
                case SDL_CONTROLLER_BUTTON_B:
                    backMenu();
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
            const int count = renderer_.menuItemCount();
            switch (event.key.keysym.sym) {
            case SDLK_UP:
            case SDLK_w:
                menuSelection_ = (menuSelection_ + count - 1) % count;
                break;
            case SDLK_DOWN:
            case SDLK_s:
                menuSelection_ = (menuSelection_ + 1) % count;
                break;
            case SDLK_RETURN:
            case SDLK_SPACE:
                activateMenuSelection();
                break;
            case SDLK_LEFT:
            case SDLK_a:
                renderer_.adjustMenuItem(menuSelection_, -1);
                break;
            case SDLK_RIGHT:
            case SDLK_d:
                renderer_.adjustMenuItem(menuSelection_, 1);
                break;
            case SDLK_ESCAPE:
                backMenu();
                break;
            case SDLK_BACKSPACE:
                if (menuPage_ != Renderer::MenuPage::Main)
                    backMenu();
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
        default:
            break;
        }
    }
}
void Game::showMenuPage(Renderer::MenuPage page, int selection) {
    menuPage_ = page;
    menuSelection_ = selection;
    menuMouseDown_ = -1;
    menuMouseHover_ = -1;
    developerIconDown_ = -1;
    lastDeveloperIconClick_ = -1;
    menuPendingActivation_ = -1;
    renderer_.setMenuPage(page, selection);
}
void Game::backMenu() {
    using Page = Renderer::MenuPage;
    switch (menuPage_) {
    case Page::Main: running_ = false; break;
    case Page::Credits: showMenuPage(Page::Main, 2); break;
    case Page::Options: showMenuPage(Page::Main, 1); break;
    case Page::GameOver:
        gameOverTransitionActive_ = false;
        gameOverInteractive_ = false;
        resetGame();
        showMenuPage(Page::Main);
        break;
    }
}
float Game::controlsHelpAlpha() const {
    if (!controlsHelpShown_ || controlsHelpElapsed_ >= 7.0f)
        return 0;
    return controlsHelpElapsed_ <= 6.0f ? 1.0f : 7.0f - controlsHelpElapsed_;
}
void Game::activateMenuSelection() {
    using Page = Renderer::MenuPage;
    if (renderer_.adjustMenuItem(menuSelection_))
        return;
    if (menuPage_ == Page::GameOver) {
        switch (menuSelection_) {
        case 0:
            gameOverTransitionActive_ = false;
            gameOverInteractive_ = false;
            resetGame();
            showMenuPage(Page::Main);
            inMenu_ = false;
            cameraTransitionElapsed_ = 0;
            cameraTransitionActive_ = true;
            break;
        case 1:
            gameOverTransitionActive_ = false;
            gameOverInteractive_ = false;
            resetGame();
            showMenuPage(Page::Main);
            break;
        }
        return;
    }
    if (menuPage_ == Page::Main) {
        switch (menuSelection_) {
        case 0:
            resetGame();
            cameraTransitionElapsed_ = 0;
            cameraTransitionActive_ = true;
            inMenu_ = false;
            if (!controlsHelpShown_) {
                controlsHelpShown_ = true;
                controlsHelpElapsed_ = 0;
            }
            break;
        case 1: showMenuPage(Page::Options); break;
        case 2: showMenuPage(Page::Credits); break;
        case 3: running_ = false; break;
        }
    } else {
        backMenu();
    }
}
void Game::resetGame() {
    simulation_.reset();
    clock_.reset();
}
