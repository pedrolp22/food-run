#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <glad/glad.h>
#include <limits>
#include <vector>

namespace engine::graphics::detail {
// A frame owns GL state. Forget caller state at its start, then avoid redundant
// driver calls while retaining each shader's uniform values across view changes.
class RenderState {
  public:
    void reset() {
        activeUnit_ = unknown;
        program_ = nullptr;
        textures_.fill(unknown);
        cubemaps_.fill(unknown);
        samplers_.fill(unknown);
        for (auto& program : programs_) {
            program.uniforms.clear();
        }
    }
    void bindProgram(GLuint id) {
        if (program_ && program_->id == id) {
            return;
        }
        glUseProgram(id);
        auto found = std::find_if(programs_.begin(), programs_.end(), [id](const auto& p) { return p.id == id; });
        if (found == programs_.end()) {
            programs_.push_back({id, {}});
            program_ = &programs_.back();
        } else {
            program_ = &*found;
        }
    }
    void activeTexture(GLenum unit) {
        if (activeUnit_ != unit) {
            glActiveTexture(unit);
            activeUnit_ = unit;
        }
    }
    void bindTexture(GLenum target, GLuint id) {
        auto& bound = (target == GL_TEXTURE_2D ? textures_ : cubemaps_)[activeUnit_ - GL_TEXTURE0];
        if (bound != id) {
            glBindTexture(target, id);
            bound = id;
        }
    }
    void bindSampler(GLuint unit, GLuint id) {
        if (samplers_[unit] != id) {
            glBindSampler(unit, id);
            samplers_[unit] = id;
        }
    }
    // RenderTarget allocation and mip generation bind directly on the active unit.
    void forgetTextureBinding(GLenum target) {
        (target == GL_TEXTURE_2D ? textures_ : cubemaps_)[activeUnit_ - GL_TEXTURE0] = unknown;
    }
    void uniform1i(GLint location, GLint value) {
        if (changed(location, {static_cast<std::uint32_t>(value), 0, 0, 0})) {
            glUniform1i(location, value);
        }
    }
    void uniform1ui(GLint location, GLuint value) {
        if (changed(location, {value, 0, 0, 0})) {
            glUniform1ui(location, value);
        }
    }
    void uniform1f(GLint location, GLfloat x) {
        if (changed(location, {bits(x), 0, 0, 0})) {
            glUniform1f(location, x);
        }
    }
    void uniform2f(GLint location, GLfloat x, GLfloat y) {
        if (changed(location, {bits(x), bits(y), 0, 0})) {
            glUniform2f(location, x, y);
        }
    }
    void uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) {
        if (changed(location, {bits(x), bits(y), bits(z), 0})) {
            glUniform3f(location, x, y, z);
        }
    }
    void uniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
        if (changed(location, {bits(x), bits(y), bits(z), bits(w)})) {
            glUniform4f(location, x, y, z, w);
        }
    }

  private:
    static constexpr GLuint unknown = std::numeric_limits<GLuint>::max();
    struct Uniform {
        std::array<std::uint32_t, 4> value{};
        bool valid = false;
    };
    struct Program {
        GLuint id;
        std::vector<Uniform> uniforms;
    };
    std::vector<Program> programs_;
    Program* program_ = nullptr;
    std::array<GLuint, 14> textures_{}, cubemaps_{}, samplers_{};
    GLenum activeUnit_ = unknown;

    static std::uint32_t bits(GLfloat value) { return std::bit_cast<std::uint32_t>(value); }
    bool changed(GLint location, std::array<std::uint32_t, 4> value) {
        if (location < 0) {
            return false;
        }
        auto& uniforms = program_->uniforms;
        if (static_cast<std::size_t>(location) >= uniforms.size()) {
            uniforms.resize(location + 1);
        }
        auto& uniform = uniforms[location];
        if (uniform.valid && uniform.value == value) {
            return false;
        }
        uniform.value = value;
        uniform.valid = true;
        return true;
    }
};
} // namespace engine::graphics::detail
