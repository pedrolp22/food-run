#pragma once

#include "engine/graphics/Types.h"
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace engine::graphics {
enum class TextureFilter { Nearest, Linear, Mipmapped, Trilinear };
struct TextureOptions {
    TextureFilter filter = TextureFilter::Mipmapped;
    bool repeat = false;
    bool flipVertically = false;
    bool retainAlpha = false; // Opt in for silhouette geometry; ordinary textures keep no CPU pixels.
};
class Texture {
  public:
    Texture() = default;
    explicit Texture(const std::filesystem::path& path, TextureOptions options = {});
    // Linear, clamped floating-point data texture (no mipmaps), e.g. for distance fields.
    static Texture fromRedChannel(Size size, std::span<const float> pixels);
    ~Texture();
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    unsigned int id() const { return id_; }
    Size size() const { return size_; }
    bool repeats() const { return repeat_; }
    int maxMipLevel() const { return maxMipLevel_; }
    std::span<const std::uint8_t> alphaMask() const { return alpha_; }
    float aspect() const { return size_.height > 0 ? static_cast<float>(size_.width) / size_.height : 1.0f; }

  private:
    unsigned int id_ = 0;
    Size size_;
    bool repeat_ = false;
    int maxMipLevel_ = 0;
    std::vector<std::uint8_t> alpha_;
};
} // namespace engine::graphics
