#include "engine/platform/Window.h"
#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <iostream>
#include <stdexcept>

namespace engine::platform {
namespace {
void requireSDL(bool success, const char* action) {
    if (!success) {
        throw std::runtime_error(std::string(action) + ": " + SDL_GetError());
    }
}
} // namespace
Window::Window(const WindowSettings& settings) {
    if (settings.size.width <= 0 || settings.size.height <= 0) {
        throw std::invalid_argument("Invalid window size");
    }
    try {
        SDL_SetMainReady();
        requireSDL(SDL_InitSubSystem(SDL_INIT_VIDEO) == 0, "Initialize SDL video");
        videoInitialized_ = true;
        requireSDL(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) == 0, "Set GL major version");
        requireSDL(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3) == 0, "Set GL minor version");
        requireSDL(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) == 0,
                   "Set GL profile");
        for (auto attribute : {SDL_GL_RED_SIZE, SDL_GL_GREEN_SIZE, SDL_GL_BLUE_SIZE}) {
            requireSDL(SDL_GL_SetAttribute(attribute, 8) == 0, "Set color buffer size");
        }
        requireSDL(SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24) == 0, "Set depth buffer size");
        requireSDL(SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) == 0, "Set double buffering");
        window_ = SDL_CreateWindow(settings.title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   settings.size.width, settings.size.height,
                                   SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                       (settings.visible ? SDL_WINDOW_SHOWN : SDL_WINDOW_HIDDEN));
        requireSDL(window_ != nullptr, "Create window");
        context_ = SDL_GL_CreateContext(window_);
        requireSDL(context_ != nullptr, "Create OpenGL context");
        makeCurrent();
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
            throw std::runtime_error("Could not load OpenGL functions");
        }
        if (SDL_GL_SetSwapInterval(settings.vsync ? 1 : 0) != 0) {
            std::cerr << "VSync setup warning: " << SDL_GetError() << '\n';
        }
    } catch (...) {
        release();
        throw;
    }
}
Window::~Window() {
    release();
}
void Window::release() {
    if (context_) {
        SDL_GL_DeleteContext(context_);
        context_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    if (videoInitialized_) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        videoInitialized_ = false;
    }
}
void Window::makeCurrent() const {
    requireSDL(SDL_GL_MakeCurrent(window_, context_) == 0, "Make GL context current");
}
void Window::swap() const {
    SDL_GL_SwapWindow(window_);
}
void Window::setTitle(const std::string& title) const {
    SDL_SetWindowTitle(window_, title.c_str());
}
void Window::setSize(graphics::Size size) const {
    if (size.width <= 0 || size.height <= 0) {
        throw std::invalid_argument("Invalid window size");
    }
    SDL_SetWindowSize(window_, size.width, size.height);
}
graphics::Size Window::drawableSize() const {
    graphics::Size size;
    SDL_GL_GetDrawableSize(window_, &size.width, &size.height);
    return size;
}
} // namespace engine::platform
