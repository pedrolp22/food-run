#include "Game.h"
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "Food Run " << FOODRUN_VERSION << '\n';
        return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--smoke-test") {
        // Exercise the installed game's assets, shaders and OpenGL context without
        // opening a visible window or waiting for player input.
        Renderer renderer({}, {}, false);
        if (!renderer.initialize())
            return 1;
        for (int frame = 0; frame < 3; ++frame)
            renderer.renderMenu(frame / 60.0f, 0);
        return 0;
    }
    if (argc != 1) {
        std::cerr << "Usage: foodrun [--version | --smoke-test]\n";
        return 1;
    }
    Game game;
    return game.run();
}
