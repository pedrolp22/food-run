#pragma once

#include <array>

namespace engine::math {
inline constexpr float kPi = 3.14159265358979323846f;
struct Vec2 {
    float x = 0;
    float y = 0;
};
struct Vec3 {
    float x = 0;
    float y = 0;
    float z = 0;
};
// Column-major matrices, column vectors, right-handed coordinates (OpenGL).
struct Mat4 {
    std::array<float, 16> values{};
};

float radians(float degrees);
Vec3 operator-(const Vec3& left, const Vec3& right);
float dot(const Vec3& left, const Vec3& right);
Vec3 cross(const Vec3& left, const Vec3& right);
Vec3 normalize(const Vec3& value);
Mat4 identity();
Mat4 multiply(const Mat4& left, const Mat4& right);
Mat4 translation(const Vec3& position);
Mat4 scale(const Vec3& size);
Mat4 rotateX(float angleRadians);
Mat4 rotateY(float angleRadians);
Mat4 perspective(float fovRadians, float aspectRatio, float nearPlane, float farPlane);
Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up);
Vec3 transformPoint(const Mat4& m, const Vec3& p);
Vec3 transformNormal(const Mat4& m, const Vec3& n);
Mat4 orthographic(float halfWidth, float halfHeight, float nearPlane, float farPlane);
} // namespace engine::math
