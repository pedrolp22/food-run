#include "engine/math/Math.h"
#include <cmath>

namespace engine::math {

float radians(float degrees) {
    return degrees * kPi / 180.0f;
}

Vec3 operator-(const Vec3& left, const Vec3& right) {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

float dot(const Vec3& left, const Vec3& right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

Vec3 cross(const Vec3& left, const Vec3& right) {
    return {left.y * right.z - left.z * right.y, left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

Vec3 normalize(const Vec3& value) {
    const float length = std::sqrt(dot(value, value));
    if (length <= 0.0001f) {
        return {0.0f, 0.0f, 0.0f};
    }

    return {value.x / length, value.y / length, value.z / length};
}

Mat4 identity() {
    Mat4 result;
    result.values[0] = 1.0f;
    result.values[5] = 1.0f;
    result.values[10] = 1.0f;
    result.values[15] = 1.0f;
    return result;
}

Mat4 multiply(const Mat4& left, const Mat4& right) {
    Mat4 result;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float value = 0.0f;
            for (int index = 0; index < 4; ++index) {
                value += left.values[index * 4 + row] * right.values[column * 4 + index];
            }
            result.values[column * 4 + row] = value;
        }
    }
    return result;
}

Mat4 translation(const Vec3& position) {
    Mat4 result = identity();
    result.values[12] = position.x;
    result.values[13] = position.y;
    result.values[14] = position.z;
    return result;
}

Mat4 scale(const Vec3& size) {
    Mat4 result = identity();
    result.values[0] = size.x;
    result.values[5] = size.y;
    result.values[10] = size.z;
    return result;
}

Mat4 rotateX(float angleRadians) {
    const float cosine = std::cos(angleRadians);
    const float sine = std::sin(angleRadians);

    Mat4 result = identity();
    result.values[5] = cosine;
    result.values[6] = sine;
    result.values[9] = -sine;
    result.values[10] = cosine;
    return result;
}

Mat4 rotateY(float angleRadians) {
    const float cosine = std::cos(angleRadians);
    const float sine = std::sin(angleRadians);

    Mat4 result = identity();
    result.values[0] = cosine;
    result.values[2] = -sine;
    result.values[8] = sine;
    result.values[10] = cosine;
    return result;
}

Mat4 perspective(float fovRadians, float aspectRatio, float nearPlane, float farPlane) {
    Mat4 result;
    const float tanHalfFov = std::tan(fovRadians / 2.0f);

    result.values[0] = 1.0f / (aspectRatio * tanHalfFov);
    result.values[5] = 1.0f / tanHalfFov;
    result.values[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    result.values[11] = -1.0f;
    result.values[14] = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);

    return result;
}

Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    const Vec3 forward = normalize(center - eye);
    const Vec3 side = normalize(cross(forward, up));
    const Vec3 cameraUp = cross(side, forward);

    Mat4 result = identity();
    result.values[0] = side.x;
    result.values[1] = cameraUp.x;
    result.values[2] = -forward.x;

    result.values[4] = side.y;
    result.values[5] = cameraUp.y;
    result.values[6] = -forward.y;

    result.values[8] = side.z;
    result.values[9] = cameraUp.z;
    result.values[10] = -forward.z;

    result.values[12] = -dot(side, eye);
    result.values[13] = -dot(cameraUp, eye);
    result.values[14] = dot(forward, eye);

    return result;
}

Vec3 transformPoint(const Mat4& m, const Vec3& p) {
    return {m.values[0] * p.x + m.values[4] * p.y + m.values[8] * p.z + m.values[12],
            m.values[1] * p.x + m.values[5] * p.y + m.values[9] * p.z + m.values[13],
            m.values[2] * p.x + m.values[6] * p.y + m.values[10] * p.z + m.values[14]};
}

Vec3 transformNormal(const Mat4& m, const Vec3& n) {
    // Normals use the inverse transpose, including nonuniform or mirrored node scales.
    const float a = m.values[0], b = m.values[4], c = m.values[8];
    const float d = m.values[1], e = m.values[5], f = m.values[9];
    const float g = m.values[2], h = m.values[6], i = m.values[10];
    const float determinant = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if (std::abs(determinant) < 1e-12f) {
        return {0.0f, 1.0f, 0.0f};
    }
    const float x = (e * i - f * h) * n.x + (f * g - d * i) * n.y + (d * h - e * g) * n.z;
    const float y = (c * h - b * i) * n.x + (a * i - c * g) * n.y + (b * g - a * h) * n.z;
    const float z = (b * f - c * e) * n.x + (c * d - a * f) * n.y + (a * e - b * d) * n.z;
    const float len = std::sqrt(x * x + y * y + z * z);
    if (len <= 1e-12f) {
        return {0.0f, 1.0f, 0.0f};
    }
    const float sign = determinant < 0.0f ? -1.0f : 1.0f;
    return {sign * x / len, sign * y / len, sign * z / len};
}

Mat4 orthographic(float halfWidth, float halfHeight, float nearPlane, float farPlane) {
    Mat4 result = identity();
    result.values[0] = 1.0f / halfWidth;
    result.values[5] = 1.0f / halfHeight;
    result.values[10] = -2.0f / (farPlane - nearPlane);
    result.values[14] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    return result;
}

} // namespace engine::math
