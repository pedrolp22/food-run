#pragma once

#include "engine/graphics/Types.h"
#include <string>

struct SDL_Window;
namespace engine::platform {
struct WindowSettings {
    std::string title = "Application";
    graphics::Size size = {1280, 720};
    bool visible = true;
    bool vsync = true;
};
// Owns one SDL video reference and its GL context. Keep it alive until all GPU resources are destroyed.
class Window {
  public:
    explicit Window(const WindowSettings& settings = {});
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    void makeCurrent() const;
    void swap() const;
    void setTitle(const std::string& title) const;
    void setSize(graphics::Size size) const;
    bool setVsync(bool enabled) const;
    bool vsync() const;
    graphics::Size drawableSize() const;
    SDL_Window* nativeHandle() const { return window_; }

  private:
    void release();
    SDL_Window* window_ = nullptr;
    void* context_ = nullptr;
    bool videoInitialized_ = false;
};
} // namespace engine::platform
