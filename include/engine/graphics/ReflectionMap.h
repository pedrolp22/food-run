#pragma once

namespace engine::graphics {
// A live world cubemap with a shared depth buffer and a roughness mip chain.
// Requires a current OpenGL context for its entire lifetime.
class ReflectionMap {
  public:
    explicit ReflectionMap(int faceSize);
    ~ReflectionMap();
    ReflectionMap(const ReflectionMap&) = delete;
    ReflectionMap& operator=(const ReflectionMap&) = delete;
    void bindFace(int face) const; // OpenGL order: +X, -X, +Y, -Y, +Z, -Z.
    void generateMipmaps() const;
    unsigned int texture() const { return texture_; }
    int faceSize() const { return faceSize_; }
    int maxMipLevel() const { return maxMipLevel_; }

  private:
    void release();
    unsigned int fbo_ = 0;
    unsigned int texture_ = 0;
    unsigned int depth_ = 0;
    int faceSize_ = 0;
    int maxMipLevel_ = 0;
};
} // namespace engine::graphics
