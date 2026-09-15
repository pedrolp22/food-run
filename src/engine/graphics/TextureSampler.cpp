#include "engine/graphics/TextureSampler.h"
#include <glad/glad.h>
#include <stdexcept>
#include <utility>

namespace engine::graphics {
TextureSampler::TextureSampler(bool repeat) {
    glGenSamplers(1, &id_);
    if (!id_) {
        throw std::runtime_error("Could not allocate texture sampler");
    }
    glSamplerParameteri(id_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(id_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(id_, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glSamplerParameteri(id_, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
}
TextureSampler::~TextureSampler() {
    if (id_) {
        glDeleteSamplers(1, &id_);
    }
}
TextureSampler::TextureSampler(TextureSampler&& other) noexcept : id_(std::exchange(other.id_, 0)) {}
TextureSampler& TextureSampler::operator=(TextureSampler&& other) noexcept {
    if (this != &other) {
        if (id_) {
            glDeleteSamplers(1, &id_);
        }
        id_ = std::exchange(other.id_, 0);
    }
    return *this;
}
} // namespace engine::graphics
