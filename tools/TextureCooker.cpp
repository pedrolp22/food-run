#include "engine/graphics/CookedTexture.h"
#include <algorithm>
#include <bit>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stb_image.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using engine::graphics::detail::CookedTextureHeader;
struct Mip {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba;
};

std::vector<std::uint8_t> downsample(const Mip& source, std::uint32_t width, std::uint32_t height) {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 4);
    for (std::uint32_t y = 0; y < height; ++y) {
        const std::uint32_t top = y * source.height / height;
        const std::uint32_t bottom = std::max(top + 1, (y + 1) * source.height / height);
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint32_t left = x * source.width / width;
            const std::uint32_t right = std::max(left + 1, (x + 1) * source.width / width);
            std::uint32_t sums[4]{};
            std::uint32_t samples = 0;
            for (std::uint32_t sy = top; sy < std::min(bottom, source.height); ++sy) {
                for (std::uint32_t sx = left; sx < std::min(right, source.width); ++sx) {
                    const std::size_t offset = (static_cast<std::size_t>(sy) * source.width + sx) * 4;
                    for (int channel = 0; channel < 4; ++channel) {
                        sums[channel] += source.rgba[offset + channel];
                    }
                    ++samples;
                }
            }
            const std::size_t output = (static_cast<std::size_t>(y) * width + x) * 4;
            for (int channel = 0; channel < 4; ++channel) {
                pixels[output + channel] = static_cast<std::uint8_t>((sums[channel] + samples / 2) / samples);
            }
        }
    }
    return pixels;
}

void cookImage(const std::filesystem::path& source) {
    int width = 0, height = 0, channels = 0;
    using Pixels = std::unique_ptr<unsigned char, decltype(&stbi_image_free)>;
    Pixels decoded(stbi_load(source.string().c_str(), &width, &height, &channels, 4), stbi_image_free);
    if (!decoded || width <= 0 || height <= 0) {
        throw std::runtime_error("Could not decode " + source.string() + ": " + stbi_failure_reason());
    }

    std::vector<Mip> mips;
    Mip base{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), {}};
    const std::size_t baseBytes = static_cast<std::size_t>(width) * height * 4;
    base.rgba.assign(decoded.get(), decoded.get() + baseBytes);
    mips.push_back(std::move(base));
    while (mips.back().width > 1 || mips.back().height > 1) {
        const auto nextWidth = std::max(1u, mips.back().width / 2);
        const auto nextHeight = std::max(1u, mips.back().height / 2);
        mips.push_back({nextWidth, nextHeight, downsample(mips.back(), nextWidth, nextHeight)});
    }

    std::error_code error;
    const auto sourceSize = std::filesystem::file_size(source, error);
    if (error) {
        throw std::runtime_error("Could not stat " + source.string() + ": " + error.message());
    }
    const auto timestamp = std::filesystem::last_write_time(source, error);
    if (error) {
        throw std::runtime_error("Could not read timestamp for " + source.string() + ": " + error.message());
    }
    CookedTextureHeader header;
    header.width = static_cast<std::uint32_t>(width);
    header.height = static_cast<std::uint32_t>(height);
    header.mipLevels = static_cast<std::uint32_t>(mips.size());
    header.sourceSize = sourceSize;
    header.sourceTimestamp = timestamp.time_since_epoch().count();

    const auto destination = source.parent_path() / ".cooked" / (source.filename().string() + ".txc");
    std::filesystem::create_directories(destination.parent_path());
    auto temporary = destination;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output || !output.write(reinterpret_cast<const char*>(&header), sizeof(header))) {
            throw std::runtime_error("Could not write cooked texture header for " + source.string());
        }
        for (const auto& mip : mips) {
            output.write(reinterpret_cast<const char*>(mip.rgba.data()),
                         static_cast<std::streamsize>(mip.rgba.size()));
        }
        if (!output) {
            throw std::runtime_error("Could not write cooked mip chain for " + source.string());
        }
    }
    std::filesystem::remove(destination, error);
    std::filesystem::rename(temporary, destination);
}

bool imagePath(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" ||
           extension == ".tga";
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            throw std::invalid_argument("Usage: subway_texture_cooker <asset-root>");
        }
        const std::filesystem::path root = argv[1];
        if (!std::filesystem::is_directory(root)) {
            throw std::invalid_argument("Asset root is not a directory: " + root.string());
        }
        std::size_t count = 0;
        for (std::filesystem::recursive_directory_iterator it(root), end; it != end; ++it) {
            if (it->is_directory() && it->path().filename() == ".cooked") {
                it.disable_recursion_pending();
                continue;
            }
            if (!it->is_regular_file() || !imagePath(it->path())) {
                continue;
            }
            cookImage(it->path());
            ++count;
        }
        std::cout << "Cooked " << count << " texture assets under " << root << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Texture cooking failed: " << error.what() << '\n';
        return 1;
    }
}
