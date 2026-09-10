#pragma once

#include "engine/graphics/Types.h"
#include <filesystem>

namespace engine::graphics {
enum class TextureFilter { Nearest, Linear, Mipmapped };
struct TextureOptions {
    TextureFilter filter = TextureFilter::Mipmapped;
    bool repeat = false;
    bool flipVertically = false;
};
class Texture {
  public:
    Texture() = default;
    explicit Texture(const std::filesystem::path& path, TextureOptions options = {});
    ~Texture();
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    unsigned int id() const { return id_; }
    Size size() const { return size_; }
    bool repeats() const { return repeat_; }
    float aspect() const { return size_.height > 0 ? static_cast<float>(size_.width) / size_.height : 1.0f; }

  private:
    unsigned int id_ = 0;
    Size size_;
    bool repeat_ = false;
};
} // namespace engine::graphics
