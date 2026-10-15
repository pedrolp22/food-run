#pragma once

#include "engine/math/Math.h"
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace engine::assets {
inline constexpr int kModelVertexStride = 8;       // Position xyz, normal xyz, UV xy.
inline constexpr std::size_t kMaxSkinJoints = 128; // Fits the minimum OpenGL uniform-block size.
struct SkinInfluence {
    std::array<unsigned int, 4> joints{};
    std::array<float, 4> weights{};
};
struct RigNode {
    std::string name;
    int parent = -1;
    math::Mat4 localBind = math::identity();
    math::Mat4 worldBind = math::identity();
};
struct SkinData {
    std::vector<int> joints;
    std::vector<math::Mat4> inverseBindMatrices;
};
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
    std::array<float, 3> emissiveFactor = {0, 0, 0}; // Linear glTF emission, independent of illumination.
    float emissiveStrength = 1;
    std::filesystem::path emissiveTexture;
};
struct PrimitiveData {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    MaterialData material;
    int node = -1;
    int skin = -1;
    std::vector<SkinInfluence> influences;
};
struct ModelData {
    std::vector<PrimitiveData> primitives;
    std::vector<RigNode> nodes;
    std::vector<int> nodeOrder; // Parents before children, independent of glTF node indices.
    std::vector<SkinData> skins;
    math::Mat4 normalization = math::identity();
    math::Mat4 denormalization = math::identity();
};
// CPU-only import. Supports external-buffer glTF triangles, four-weight skins and UV0 material maps.
// Centers X/Z and grounds Y, preserving proportions at the requested height. Throws on invalid input.
ModelData loadGltf(const std::filesystem::path& path, float targetHeight = 1.0f);
} // namespace engine::assets
