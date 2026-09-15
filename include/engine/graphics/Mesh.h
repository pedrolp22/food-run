#pragma once

#include <initializer_list>
#include <span>

namespace engine::graphics {
struct VertexAttribute {
    unsigned int location;
    int components;
    int offset;
};
// Interleaved floats; stride and attribute offsets are measured in floats.
class Mesh {
  public:
    Mesh() = default;
    Mesh(std::span<const float> vertices, int stride, std::initializer_list<VertexAttribute> attributes,
         std::span<const unsigned int> indices = {});
    ~Mesh();
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    void draw() const;
    int count() const { return count_; }

  private:
    void release();
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int ebo_ = 0;
    int count_ = 0;
};
Mesh makeCubeMesh();
Mesh makeQuadMesh();
class Texture;
// Requires a texture loaded with retainAlpha. Sprite attributes: position 0, UV 1, normal 2.
Mesh makeSpriteMesh(const Texture& texture);
} // namespace engine::graphics
