#pragma once

#include "engine/assets/GltfLoader.h"
#include "engine/graphics/Mesh.h"
#include "engine/graphics/Texture.h"

namespace engine::graphics {
class Model {
  public:
    struct Material {
        std::array<float, 4> baseColor = {1, 1, 1, 1};
        Texture texture;
    };
    struct Primitive {
        Mesh mesh;
        Material material;
    };
    Model() = default;
    explicit Model(const assets::ModelData& data);
    explicit Model(const std::filesystem::path& path, float targetHeight = 1.0f);
    Model(Model&&) noexcept = default;
    Model& operator=(Model&&) noexcept = default;
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    const std::vector<Primitive>& primitives() const { return primitives_; }

  private:
    std::vector<Primitive> primitives_;
};
} // namespace engine::graphics
