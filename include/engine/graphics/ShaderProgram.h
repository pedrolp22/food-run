#pragma once

#include <string_view>

namespace engine::graphics {
// Move-only GL ownership. Destroy while the creating context is current.
class ShaderProgram {
  public:
    ShaderProgram() = default;
    ShaderProgram(std::string_view vertex, std::string_view fragment, std::string_view label,
                  std::string_view sharedFragment = {}, std::string_view sharedVertex = {});
    ~ShaderProgram();
    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    void bind() const;
    int uniform(const char* name) const;
    unsigned int id() const { return id_; }

  private:
    unsigned int id_ = 0;
};
} // namespace engine::graphics
