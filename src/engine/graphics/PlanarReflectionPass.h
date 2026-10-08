#pragma once

#include "engine/graphics/RenderMode.h"
#include "engine/graphics/RenderSettings.h"
#include "engine/graphics/Scene.h"
#include <array>
#include <memory>
#include <optional>
#include <span>

namespace engine::graphics {
class RenderTarget;
namespace detail {
class RenderState;
struct ScreenBounds {
    float left = 1, bottom = 1, right = -1, top = -1;
};
struct ReflectionFrame {
    math::Mat4 viewProjection;
    Size size;
    RenderMode mode;
};
struct PlanarReflectionView {
    Camera camera;
    math::Mat4 viewProjection;
    Size size;
    ScreenBounds skyBounds;
    float aspect = 1;
    std::array<float, 4> clipPlane{};
    RenderTarget* target = nullptr; // Owned by the pass; valid until the next update.
};
struct PlanarReflectionBinding {
    unsigned int texture = 0;
    math::Mat4 viewProjection;
    math::Vec2 textureScale = {1, 1};
    float maxLod = 0;
    float strength = 0;
};

// Owns facade grouping, visibility, refresh policy and persistent GPU targets.
// The caller draws returned views and generates their mipmaps. This component
// does not depend on SceneRenderer, programs, game layout or draw handlers.
class PlanarReflectionPass {
  public:
    explicit PlanarReflectionPass(const RenderSettings& settings);
    ~PlanarReflectionPass();
    PlanarReflectionPass(const PlanarReflectionPass&) = delete;
    PlanarReflectionPass& operator=(const PlanarReflectionPass&) = delete;
    std::span<const PlanarReflectionView> update(const Scene& scene, const ReflectionFrame& frame, RenderState& state);
    std::optional<PlanarReflectionBinding> binding(const Quad& surface, math::Vec3 cameraPosition) const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace detail
} // namespace engine::graphics
