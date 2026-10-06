#pragma once

#include <array>
#include <filesystem>
#include <vector>

namespace engine::assets {
inline constexpr int kModelVertexStride = 8; // Position xyz, normal xyz, UV xy.
struct MaterialData {
    std::array<float, 4> baseColor = {1, 1, 1, 1}; // Linear glTF baseColorFactor, not sRGB.
    std::filesystem::path baseTexture;
    float roughness = 1; // glTF perceptual roughnessFactor, in linear space.
    float metallic = 1;  // glTF metallicFactor defaults to 1, including when omitted.
    std::filesystem::path metallicRoughnessTexture;
    std::filesystem::path normalTexture;
    std::filesystem::path occlusionTexture;
    float normalScale = 1;
    float occlusionStrength = 1;
    bool packedReflectivity = false; // Engine surface-map companions may use A; glTF imports leave it disabled.
    float alphaCutoff = 0;           // glTF MASK cutoff; zero keeps the default opaque material.
    bool doubleSided = false;
    bool baseTextureNearest = false; // Preserve authored nearest sampling for pixel-art cutouts.
    bool baseTextureRepeat = false;
};
struct PrimitiveData {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    MaterialData material;
};
struct ModelData {
    std::vector<PrimitiveData> primitives;
};
// CPU-only import. Supports external-buffer glTF triangles and UV0 base-color, material and normal maps.
// Centers X/Z and grounds Y, preserving proportions at the requested height. Throws on invalid input.
ModelData loadGltf(const std::filesystem::path& path, float targetHeight = 1.0f);
} // namespace engine::assets
