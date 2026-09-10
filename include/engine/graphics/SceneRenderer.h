#pragma once

#include "engine/graphics/RenderMode.h"
#include "engine/graphics/Scene.h"
#include <memory>

namespace engine::graphics {
struct RenderSettings {
    Size retroSize = {240, 320}; // PSX raster.
    int shadowMapSize = 2048;
    float retroPixelStep = 0.5f;   // Half-pixel steps keep polygon movement subtle; zero disables snapping.
    float modelAffineStrength = 0; // PSX only, [0, 1]: optional affine warping, disabled by default.
    int n64Samples = 4;            // MSAA coverage samples, capped to driver support; 1 disables edge AA.
    Size n64Size = retroSize;      // Match the PSX raster by default.
};
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

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace engine::graphics
