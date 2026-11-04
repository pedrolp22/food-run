#include "Game.h"
#include "TestSupport.h"
#include <SDL.h>
#include <glad/glad.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

namespace {
SDL_Window* gameWindow() {
    for (unsigned id = 1; id < 256; ++id) {
        if (auto* window = SDL_GetWindowFromID(id); window &&
            std::strncmp(SDL_GetWindowTitle(window), "Teste", 5) == 0)
            return window;
    }
    return nullptr;
}
struct MenuTargets {
    std::array<float, 2> play, options, credits, back;
    std::array<std::array<float, 2>, 3> icons;
};
MenuTargets menuHitChecks(const std::filesystem::path& config) {
    Renderer renderer({}, config, false);
    CHECK(renderer.initialize());
    for (int i = 0; i < 8; ++i) {
        SDL_PumpEvents();
        renderer.renderMenu(i * .05f, 0);
    }
    auto* window = gameWindow();
    CHECK(window != nullptr);
    const auto checkLayout = [&](int itemCount) {
        CHECK(renderer.menuItemCount() == itemCount);
        int width = 0, height = 0;
        SDL_GetWindowSize(window, &width, &height);
        std::array<int, 13> count{}, sumX{}, sumY{};
        for (int y = 0; y < height; y += 5) {
            for (int x = 0; x < width; x += 5) {
                const int hit = renderer.menuHitTest(x, y);
                if (hit >= 0) {
                    CHECK(hit < itemCount);
                    ++count[hit];
                    sumX[hit] += x;
                    sumY[hit] += y;
                }
            }
        }
        std::array<std::array<float, 2>, 13> centers{};
        for (int i = 0; i < itemCount; ++i) {
            CHECK(count[i] > 3); // Every control is visible and reachable on this one page.
            CHECK(renderer.menuHitTest(sumX[i] / count[i], sumY[i] / count[i]) == i);
            centers[i] = {float(sumX[i]) / count[i] / width, float(sumY[i]) / count[i] / height};
        }
        CHECK(renderer.menuHitTest(-1, 0) == -1);
        CHECK(renderer.menuHitTest(width, height) == -1);
        return centers;
    };
    const auto main = checkLayout(4);
    const auto checkIcons = [&] {
        int width = 0, height = 0;
        SDL_GetWindowSize(window, &width, &height);
        std::array<std::vector<std::array<int, 2>>, 3> pixels;
        for (int y = 0; y < height; y += 4) {
            for (int x = 0; x < width; x += 4) {
                const int hit = renderer.developerIconHitTest(x, y);
                if (hit >= 0) {
                    CHECK(hit < 3 && renderer.menuHitTest(x, y) == -1);
                    pixels[hit].push_back({x, y});
                }
            }
        }
        std::array<std::array<float, 2>, 3> centers{};
        for (int i = 0; i < 3; ++i) {
            CHECK(pixels[i].size() > 4);
            int sumX = 0, sumY = 0;
            for (const auto point : pixels[i]) {
                sumX += point[0];
                sumY += point[1];
            }
            const int cx = sumX / int(pixels[i].size()), cy = sumY / int(pixels[i].size());
            int best = std::numeric_limits<int>::max();
            for (const auto point : pixels[i]) {
                const int distance = (point[0] - cx) * (point[0] - cx) + (point[1] - cy) * (point[1] - cy);
                if (distance < best) {
                    centers[i] = {float(point[0]) / width, float(point[1]) / height};
                    best = distance;
                }
            }
            // The transparent corners inside the enclosing rectangle are not clickable.
            const auto [left, right] = std::minmax_element(pixels[i].begin(), pixels[i].end(),
                [](auto a, auto b) { return a[0] < b[0]; });
            const auto [top, bottom] = std::minmax_element(pixels[i].begin(), pixels[i].end(),
                [](auto a, auto b) { return a[1] < b[1]; });
            const int rectangleSamples = ((*right)[0] - (*left)[0] + 4) / 4 * ((*bottom)[1] - (*top)[1] + 4) / 4;
            CHECK(int(pixels[i].size()) < rectangleSamples);
        }
        CHECK(centers[0][1] < centers[1][1] && centers[1][1] < centers[2][1]);
        CHECK(renderer.developerIconHitTest(-1, 0) == -1);
        CHECK(renderer.developerIconHitTest(width, height) == -1);
        return centers;
    };
    using Page = Renderer::MenuPage;
    renderer.setMenuPage(Page::Credits);
    renderer.renderMenu(.4f, 0);
    const auto credits = checkLayout(1);
    const auto icons = checkIcons();
    renderer.clickDeveloperIcon(0);
    renderer.renderMenu(.43f, 0);
    renderer.clickDeveloperIcon(0, true);
    for (int frame = 0; frame < 20; ++frame)
        renderer.renderMenu(.43f + frame * .1f, 0);
    CHECK(checkIcons() == icons); // Both reactions finish; the portraits do not spin at idle.
    renderer.setMenuPage(Page::Options);
    CHECK(renderer.developerIconHitTest(int(icons[0][0] * 720), int(icons[0][1] * 1280)) == -1);
    renderer.renderMenu(.5f, 0);
    checkLayout(13);
    CHECK(renderer.menuItemValue(0) == "High");
    CHECK(renderer.adjustMenuItem(0));
    CHECK(renderer.menuItemValue(0) == "Low");
    CHECK(game::GraphicsSettings::load(config).renderScale == 50);
    CHECK(renderer.adjustMenuItem(0) && renderer.adjustMenuItem(0));
    CHECK(renderer.menuItemValue(0) == "High");
    CHECK(renderer.adjustMenuItem(1, -1));
    CHECK(renderer.graphicsSettings().renderScale == 75);
    CHECK(renderer.adjustMenuItem(2));
    CHECK(renderer.graphicsSettings().antiAliasing);
    CHECK(renderer.adjustMenuItem(4));
    CHECK(renderer.graphicsSettings().shadows == game::GraphicsQuality::Off);
    CHECK(renderer.adjustMenuItem(5));
    CHECK(!renderer.graphicsSettings().ambientOcclusion);
    CHECK(renderer.adjustMenuItem(6));
    CHECK(renderer.graphicsSettings().reflections == game::GraphicsQuality::Off);
    CHECK(renderer.adjustMenuItem(7) && renderer.adjustMenuItem(8));
    CHECK(!renderer.graphicsSettings().materialDetail && !renderer.graphicsSettings().foliageParticles);
    CHECK(renderer.adjustMenuItem(9) && renderer.adjustMenuItem(10) && renderer.adjustMenuItem(11));
    const auto saved = game::GraphicsSettings::load(config);
    CHECK(saved.retroMode && saved.ditheredShadows && !saved.distanceFog && saved.antiAliasing);
    renderer.renderMenu(.6f, 9);
    CHECK(!glIsEnabled(GL_DITHER)); // The menu must use the actual retro pipeline.
    checkLayout(13);
    CHECK(renderer.adjustMenuItem(9) && renderer.adjustMenuItem(10) && renderer.adjustMenuItem(11));
    CHECK(renderer.menuItemValue(0) == "Custom");
    CHECK(renderer.adjustMenuItem(0, -1) && renderer.adjustMenuItem(0));
    CHECK(renderer.menuItemValue(0) == "High");
    for (const auto size : {std::array{1000, 650}, std::array{720, 964}}) {
        SDL_SetWindowSize(window, size[0], size[1]);
        for (int retro = 0; retro <= 1; ++retro) {
            renderer.setMenuPage(Page::Options);
            if (retro)
                CHECK(renderer.adjustMenuItem(9));
            for (const auto page : {Page::Options, Page::Main, Page::Credits}) {
                renderer.setMenuPage(page);
                SDL_PumpEvents();
                renderer.renderMenu(1.0f + retro, 0);
                CHECK(bool(glIsEnabled(GL_DITHER)) == !retro);
                const auto centers = checkLayout(page == Page::Options ? 13 : page == Page::Main ? 4 : 1);
                if (page == Page::Credits)
                    checkIcons();
                if (page == Page::Options) {
                    // Extra is a heading between Particles and Retro mode, not another submenu.
                    const int x = int((centers[8][0] + centers[9][0]) * .5f * size[0]);
                    const int y = int((centers[8][1] + centers[9][1]) * .5f * size[1]);
                    CHECK(renderer.menuHitTest(x, y) == -1);
                }
            }
        }
        renderer.setMenuPage(Page::Options);
        CHECK(renderer.adjustMenuItem(9)); // Restore normal presentation for the next size.
    }
    return {main[0], main[1], main[2], credits[0], icons};
}
} // namespace

