#pragma once

#include "engine/graphics/Types.h"

namespace engine::graphics {
class RenderTarget {
  public:
    enum class Kind { ColorDepth, Shadow };
    RenderTarget() = default;
    RenderTarget(Size size, Kind kind, int samples = 1);
    ~RenderTarget();
    RenderTarget(RenderTarget&& other) noexcept;
    RenderTarget& operator=(RenderTarget&& other) noexcept;
    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;
    void bind() const;
    // Resolve MSAA color for sampling/readback, leaving the resolved framebuffer bound.
    void resolve() const;
    static void bindDefault(Size size);
    unsigned int texture() const { return texture_; }
    Size size() const { return size_; }
    int samples() const { return samples_; }

  private:
    void release();
    unsigned int fbo_ = 0;
    unsigned int texture_ = 0;
    unsigned int depth_ = 0;
    unsigned int resolveFbo_ = 0;
    unsigned int colorBuffer_ = 0;
    int samples_ = 1;
    Size size_;
};
} // namespace engine::graphics
