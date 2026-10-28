#include "engine/graphics/Texture.h"
#include "engine/graphics/CookedTexture.h"
#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <glad/glad.h>
#include <memory>
#include <stb_image.h>
#include <stdexcept>
#include <string>
#include <utility>

namespace engine::graphics {
namespace {
struct MipPixels {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

std::filesystem::path cookedPath(const std::filesystem::path& source) {
    return source.parent_path() / ".cooked" / (source.filename().string() + ".txc");
}

bool loadCooked(const std::filesystem::path& source, bool mipmapped, std::vector<MipPixels>& levels) {
    using detail::CookedTextureHeader;
    const auto cache = cookedPath(source);
    std::error_code error;
    const auto sourceSize = std::filesystem::file_size(source, error);
    if (error) {
        return false;
    }
    const auto timestamp = std::filesystem::last_write_time(source, error);
    if (error) {
        return false;
    }
    const auto cacheSize = std::filesystem::file_size(cache, error);
    if (error || cacheSize < sizeof(CookedTextureHeader)) {
        return false;
    }
    std::ifstream input(cache, std::ios::binary);
    CookedTextureHeader header;
    if (!input.read(reinterpret_cast<char*>(&header), sizeof(header)) || header.magic != detail::kCookedTextureMagic ||
        header.sourceSize != sourceSize || header.sourceTimestamp != timestamp.time_since_epoch().count() ||
        header.width == 0 || header.height == 0 || header.width > 32768 || header.height > 32768) {
        return false;
    }
    const auto fullLevelCount = static_cast<std::uint32_t>(std::bit_width(std::max(header.width, header.height)));
    if (header.mipLevels != fullLevelCount) {
        return false;
    }
    const int requestedLevels = mipmapped ? static_cast<int>(header.mipLevels) : 1;
    std::uint64_t expectedSize = sizeof(CookedTextureHeader);
    for (std::uint32_t level = 0, width = header.width, height = header.height; level < header.mipLevels; ++level) {
        expectedSize += static_cast<std::uint64_t>(width) * height * 4;
        width = std::max(1u, width / 2);
        height = std::max(1u, height / 2);
    }
    if (cacheSize != expectedSize) {
        return false;
    }

    levels.clear();
    levels.reserve(static_cast<std::size_t>(requestedLevels));
    for (int level = 0; level < requestedLevels; ++level) {
        const auto width = std::max(1u, header.width >> level);
        const auto height = std::max(1u, header.height >> level);
        MipPixels pixels{static_cast<int>(width), static_cast<int>(height), {}};
        pixels.rgba.resize(static_cast<std::size_t>(width) * height * 4);
        if (!input.read(reinterpret_cast<char*>(pixels.rgba.data()),
                        static_cast<std::streamsize>(pixels.rgba.size()))) {
            levels.clear();
            return false;
        }
        levels.push_back(std::move(pixels));
    }
    return true;
}

void flipVertically(MipPixels& mip) {
    const auto rowBytes = static_cast<std::size_t>(mip.width) * 4;
    for (int y = 0; y < mip.height / 2; ++y) {
        auto* row = mip.rgba.data() + static_cast<std::size_t>(y) * rowBytes;
        auto* opposite = mip.rgba.data() + static_cast<std::size_t>(mip.height - y - 1) * rowBytes;
        std::swap_ranges(row, row + rowBytes, opposite);
    }
}
} // namespace

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
    const bool mipmapped = options.filter == TextureFilter::Mipmapped || options.filter == TextureFilter::Trilinear;
    std::vector<MipPixels> levels;
    const bool cooked = loadCooked(path, mipmapped, levels);
    if (cooked) {
        size_ = {levels.front().width, levels.front().height};
    } else {
        int channels = 0;
        using Pixels = std::unique_ptr<unsigned char, decltype(&stbi_image_free)>;
        Pixels decoded(stbi_load(path.string().c_str(), &size_.width, &size_.height, &channels, 4), stbi_image_free);
        if (!decoded) {
            throw std::runtime_error("Could not load texture " + path.string() + ": " + stbi_failure_reason());
        }
        const auto byteCount = static_cast<std::size_t>(size_.width) * size_.height * 4;
        MipPixels base{size_.width, size_.height, {}};
        base.rgba.assign(decoded.get(), decoded.get() + byteCount);
        levels.push_back(std::move(base));
    }
    if (options.flipVertically) {
        for (auto& mip : levels) {
            flipVertically(mip);
        }
    }
    if (options.retainAlpha) {
        alpha_.resize(static_cast<std::size_t>(size_.width) * size_.height);
        for (std::size_t i = 0; i < alpha_.size(); ++i) {
            alpha_[i] = levels.front().rgba[i * 4 + 3];
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
    maxMipLevel_ = mipmapped ? std::bit_width(static_cast<unsigned int>(std::max(size_.width, size_.height))) - 1 : 0;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, maxMipLevel_);
    for (int level = 0; level < static_cast<int>(levels.size()); ++level) {
        const auto& mip = levels[level];
        glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA8, mip.width, mip.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     mip.rgba.data());
    }
    if (mipmapped && !cooked) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
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