int main() {
    using namespace std::chrono_literals;
    // Own an event-subsystem reference before another thread uses its queue.
    if (SDL_InitSubSystem(SDL_INIT_EVENTS) != 0) {
        return 1;
    }
    int result = 1;
    const auto folder = std::filesystem::temp_directory_path() /
        ("subway-application-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(folder);
    struct Cleanup {
        std::filesystem::path folder;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(folder, ignored); }
    } cleanup{folder};
    const auto config = folder / "game.cfg";
    const auto targets = menuHitChecks(config);
    std::atomic<bool> mouseStartedGame = false, settingsApplied = false, shortcutsRemoved = false;
    {
        Game game(config, false);
        std::jthread input([&](std::stop_token stop) {
            const auto waitFor = [&](const auto& predicate) {
                const auto deadline = std::chrono::steady_clock::now() + 5s;
                while (!stop.stop_requested() && std::chrono::steady_clock::now() < deadline) {
                    if (predicate())
                        return true;
                    std::this_thread::sleep_for(20ms);
                }
                return false;
            };
            if (!waitFor([&] { return gameWindow() != nullptr; }))
                return;
            auto* window = gameWindow();
            int width = 0, height = 0;
            SDL_GetWindowSize(window, &width, &height);
            const auto click = [&](const std::array<float, 2>& point, int clicks = 1) {
                SDL_Event mouse{};
                mouse.type = SDL_MOUSEMOTION;
                mouse.motion.x = int(point[0] * width);
                mouse.motion.y = int(point[1] * height);
                SDL_PushEvent(&mouse);
                mouse = {};
                mouse.type = SDL_MOUSEBUTTONDOWN;
                mouse.button.button = SDL_BUTTON_LEFT;
                mouse.button.clicks = clicks;
                mouse.button.x = int(point[0] * width);
                mouse.button.y = int(point[1] * height);
                SDL_PushEvent(&mouse);
                mouse.type = SDL_MOUSEBUTTONUP;
                SDL_PushEvent(&mouse);
            };
            const auto key = [](SDL_Keycode code) {
                SDL_Event event{};
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = code;
                SDL_PushEvent(&event);
            };
            // Hidden windows isolate synthetic input from the desktop mouse/focus.
            // Keyboard navigation is processed in queue order, without transition timers.
            key(SDLK_DOWN); // Options.
            key(SDLK_RETURN);
            const auto wheel = [](int steps) {
                SDL_Event event{};
                event.type = SDL_MOUSEWHEEL;
                event.wheel.y = steps;
                SDL_PushEvent(&event);
            };
            wheel(-1); // Render scale, directly below Quality.
            key(SDLK_LEFT); // 100% -> 75%.
            wheel(-7); // Particles.
            SDL_Event heldDown{};
            heldDown.type = SDL_KEYDOWN;
            heldDown.key.keysym.sym = SDLK_DOWN;
            heldDown.key.repeat = 1;
            SDL_PushEvent(&heldDown); // Holding a navigation key also scrolls through the list.
            key(SDLK_RETURN); // Retro mode on, without entering another menu.
            key(SDLK_ESCAPE);
            settingsApplied = waitFor([&] {
                const auto changed = game::GraphicsSettings::load(config);
                return changed.renderScale == 75 && changed.retroMode;
            });
            key(SDLK_DOWN); // Credits, after returning from Options.
            key(SDLK_RETURN);
            for (const auto point : targets.icons) {
                click(point);
                click(point, 2);
            }
            key(SDLK_ESCAPE);
            key(SDLK_RETURN); // Returning keeps Credits selected.
            key(SDLK_RETURN); // Back.
            click(targets.play);
            mouseStartedGame = waitFor([&] { return std::strstr(SDL_GetWindowTitle(window), "Score") != nullptr; });
            key(SDLK_g);
            key(SDLK_f);
            key(SDLK_h);
            std::this_thread::sleep_for(200ms);
            shortcutsRemoved = std::strstr(SDL_GetWindowTitle(window), "PSX") != nullptr &&
                game::GraphicsSettings::load(config).retroMode;
            for (auto code : {SDLK_RIGHT, SDLK_r, SDLK_LEFT, SDLK_ESCAPE}) {
                if (stop.stop_requested())
                    return;
                key(code);
                std::this_thread::sleep_for(40ms);
            }
        });
        result = game.run();
        input.request_stop();
    }
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
    if (result == 0 && mouseStartedGame && settingsApplied && shortcutsRemoved)
        return 0;
    std::cerr << "Application checks: result=" << result << ", started=" << mouseStartedGame
              << ", settings=" << settingsApplied << ", shortcuts=" << shortcutsRemoved << '\n';
    return 1;
}
