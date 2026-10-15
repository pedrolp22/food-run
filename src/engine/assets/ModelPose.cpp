#include "engine/assets/ModelPose.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace engine::assets {
using namespace math;
ModelPose::ModelPose(const ModelData& data) : data_(data) {
    std::vector<bool> visited(data.nodes.size());
    for (int node : data.nodeOrder) {
        if (node < 0 || node >= static_cast<int>(data.nodes.size()) || visited[node]) {
            throw std::invalid_argument("Invalid model pose node order");
        }
        const int parent = data.nodes[node].parent;
        if (parent < -1 || parent >= static_cast<int>(data.nodes.size()) || (parent >= 0 && !visited[parent])) {
            throw std::invalid_argument("Model pose parents must precede their children");
        }
        visited[node] = true;
    }
    for (const auto& skin : data.skins) {
        if (skin.joints.empty() || skin.joints.size() > kMaxSkinJoints ||
            skin.joints.size() != skin.inverseBindMatrices.size()) {
            throw std::invalid_argument("Invalid model pose skin");
        }
        for (int node : skin.joints) {
            if (node < 0 || node >= static_cast<int>(visited.size()) || !visited[node]) {
                throw std::invalid_argument("Invalid model pose joint");
            }
        }
    }
    locals_.resize(data.nodes.size());
    worlds_.resize(data.nodes.size());
    palettes_.resize(data.primitives.size());
    primitiveTransforms_.resize(data.primitives.size(), identity());
    meshBindInverse_.reserve(data.primitives.size());
    for (std::size_t i = 0; i < data.primitives.size(); ++i) {
        const auto& primitive = data.primitives[i];
        if (primitive.vertices.size() % kModelVertexStride != 0 ||
            (primitive.node >= 0 && (primitive.node >= static_cast<int>(visited.size()) || !visited[primitive.node]))) {
            throw std::invalid_argument("Invalid model pose primitive");
        }
        meshBindInverse_.push_back(primitive.node < 0 || primitive.skin >= 0
                                       ? identity()
                                       : inverseAffine(data.nodes.at(primitive.node).worldBind));
        if (primitive.skin >= 0) {
            palettes_[i].resize(data.skins.at(primitive.skin).joints.size());
            if (primitive.influences.size() != primitive.vertices.size() / kModelVertexStride) {
                throw std::invalid_argument("Missing model skin influences");
            }
            for (const auto& influence : primitive.influences) {
                float sum = 0;
                for (int j = 0; j < 4; ++j) {
                    if (influence.joints[j] >= palettes_[i].size() || !std::isfinite(influence.weights[j]) ||
                        influence.weights[j] < 0) {
                        throw std::invalid_argument("Invalid model skin influence");
                    }
                    sum += influence.weights[j];
                }
                if (std::abs(sum - 1) > .0001f)
                    throw std::invalid_argument("Model skin weights must sum to one");
            }
        }
    }
    reset();
    evaluate();
}
void ModelPose::reset() {
    for (std::size_t i = 0; i < locals_.size(); ++i) {
        locals_[i] = data_.nodes[i].localBind;
    }
}
void ModelPose::setLocal(int node, const Mat4& transform) {
    if (std::any_of(transform.values.begin(), transform.values.end(),
                    [](float value) { return !std::isfinite(value); }) ||
        transform.values[3] != 0 || transform.values[7] != 0 || transform.values[11] != 0 ||
        transform.values[15] != 1) {
        throw std::invalid_argument("Non-finite model pose transform");
    }
    locals_.at(node) = transform;
}
void ModelPose::evaluate() {
    for (int node : data_.nodeOrder) {
        const int parent = data_.nodes[node].parent;
        worlds_[node] = parent < 0 ? locals_[node] : multiply(worlds_[parent], locals_[node]);
    }
    const float infinity = std::numeric_limits<float>::infinity();
    Vec3 minimum{infinity, infinity, infinity}, maximum{-infinity, -infinity, -infinity};
    const auto include = [&](Vec3 p) {
        minimum = {std::min(minimum.x, p.x), std::min(minimum.y, p.y), std::min(minimum.z, p.z)};
        maximum = {std::max(maximum.x, p.x), std::max(maximum.y, p.y), std::max(maximum.z, p.z)};
    };
    for (std::size_t i = 0; i < data_.primitives.size(); ++i) {
        const auto& primitive = data_.primitives[i];
        auto& palette = palettes_[i];
        if (primitive.skin >= 0) {
            const auto& skin = data_.skins[primitive.skin];
            for (std::size_t j = 0; j < palette.size(); ++j) {
                palette[j] = multiply(multiply(data_.normalization, worlds_[skin.joints[j]]),
                                      multiply(skin.inverseBindMatrices[j], data_.denormalization));
            }
        } else if (primitive.node >= 0) {
            primitiveTransforms_[i] = multiply(multiply(data_.normalization, worlds_[primitive.node]),
                                               multiply(meshBindInverse_[i], data_.denormalization));
        }
        // Exact posed bounds are computed once, then reused for the main, light and reflection views.
        for (std::size_t v = 0; v < primitive.vertices.size() / kModelVertexStride; ++v) {
            const auto offset = v * kModelVertexStride;
            const Vec3 bind{primitive.vertices[offset], primitive.vertices[offset + 1], primitive.vertices[offset + 2]};
            Vec3 p{};
            if (!palette.empty()) {
                const auto& influence = primitive.influences[v];
                for (int j = 0; j < 4; ++j) {
                    if (influence.weights[j] == 0)
                        continue;
                    const auto part = transformPoint(palette[influence.joints[j]], bind);
                    p.x += part.x * influence.weights[j];
                    p.y += part.y * influence.weights[j];
                    p.z += part.z * influence.weights[j];
                }
            } else {
                p = transformPoint(primitiveTransforms_[i], bind);
            }
            include(p);
        }
    }
    if (minimum.x <= maximum.x) {
        boundsCenter_ = {(minimum.x + maximum.x) * .5f, (minimum.y + maximum.y) * .5f, (minimum.z + maximum.z) * .5f};
        boundsHalfExtent_ = {(maximum.x - minimum.x) * .5f, (maximum.y - minimum.y) * .5f,
                             (maximum.z - minimum.z) * .5f};
    }
    ++revision_;
}
} // namespace engine::assets
