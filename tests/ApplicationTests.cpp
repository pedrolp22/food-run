#include "Game.h"
#include <SDL2/SDL.h>
#include <chrono>
#include <thread>

int main() {
    using namespace std::chrono_literals;
    // Own an event-subsystem reference before another thread uses its queue.
    if (SDL_InitSubSystem(SDL_INIT_EVENTS) != 0) {
        return 1;
    }
    int result = 1;
    {
        Game game;
        std::jthread input([](std::stop_token stop) {
            std::this_thread::sleep_for(100ms);
            for (auto key : {SDLK_RETURN, SDLK_f, SDLK_g, SDLK_f, SDLK_h, SDLK_f, SDLK_g, SDLK_h,
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
    return result;
}
