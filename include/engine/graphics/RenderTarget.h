#pragma once

#include "engine/graphics/Types.h"

namespace engine::graphics {
class RenderTarget {
  public:
    // ColorDepthAlpha retains alpha for presentation masks; Reflection uses filtered, mipmapped sRGB color.
    enum class Kind { ColorDepth, ColorDepthAlpha, Shadow, Reflection };
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
    void generateMipmaps() const; // Finish a reflection capture before sampling its filtered mip chain.
    int maxMipLevel() const;
    static void bindDefault(Size size);
    unsigned int texture() const { return texture_; }
    unsigned int depthTexture() const { return depthTexture_; }
    Size size() const { return size_; }
    int samples() const { return samples_; }

  private:
    void release();
    unsigned int fbo_ = 0;
    unsigned int texture_ = 0;
    unsigned int depth_ = 0;
    unsigned int depthTexture_ = 0;
    unsigned int resolveFbo_ = 0;
    unsigned int colorBuffer_ = 0;
    int samples_ = 1;
    Size size_;
};
} // namespace engine::graphics
