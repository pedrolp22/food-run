#include "Game.h"
#include "TestSupport.h"
#include <SDL2/SDL.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>

namespace {
SDL_Window* gameWindow() {
    for (unsigned id = 1; id < 256; ++id) {
        if (auto* window = SDL_GetWindowFromID(id); window &&
            std::strncmp(SDL_GetWindowTitle(window), "Teste", 5) == 0)
            return window;
    }
    return nullptr;
}
std::array<float, 2> menuHitChecks() {
    Renderer renderer;
    CHECK(renderer.initialize());
    // Let the window manager apply any screen-size constraint before sampling.
    for (int i = 0; i < 8; ++i) {
        SDL_PumpEvents();
        renderer.renderMenu(i * .05f, 0);
        SDL_Delay(15);
    }
    auto* window = gameWindow();
    CHECK(window != nullptr);
    const auto checkLayout = [&] {
        int width = 0, height = 0;
        SDL_GetWindowSize(window, &width, &height);
        std::array<int, 4> count{}, sumX{}, sumY{};
        for (int y = 0; y < height; y += 8) {
            for (int x = 0; x < width; x += 8) {
                const int hit = renderer.menuHitTest(x, y);
                if (hit >= 0) {
                    CHECK(hit < 4);
                    ++count[hit];
                    sumX[hit] += x;
                    sumY[hit] += y;
                }
            }
        }
        for (int i = 0; i < 4; ++i) {
            CHECK(count[i] > 3);
            CHECK(renderer.menuHitTest(sumX[i] / count[i], sumY[i] / count[i]) == i);
        }
        CHECK(renderer.menuHitTest(-1, 0) == -1);
        CHECK(renderer.menuHitTest(width, height) == -1);
        return std::array<float, 2>{float(sumX[0]) / count[0] / width, float(sumY[0]) / count[0] / height};
    };
    const auto play = checkLayout();
    SDL_SetWindowSize(window, 1000, 650);
    for (int i = 0; i < 8; ++i) {
        SDL_PumpEvents();
        renderer.renderMenu(.5f + i * .05f, 0);
        SDL_Delay(15);
    }
    checkLayout();
    return play;
}
} // namespace

int main() {
    using namespace std::chrono_literals;
    // Own an event-subsystem reference before another thread uses its queue.
    if (SDL_InitSubSystem(SDL_INIT_EVENTS) != 0) {
        return 1;
    }
    int result = 1;
    const auto play = menuHitChecks();
    std::atomic<bool> mouseStartedGame = false;
    {
        Game game;
        std::jthread input([&](std::stop_token stop) {
            std::this_thread::sleep_for(500ms);
            auto* window = gameWindow();
            if (!window)
                return;
            int width = 0, height = 0;
            SDL_GetWindowSize(window, &width, &height);
            SDL_Event mouse{};
            mouse.type = SDL_MOUSEMOTION;
            mouse.motion.x = int(play[0] * width);
            mouse.motion.y = int(play[1] * height);
            SDL_PushEvent(&mouse);
            mouse = {};
            mouse.type = SDL_MOUSEBUTTONDOWN;
            mouse.button.button = SDL_BUTTON_LEFT;
            mouse.button.x = int(play[0] * width);
            mouse.button.y = int(play[1] * height);
            SDL_PushEvent(&mouse);
            mouse.type = SDL_MOUSEBUTTONUP;
            SDL_PushEvent(&mouse);
            std::this_thread::sleep_for(700ms);
            mouseStartedGame = std::strstr(SDL_GetWindowTitle(window), "Score") != nullptr;
            for (auto key : {SDLK_f, SDLK_g, SDLK_f, SDLK_h, SDLK_f, SDLK_g, SDLK_h,
                             SDLK_RIGHT, SDLK_r, SDLK_LEFT, SDLK_g, SDLK_f, SDLK_h, SDLK_ESCAPE}) {
                if (stop.stop_requested()) {
                    return;
                }
                SDL_Event event{};
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = key;
                SDL_PushEvent(&event);
                std::this_thread::sleep_for(40ms);
            }
        });
        result = game.run();
        input.request_stop();
    }
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
    return result == 0 && mouseStartedGame ? 0 : 1;
}
