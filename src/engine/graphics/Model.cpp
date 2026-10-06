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
        primitive.material.alphaCutoff = source.material.alphaCutoff;
        primitive.material.doubleSided = source.material.doubleSided;
        if (!source.material.baseTexture.empty()) {
            const auto filter = source.material.baseTextureNearest ? TextureFilter::Nearest : TextureFilter::Linear;
            primitive.material.texture =
                Texture(source.material.baseTexture, {filter, source.material.baseTextureRepeat, false});
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
}
} // namespace engine::graphics
