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
        if (!source.material.baseTexture.empty()) {
            primitive.material.texture = Texture(source.material.baseTexture, {TextureFilter::Linear, false, false});
        }
        primitives_.push_back(std::move(primitive));
    }
}
} // namespace engine::graphics
