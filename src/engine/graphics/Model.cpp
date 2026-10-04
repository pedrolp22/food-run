#include "engine/graphics/Model.h"

namespace engine::graphics {
Model::Model(const std::filesystem::path& path, float targetHeight) : Model(assets::loadGltf(path, targetHeight)) {}
Model::Model(const assets::ModelData& data) {
    primitives_.reserve(data.primitives.size());
    for (const auto& source : data.primitives) {
        Primitive primitive;
        primitive.mesh =
            Mesh(source.vertices, assets::kModelVertexStride, {{0, 3, 0}, {1, 3, 3}, {2, 2, 6}}, source.indices);
        primitive.material.baseColor = source.material.baseColor;
        primitive.material.surfaceMaterial.roughness = source.material.roughness;
        primitive.material.surfaceMaterial.metallic = source.material.metallic;
        primitive.material.normalScale = source.material.normalScale;
        primitive.material.occlusionStrength = source.material.occlusionStrength;
        primitive.material.packedReflectivity = source.material.packedReflectivity;
        if (!source.material.baseTexture.empty()) {
            primitive.material.texture = Texture(source.material.baseTexture, {TextureFilter::Linear, false, false});
        }
        if (!source.material.metallicRoughnessTexture.empty()) {
            primitive.material.properties =
                Texture(source.material.metallicRoughnessTexture, {TextureFilter::Trilinear});
        }
        if (!source.material.normalTexture.empty()) {
            primitive.material.normal = Texture(source.material.normalTexture, {TextureFilter::Trilinear});
        }
        primitive.material.packedOcclusion =
            !source.material.occlusionTexture.empty() &&
            source.material.occlusionTexture == source.material.metallicRoughnessTexture;
        if (!source.material.occlusionTexture.empty() && !primitive.material.packedOcclusion) {
            primitive.material.occlusion = Texture(source.material.occlusionTexture, {TextureFilter::Trilinear});
        }
        primitives_.push_back(std::move(primitive));
    }
}
} // namespace engine::graphics
