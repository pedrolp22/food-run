#include "engine/graphics/ReflectionMap.h"
#include <bit>
#include <glad/glad.h>
#include <stdexcept>

namespace engine::graphics {
ReflectionMap::ReflectionMap(int faceSize) : faceSize_(faceSize) {
    int limit = 0;
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &limit);
    if (faceSize < 1 || faceSize > limit) {
        throw std::invalid_argument("Reflection map size must fit the OpenGL cubemap limit");
    }
    maxMipLevel_ = std::bit_width(static_cast<unsigned int>(faceSize)) - 1;
    glGenFramebuffers(1, &fbo_);
    glGenTextures(1, &texture_);
    glGenRenderbuffers(1, &depth_);
    if (!fbo_ || !texture_ || !depth_) {
        release();
        throw std::runtime_error("Could not allocate reflection map");
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture_);
    for (int face = 0; face < 6; ++face) {
        // Capture shaders already output display colors with FRAMEBUFFER_SRGB
        // disabled. sRGB storage decodes them for linear mip filtering/sampling.
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_SRGB8, faceSize, faceSize, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, maxMipLevel_);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, faceSize, faceSize);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, texture_, 0);
    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!complete) {
        release();
        throw std::runtime_error("Incomplete reflection map framebuffer");
    }
}
ReflectionMap::~ReflectionMap() {
    release();
}
void ReflectionMap::release() {
    if (fbo_) {
        glDeleteFramebuffers(1, &fbo_);
    }
    if (texture_) {
        glDeleteTextures(1, &texture_);
    }
    if (depth_) {
        glDeleteRenderbuffers(1, &depth_);
    }
    fbo_ = texture_ = depth_ = 0;
}
void ReflectionMap::bindFace(int face) const {
    if (face < 0 || face >= 6) {
        throw std::invalid_argument("Reflection map face must be in [0, 5]");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, texture_, 0);
    glViewport(0, 0, faceSize_, faceSize_);
}
void ReflectionMap::generateMipmaps() const {
    // Detach before sampling/generating mips: no texture feedback from capture.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture_);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}
} // namespace engine::graphics
