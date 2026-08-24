//
// Created by pedroleonardo on 19/08/2026.
//

#ifndef SUBWAY_RENDERER_H
#define SUBWAY_RENDERER_H

#include <glad/glad.h>
#include <SDL2/SDL.h>

class Renderer {

    public:
    Renderer();
    ~Renderer();
    int initSDL();
    void render();
};


#endif //SUBWAY_RENDERER_H
