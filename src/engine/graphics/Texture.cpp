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
bool recoverOpaqueMatte(MipPixels& mip, const std::array<std::uint8_t, 3>& color) {
    const std::size_t count = static_cast<std::size_t>(mip.width) * mip.height;
    for (std::size_t pixel = 0; pixel < count; ++pixel) {
        if (mip.rgba[pixel * 4 + 3] != 255)
            return false;
    }
    std::vector<std::size_t> pending;
    const auto visit = [&](int x, int y) {
        if (x < 0 || x >= mip.width || y < 0 || y >= mip.height)
            return;
        const std::size_t pixel = static_cast<std::size_t>(y) * mip.width + x;
        auto* rgba = mip.rgba.data() + pixel * 4;
        if (rgba[3] == 255 && rgba[0] == color[0] && rgba[1] == color[1] && rgba[2] == color[2]) {
            rgba[3] = 0;
            pending.push_back(pixel);
        }
    };
    for (int x = 0; x < mip.width; ++x) {
        visit(x, 0);
        visit(x, mip.height - 1);
    }
    for (int y = 0; y < mip.height; ++y) {
        visit(0, y);
        visit(mip.width - 1, y);
    }
    for (std::size_t head = 0; head < pending.size(); ++head) {
        const int x = static_cast<int>(pending[head] % mip.width);
        const int y = static_cast<int>(pending[head] / mip.width);
        visit(x - 1, y);
        visit(x + 1, y);
        visit(x, y - 1);
        visit(x, y + 1);
    }
    return !pending.empty();
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
    bool cooked = loadCooked(path, mipmapped, levels);
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
    if (options.opaqueBorderColor && recoverOpaqueMatte(levels.front(), *options.opaqueBorderColor)) {
        levels.resize(1); // Rebuild any cached mips from the recovered alpha, rather than the opaque export.
        cooked = false;
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
