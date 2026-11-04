#include "engine/graphics/Model.h"
#include <algorithm>
#include <limits>

namespace engine::graphics {
Model::Model(const std::filesystem::path& path, float targetHeight) : Model(assets::loadGltf(path, targetHeight)) {}
Model::Model(const assets::ModelData& data) {
    if (!data.nodes.empty()) {
        animationData_ = std::make_unique<assets::ModelData>(data);
        bindPose_ = std::make_unique<assets::ModelPose>(*animationData_);
    }
    const float infinity = std::numeric_limits<float>::infinity();
    math::Vec3 minimum{infinity, infinity, infinity}, maximum{-infinity, -infinity, -infinity};
    primitives_.reserve(data.primitives.size());
    for (const auto& source : data.primitives) {
        for (std::size_t i = 0; i + 2 < source.vertices.size(); i += assets::kModelVertexStride) {
            minimum.x = std::min(minimum.x, source.vertices[i]);
            minimum.y = std::min(minimum.y, source.vertices[i + 1]);
            minimum.z = std::min(minimum.z, source.vertices[i + 2]);
            maximum.x = std::max(maximum.x, source.vertices[i]);
            maximum.y = std::max(maximum.y, source.vertices[i + 1]);
            maximum.z = std::max(maximum.z, source.vertices[i + 2]);
        }
        Primitive primitive;
        if (source.skin >= 0) {
            std::vector<float> skinned;
            skinned.reserve(source.influences.size() * 16);
            for (std::size_t i = 0; i < source.influences.size(); ++i) {
                const auto start = source.vertices.begin() + i * assets::kModelVertexStride;
                skinned.insert(skinned.end(), start, start + assets::kModelVertexStride);
                for (auto joint : source.influences[i].joints)
                    skinned.push_back(static_cast<float>(joint));
                for (auto weight : source.influences[i].weights)
                    skinned.push_back(weight);
            }
            primitive.mesh =
                Mesh(skinned, 16, {{0, 3, 0}, {1, 3, 3}, {2, 2, 6}, {3, 4, 8}, {4, 4, 12}}, source.indices);
        } else {
            primitive.mesh =
                Mesh(source.vertices, assets::kModelVertexStride, {{0, 3, 0}, {1, 3, 3}, {2, 2, 6}}, source.indices);
        }
        primitive.material.baseColor = source.material.baseColor;
        primitive.material.surfaceMaterial.roughness = source.material.roughness;
        primitive.material.surfaceMaterial.metallic = source.material.metallic;
        primitive.material.normalScale = source.material.normalScale;
        primitive.material.occlusionStrength = source.material.occlusionStrength;
        primitive.material.packedReflectivity = source.material.packedReflectivity;
        primitive.material.alphaCutoff = source.material.alphaCutoff;
        primitive.material.doubleSided = source.material.doubleSided;
        for (std::size_t i = 0; i < primitive.material.emissiveFactor.size(); ++i) {
            primitive.material.emissiveFactor[i] = source.material.emissiveFactor[i] * source.material.emissiveStrength;
        }
        if (!source.material.emissiveTexture.empty()) {
            primitive.material.emissive = Texture(source.material.emissiveTexture,
                                                  {TextureFilter::Mipmapped, source.material.baseTextureRepeat, false});
        }
        if (!source.material.baseTexture.empty()) {
            const auto filter = source.material.baseTextureNearest ? TextureFilter::Nearest : TextureFilter::Linear;
            primitive.material.texture =
                Texture(source.material.baseTexture, {filter, source.material.baseTextureRepeat, false, false,
                                                       source.material.baseTextureOpaqueBorderColor});
        }
        if (!source.material.metallicRoughnessTexture.empty()) {
            // Generated companions share the base-color UVs, including atlas coordinates outside [0, 1].
            primitive.material.properties = Texture(
                source.material.metallicRoughnessTexture,
                {TextureFilter::Trilinear, source.material.packedReflectivity && source.material.baseTextureRepeat});
        }
        if (!source.material.normalTexture.empty()) {
            primitive.material.normal = Texture(
                source.material.normalTexture,
                {TextureFilter::Trilinear, source.material.packedReflectivity && source.material.baseTextureRepeat});
        }
        primitive.material.packedOcclusion =
            !source.material.occlusionTexture.empty() &&
            source.material.occlusionTexture == source.material.metallicRoughnessTexture;
        if (!source.material.occlusionTexture.empty() && !primitive.material.packedOcclusion) {
            primitive.material.occlusion = Texture(source.material.occlusionTexture, {TextureFilter::Trilinear});
        }
        primitives_.push_back(std::move(primitive));
    }
    if (minimum.x <= maximum.x) {
        boundsCenter_ = {(minimum.x + maximum.x) * .5f, (minimum.y + maximum.y) * .5f, (minimum.z + maximum.z) * .5f};
        boundsHalfExtent_ = {(maximum.x - minimum.x) * .5f, (maximum.y - minimum.y) * .5f,
                             (maximum.z - minimum.z) * .5f};
    }
    if (bindPose_) {
        boundsCenter_ = bindPose_->boundsCenter();
        boundsHalfExtent_ = bindPose_->boundsHalfExtent();
    }
}
} // namespace engine::graphics
