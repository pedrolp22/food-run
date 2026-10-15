#pragma once

#include "engine/assets/ModelPose.h"
#include "engine/graphics/ShaderProgram.h"

namespace engine::graphics::detail {
struct SkinningUniforms {
    int enabled;
    explicit SkinningUniforms(const ShaderProgram& shader);
};
// One reusable uniform buffer; reuse its current palette across views until the pose, primitive or revision changes.
class SkinningBuffer {
  public:
    SkinningBuffer() = default;
    ~SkinningBuffer();
    SkinningBuffer(const SkinningBuffer&) = delete;
    SkinningBuffer& operator=(const SkinningBuffer&) = delete;
    void beginFrame();
    void bind(const assets::ModelPose& pose, std::size_t primitive);
    void finishFrame();

  private:
    unsigned int buffer_ = 0;
    const assets::ModelPose* pose_ = nullptr;
    std::size_t primitive_ = 0;
    std::uint64_t revision_ = 0;
    bool bound_ = false;
};
} // namespace engine::graphics::detail
