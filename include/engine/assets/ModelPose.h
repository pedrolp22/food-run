#pragma once

#include "engine/assets/GltfLoader.h"
#include <cstdint>
#include <span>

namespace engine::assets {
// A CPU-only pose borrowing immutable import data. Separate instances can animate the same model independently.
// Set local joint transforms, then evaluate once before submitting the scene to any render pass.
class ModelPose {
  public:
    explicit ModelPose(const ModelData& data);
    const ModelData& data() const { return data_; }
    void reset();
    void setLocal(int node, const math::Mat4& transform);
    const math::Mat4& local(int node) const { return locals_.at(node); }
    void evaluate();
    const math::Mat4& world(int node) const { return worlds_.at(node); }
    const math::Mat4& primitiveTransform(std::size_t primitive) const { return primitiveTransforms_.at(primitive); }
    std::span<const math::Mat4> jointMatrices(std::size_t primitive) const { return palettes_.at(primitive); }
    math::Vec3 boundsCenter() const { return boundsCenter_; }
    math::Vec3 boundsHalfExtent() const { return boundsHalfExtent_; }
    std::uint64_t revision() const { return revision_; }

  private:
    const ModelData& data_;
    std::vector<math::Mat4> locals_, worlds_, meshBindInverse_;
    std::vector<math::Mat4> primitiveTransforms_;
    std::vector<std::vector<math::Mat4>> palettes_;
    math::Vec3 boundsCenter_{}, boundsHalfExtent_{};
    std::uint64_t revision_ = 0;
};
} // namespace engine::assets
