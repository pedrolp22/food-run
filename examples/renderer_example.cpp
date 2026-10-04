#include "engine/graphics/SceneRenderer.h"
#include "engine/platform/Window.h"
#include <SDL2/SDL.h>
#include <chrono>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    try {
        using namespace engine::graphics;
        using namespace engine::math;
        const int frameLimit = argc > 1 ? std::stoi(argv[1]) : 0;
        engine::platform::Window window({"Reusable renderer — G: PSX, F: fog, Esc: quit", {960, 640}});
        SceneRenderer renderer;
        Scene scene;
        scene.camera.position = {0, 5, 9};
        scene.camera.target = {0, 0, 0};
        scene.light.shadow.halfWidth = 20;
        scene.light.shadow.halfHeight = 20;
        Quad ground;
        ground.model = multiply(rotateX(kPi * 0.5f), scale({20, 20, 1}));
        ground.color = {0.65f, 0.67f, 0.61f};
        scene.objects.emplace_back(ground);
        for (int i = -1; i <= 1; ++i) {
            scene.objects.emplace_back(Cube{multiply(translation({i * 2.2f, 0.5f, 0}), scale({1, 1, 1})),
                                            {0.2f + 0.2f * (i + 1), 0.5f, 0.85f - 0.2f * (i + 1)}});
        }
        bool running = true;
        int frame = 0;
        const auto start = std::chrono::steady_clock::now();
        while (running && (frameLimit <= 0 || frame < frameLimit)) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                }
                if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = false;
                    }
                    if (event.key.keysym.sym == SDLK_g) {
                        renderer.setRetroMode(!renderer.retroMode());
                    }
                    if (event.key.keysym.sym == SDLK_f) {
                        renderer.setPsxDistanceFog(!renderer.psxDistanceFog());
                    }
                }
            }
            scene.time = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
            renderer.render(scene, window.drawableSize());
            window.swap();
            ++frame;
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
