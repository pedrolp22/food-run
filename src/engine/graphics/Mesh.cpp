#include "engine/graphics/Mesh.h"
#include <algorithm>
#include <glad/glad.h>
#include <limits>
#include <stdexcept>
#include <utility>

namespace engine::graphics {
Mesh::Mesh(std::span<const float> vertices, int stride, std::initializer_list<VertexAttribute> attributes,
           std::span<const unsigned int> indices) {
    if (stride <= 0 || vertices.empty() || vertices.size() % stride != 0) {
        throw std::invalid_argument("Invalid mesh vertex layout");
    }
    const auto vertexCount = vertices.size() / stride;
    const auto count = indices.empty() ? vertexCount : indices.size();
    if (count > static_cast<std::size_t>(std::numeric_limits<int>::max()) || count % 3 != 0 ||
        std::any_of(indices.begin(), indices.end(), [&](auto index) { return index >= vertexCount; })) {
        throw std::invalid_argument("Invalid triangle mesh indices or vertex count");
    }
    for (const auto& attribute : attributes) {
        if (attribute.components < 1 || attribute.components > 4 || attribute.offset < 0 ||
            attribute.offset + attribute.components > stride) {
            throw std::invalid_argument("Invalid vertex attribute");
        }
    }
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    if (!indices.empty()) {
        glGenBuffers(1, &ebo_);
    }
    if (!vao_ || !vbo_ || (!indices.empty() && !ebo_)) {
        release();
        throw std::runtime_error("Could not allocate mesh buffers");
    }
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size_bytes()), vertices.data(), GL_STATIC_DRAW);
    for (const auto& attribute : attributes) {
        glVertexAttribPointer(attribute.location, attribute.components, GL_FLOAT, GL_FALSE,
                              stride * static_cast<int>(sizeof(float)),
                              reinterpret_cast<void*>(attribute.offset * sizeof(float)));
        glEnableVertexAttribArray(attribute.location);
    }
    if (ebo_) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size_bytes()), indices.data(),
                     GL_STATIC_DRAW);
    }
    count_ = static_cast<int>(indices.empty() ? vertices.size() / stride : indices.size());
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
Mesh::~Mesh() {
    release();
}
Mesh::Mesh(Mesh&& other) noexcept {
    *this = std::move(other);
}
Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        release();
        vao_ = std::exchange(other.vao_, 0);
        vbo_ = std::exchange(other.vbo_, 0);
        ebo_ = std::exchange(other.ebo_, 0);
        count_ = std::exchange(other.count_, 0);
    }
    return *this;
}
void Mesh::release() {
    if (vao_) {
        glDeleteVertexArrays(1, &vao_);
    }
    if (vbo_) {
        glDeleteBuffers(1, &vbo_);
    }
    if (ebo_) {
        glDeleteBuffers(1, &ebo_);
    }
    vao_ = vbo_ = ebo_ = 0;
    count_ = 0;
}
void Mesh::draw() const {
    if (!count_) {
        return;
    }
    glBindVertexArray(vao_);
    if (ebo_) {
        glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_INT, nullptr);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, count_);
    }
}
} // namespace engine::graphics
