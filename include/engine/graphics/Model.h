#pragma once

#include "engine/assets/GltfLoader.h"
#include "engine/graphics/Material.h"
#include "engine/graphics/Mesh.h"
#include "engine/graphics/Texture.h"
#include "engine/math/Math.h"

namespace engine::graphics {
class Model {
  public:
    struct Material {
        std::array<float, 4> baseColor = {1, 1, 1, 1};
        Texture texture;
        SurfaceMaterial surfaceMaterial{};
        Texture properties;
        Texture normal;
        Texture occlusion;
        float normalScale = 1;
        float occlusionStrength = 1;
        bool packedOcclusion = false;
        bool packedReflectivity = false;
        float alphaCutoff = 0;
        bool doubleSided = false;
        std::array<float, 3> emissiveFactor = {0, 0, 0}; // Includes the imported emissive-strength multiplier.
        Texture emissive;
        MaterialTextures textures() const {
            return {.properties = properties.id() ? &properties : nullptr,
                    .normal = normal.id() ? &normal : nullptr,
                    .occlusion = occlusion.id()    ? &occlusion
                                 : packedOcclusion ? &properties
                                                   : nullptr,
                    .normalScale = normalScale,
                    .occlusionStrength = occlusionStrength,
                    .packedReflectivity = packedReflectivity};
        }
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
    math::Vec3 boundsCenter() const { return boundsCenter_; }
    math::Vec3 boundsHalfExtent() const { return boundsHalfExtent_; }

  private:
    std::vector<Primitive> primitives_;
    math::Vec3 boundsCenter_{};
    math::Vec3 boundsHalfExtent_{};
};
} // namespace engine::graphics
