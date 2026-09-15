#include "engine/graphics/RenderTarget.h"
#include <algorithm>
#include <glad/glad.h>
#include <stdexcept>
#include <utility>

namespace engine::graphics {
RenderTarget::RenderTarget(Size size, Kind kind, int samples) : size_(size) {
    if (size.width <= 0 || size.height <= 0 || samples < 1 || (kind == Kind::Shadow && samples != 1)) {
        throw std::invalid_argument("Render target requires positive dimensions/samples; shadows use one sample");
    }
    if (samples > 1) {
        int maxSamples = 1;
        glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
        samples_ = std::min(samples, maxSamples);
    }
    glGenFramebuffers(1, &fbo_);
    glGenTextures(1, &texture_);
    if (kind == Kind::ColorDepth) {
        glGenRenderbuffers(1, &depth_);
    }
    if (samples_ > 1) {
        glGenFramebuffers(1, &resolveFbo_);
        glGenRenderbuffers(1, &colorBuffer_);
    }
    if (!fbo_ || !texture_ || (kind == Kind::ColorDepth && !depth_) ||
        (samples_ > 1 && (!resolveFbo_ || !colorBuffer_))) {
        release();
        throw std::runtime_error("Could not allocate render target");
    }
    const bool shadow = kind == Kind::Shadow;
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, shadow ? GL_DEPTH_COMPONENT24 : GL_RGB8, size.width, size.height, 0,
                 shadow ? GL_DEPTH_COMPONENT : GL_RGB, shadow ? GL_FLOAT : GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, shadow ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shadow ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, shadow ? GL_CLAMP_TO_BORDER : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, shadow ? GL_CLAMP_TO_BORDER : GL_CLAMP_TO_EDGE);
    if (shadow) {
        constexpr float border[] = {1, 1, 1, 1};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, samples_ > 1 ? resolveFbo_ : fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, shadow ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_,
                           0);
    if (samples_ > 1) {
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            release();
            throw std::runtime_error("Incomplete resolve target");
        }
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glBindRenderbuffer(GL_RENDERBUFFER, colorBuffer_);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples_, GL_RGB8, size.width, size.height);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &samples_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorBuffer_);
    }
    if (shadow) {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    } else {
        glBindRenderbuffer(GL_RENDERBUFFER, depth_);
        if (samples_ > 1) {
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples_, GL_DEPTH_COMPONENT24, size.width, size.height);
        } else {
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size.width, size.height);
        }
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!complete) {
        release();
        throw std::runtime_error("Incomplete render target");
    }
}
RenderTarget::~RenderTarget() {
    release();
}
RenderTarget::RenderTarget(RenderTarget&& other) noexcept {
    *this = std::move(other);
}
RenderTarget& RenderTarget::operator=(RenderTarget&& other) noexcept {
    if (this != &other) {
        release();
        fbo_ = std::exchange(other.fbo_, 0);
        texture_ = std::exchange(other.texture_, 0);
        depth_ = std::exchange(other.depth_, 0);
        resolveFbo_ = std::exchange(other.resolveFbo_, 0);
        colorBuffer_ = std::exchange(other.colorBuffer_, 0);
        samples_ = std::exchange(other.samples_, 1);
        size_ = std::exchange(other.size_, {});
    }
    return *this;
}
void RenderTarget::release() {
    if (fbo_) {
        glDeleteFramebuffers(1, &fbo_);
    }
    if (texture_) {
        glDeleteTextures(1, &texture_);
    }
    if (depth_) {
        glDeleteRenderbuffers(1, &depth_);
    }
    if (resolveFbo_) {
        glDeleteFramebuffers(1, &resolveFbo_);
    }
    if (colorBuffer_) {
        glDeleteRenderbuffers(1, &colorBuffer_);
    }
    fbo_ = texture_ = depth_ = resolveFbo_ = colorBuffer_ = 0;
}
void RenderTarget::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, size_.width, size_.height);
}
void RenderTarget::resolve() const {
    if (samples_ > 1) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFbo_);
        glBlitFramebuffer(0, 0, size_.width, size_.height, 0, 0, size_.width, size_.height, GL_COLOR_BUFFER_BIT,
                          GL_NEAREST);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, samples_ > 1 ? resolveFbo_ : fbo_);
}
void RenderTarget::bindDefault(Size size) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, size.width, size.height);
}
} // namespace engine::graphics
