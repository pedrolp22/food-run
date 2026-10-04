#include "engine/graphics/Texture.h"
#include <algorithm>
#include <bit>
#include <glad/glad.h>
#include <memory>
#include <stb_image.h>
#include <stdexcept>
#include <string>
#include <utility>

namespace engine::graphics {
Texture Texture::fromRedChannel(Size size, std::span<const float> pixels) {
    if (size.width <= 0 || size.height <= 0 ||
        pixels.size() / static_cast<std::size_t>(size.width) != static_cast<std::size_t>(size.height) ||
        pixels.size() % static_cast<std::size_t>(size.width) != 0) {
        throw std::invalid_argument("Invalid data texture dimensions");
    }
    Texture result;
    result.size_ = size;
    glGenTextures(1, &result.id_);
    if (!result.id_) {
        throw std::runtime_error("Could not allocate data texture");
    }
    glBindTexture(GL_TEXTURE_2D, result.id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, size.width, size.height, 0, GL_RED, GL_FLOAT, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return result;
}
Texture::Texture(const std::filesystem::path& path, TextureOptions options) : repeat_(options.repeat) {
    int channels = 0;
    using Pixels = std::unique_ptr<unsigned char, decltype(&stbi_image_free)>;
    Pixels pixels(stbi_load(path.string().c_str(), &size_.width, &size_.height, &channels, 4), stbi_image_free);
    if (!pixels) {
        throw std::runtime_error("Could not load texture " + path.string() + ": " + stbi_failure_reason());
    }
    // Flip locally; loading one asset must not change stb's global orientation for later assets.
    if (options.flipVertically) {
        const auto rowBytes = static_cast<std::size_t>(size_.width) * 4;
        for (int y = 0; y < size_.height / 2; ++y) {
            auto* row = pixels.get() + y * rowBytes;
            auto* opposite = pixels.get() + (size_.height - y - 1) * rowBytes;
            std::swap_ranges(row, row + rowBytes, opposite);
        }
    }
    if (options.retainAlpha) {
        alpha_.resize(static_cast<std::size_t>(size_.width) * size_.height);
        for (std::size_t i = 0; i < alpha_.size(); ++i) {
            alpha_[i] = pixels.get()[i * 4 + 3];
        }
    }
    glGenTextures(1, &id_);
    if (!id_) {
        throw std::runtime_error("Could not allocate texture");
    }
    glBindTexture(GL_TEXTURE_2D, id_);
    const int wrap = options.repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    const int minFilter = options.filter == TextureFilter::Trilinear ? GL_LINEAR_MIPMAP_LINEAR
                          : options.filter == TextureFilter::Mipmapped ? GL_LINEAR_MIPMAP_NEAREST
                          : options.filter == TextureFilter::Linear  ? GL_LINEAR
                                                                     : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    options.filter == TextureFilter::Linear || options.filter == TextureFilter::Trilinear
                        ? GL_LINEAR : GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size_.width, size_.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());
    glGenerateMipmap(GL_TEXTURE_2D);
    maxMipLevel_ = std::bit_width(static_cast<unsigned int>(std::max(size_.width, size_.height))) - 1;
    glBindTexture(GL_TEXTURE_2D, 0);
}
Texture::~Texture() {
    if (id_) {
        glDeleteTextures(1, &id_);
    }
}
Texture::Texture(Texture&& other) noexcept
    : id_(std::exchange(other.id_, 0)), size_(std::exchange(other.size_, {})),
      repeat_(std::exchange(other.repeat_, false)), maxMipLevel_(std::exchange(other.maxMipLevel_, 0)),
      alpha_(std::move(other.alpha_)) {}
Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        if (id_) {
            glDeleteTextures(1, &id_);
        }
        id_ = std::exchange(other.id_, 0);
        size_ = std::exchange(other.size_, {});
        repeat_ = std::exchange(other.repeat_, false);
        maxMipLevel_ = std::exchange(other.maxMipLevel_, 0);
        alpha_ = std::move(other.alpha_);
    }
    return *this;
}
} // namespace engine::graphics
