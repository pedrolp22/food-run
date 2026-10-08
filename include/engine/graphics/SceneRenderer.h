#pragma once

#include "engine/graphics/RenderMode.h"
#include "engine/graphics/RenderSettings.h"
#include "engine/graphics/Scene.h"
#include <memory>

namespace engine::graphics {
// Requires a current OpenGL 3.3 context. Does not own a window or swap buffers.
class SceneRenderer {
  public:
    explicit SceneRenderer(RenderSettings settings = {});
    ~SceneRenderer();
    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;
    void render(const Scene& scene, Size drawableSize);
    void setMode(RenderMode mode);
    RenderMode mode() const;
    // Compatibility: these select/query the PSX style only.
    void setRetroMode(bool enabled);
    bool retroMode() const;
    void setPsxDitheredShadows(bool enabled);
    bool psxDitheredShadows() const; // Stored preference; applied only while PSX mode is active.
    void setPsxDistanceFog(bool enabled);
    bool psxDistanceFog() const; // Preserved across mode switches, independently of color/shadow dithering.

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace engine::graphics
