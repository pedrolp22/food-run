#include "Skinning.h"
#include <glad/glad.h>
#include <stdexcept>

namespace engine::graphics::detail {
SkinningUniforms::SkinningUniforms(const ShaderProgram& shader) : enabled(shader.uniform("uSkinned")) {
    const auto block = glGetUniformBlockIndex(shader.id(), "SkinMatrices");
    if (block == GL_INVALID_INDEX)
        throw std::runtime_error("Missing skinning uniform block");
    glUniformBlockBinding(shader.id(), block, 0);
}
SkinningBuffer::~SkinningBuffer() {
    if (buffer_)
        glDeleteBuffers(1, &buffer_);
}
void SkinningBuffer::beginFrame() {
    pose_ = nullptr;
    if (!buffer_) {
        glGenBuffers(1, &buffer_);
        glBindBuffer(GL_UNIFORM_BUFFER, buffer_);
        glBufferData(GL_UNIFORM_BUFFER, assets::kMaxSkinJoints * sizeof(math::Mat4), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
    // An active uniform block needs valid storage even for a draw with skinning disabled.
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, buffer_);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    bound_ = true;
}
void SkinningBuffer::bind(const assets::ModelPose& pose, std::size_t primitive) {
    const auto matrices = pose.jointMatrices(primitive);
    if (matrices.empty() || matrices.size() > assets::kMaxSkinJoints)
        throw std::invalid_argument("Invalid skin palette");
    if (!bound_) {
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, buffer_);
        bound_ = true;
    }
    if (pose_ != &pose || primitive_ != primitive || revision_ != pose.revision()) {
        glBindBuffer(GL_UNIFORM_BUFFER, buffer_);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, matrices.size_bytes(), matrices.data());
        pose_ = &pose;
        primitive_ = primitive;
        revision_ = pose.revision();
    }
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}
void SkinningBuffer::finishFrame() {
    if (bound_)
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0);
    bound_ = false;
}
} // namespace engine::graphics::detail
