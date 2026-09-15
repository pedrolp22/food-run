#include "engine/graphics/ShaderProgram.h"
#include <array>
#include <glad/glad.h>
#include <stdexcept>
#include <string>
#include <utility>

namespace engine::graphics {
namespace {
std::string withSharedSource(std::string_view source, std::string_view shared) {
    std::string combined(source);
    if (!shared.empty()) {
        const auto version = combined.find("#version");
        const auto lineEnd = combined.find('\n', version);
        if (version == std::string::npos || lineEnd == std::string::npos) {
            throw std::invalid_argument("Shared shader source requires a #version line");
        }
        combined.insert(lineEnd + 1, std::string(shared) + '\n');
    }
    return combined;
}
unsigned int compile(unsigned int type, std::string_view source, std::string_view label) {
    const unsigned int shader = glCreateShader(type);
    const char* data = source.data();
    const int length = static_cast<int>(source.size());
    glShaderSource(shader, 1, &data, &length);
    glCompileShader(shader);
    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success) {
        return shader;
    }
    std::array<char, 2048> log{};
    glGetShaderInfoLog(shader, static_cast<int>(log.size()), nullptr, log.data());
    glDeleteShader(shader);
    throw std::runtime_error(std::string(label) + " shader compilation failed: " + log.data());
}
} // namespace
ShaderProgram::ShaderProgram(std::string_view vertex, std::string_view fragment, std::string_view label,
                             std::string_view sharedFragment, std::string_view sharedVertex) {
    const auto vertexSource = withSharedSource(vertex, sharedVertex);
    const auto fragmentSource = withSharedSource(fragment, sharedFragment);
    const unsigned int vs = compile(GL_VERTEX_SHADER, vertexSource, label);
    unsigned int fs = 0;
    try {
        fs = compile(GL_FRAGMENT_SHADER, fragmentSource, label);
    } catch (...) {
        glDeleteShader(vs);
        throw;
    }
    id_ = glCreateProgram();
    glAttachShader(id_, vs);
    glAttachShader(id_, fs);
    glLinkProgram(id_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    int success = 0;
    glGetProgramiv(id_, GL_LINK_STATUS, &success);
    if (!success) {
        std::array<char, 2048> log{};
        glGetProgramInfoLog(id_, static_cast<int>(log.size()), nullptr, log.data());
        glDeleteProgram(std::exchange(id_, 0));
        throw std::runtime_error(std::string(label) + " shader linking failed: " + log.data());
    }
}
ShaderProgram::~ShaderProgram() {
    if (id_) {
        glDeleteProgram(id_);
    }
}
ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept : id_(std::exchange(other.id_, 0)) {}
ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this != &other) {
        if (id_) {
            glDeleteProgram(id_);
        }
        id_ = std::exchange(other.id_, 0);
    }
    return *this;
}
void ShaderProgram::bind() const {
    glUseProgram(id_);
}
int ShaderProgram::uniform(const char* name) const {
    const int location = glGetUniformLocation(id_, name);
    if (location < 0) {
        throw std::runtime_error(std::string("Missing shader uniform: ") + name);
    }
    return location;
}
} // namespace engine::graphics
