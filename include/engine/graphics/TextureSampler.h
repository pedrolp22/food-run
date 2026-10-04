#pragma once

namespace engine::graphics {
// A move-only nearest sampling override, optionally selecting texture mip levels. Filtering is unchanged
// when this sampler is unbound. Destroy while its OpenGL context is current.
class TextureSampler {
  public:
    explicit TextureSampler(bool repeat = false, bool mipmapped = false);
    ~TextureSampler();
    TextureSampler(TextureSampler&& other) noexcept;
    TextureSampler& operator=(TextureSampler&& other) noexcept;
    TextureSampler(const TextureSampler&) = delete;
    TextureSampler& operator=(const TextureSampler&) = delete;
    unsigned int id() const { return id_; }

  private:
    unsigned int id_ = 0;
};
} // namespace engine::graphics
