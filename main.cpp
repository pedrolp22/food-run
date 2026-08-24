#include "Renderer.h"

int main() {
    Renderer renderer;
    if (renderer.initSDL() != 0) {
        return 1;
    }

    renderer.run();
    return 0;
}
