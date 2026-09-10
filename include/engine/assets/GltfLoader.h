#pragma once

#include <array>
#include <filesystem>
#include <vector>

namespace engine::assets {
inline constexpr int kModelVertexStride = 8; // Position xyz, normal xyz, UV xy.
struct MaterialData {
    std::array<float, 4> baseColor = {1, 1, 1, 1};
    std::filesystem::path baseTexture;
};
struct PrimitiveData {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    MaterialData material;
};
struct ModelData {
    std::vector<PrimitiveData> primitives;
};
// CPU-only import. Supports external-buffer glTF 2.0 triangle meshes with base-color materials.
// Centers X/Z and grounds Y, preserving proportions at the requested height. Throws on invalid input.
ModelData loadGltf(const std::filesystem::path& path, float targetHeight = 1.0f);
} // namespace engine::assets
