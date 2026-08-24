#include "../include/Renderer.h"

#include <iostream>
#include <ostream>

#include <glad/glad.h>
#include <SDL2/SDL.h>

#define W_WIDTH 800
#define W_HEIGHT 600
#define VSYNC 1

int initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO)<0) { std::cerr << "SDL initialization error: " << SDL_GetError() << std::endl;
                                           SDL_Quit(); return -1; }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4); //OGL 4.6
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow("Subway Money", // plataforma pagando muito rapaziada
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W_WIDTH, W_HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!window) { std::cerr << "Window creation error: " << SDL_GetError() << std::endl; SDL_Quit(); return -1; }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext){ std::cerr << "OpenGL Context error: " << SDL_GetError() << std::endl; SDL_DestroyWindow(window);
                     SDL_Quit(); return -1; }

    SDL_GL_SetSwapInterval(VSYNC);

    //todo: inicializar o Glad aqui
}