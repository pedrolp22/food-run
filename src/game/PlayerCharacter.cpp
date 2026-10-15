#include "game/PlayerCharacter.h"
#include "GameConfig.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <stdexcept>

namespace game {
using namespace engine::math;
namespace {
using SmoothVertexKey = std::array<std::int64_t, 11>;

void smoothNormals(engine::assets::PrimitiveData& primitive) {
    using namespace engine::assets;
    constexpr float positionQuantization = 10000.0f;
    constexpr float weightQuantization = 10000.0f;
    const std::size_t vertexCount = primitive.vertices.size() / kModelVertexStride;
    if (vertexCount == 0) {
        return;
    }

    const auto keyFor = [&](std::size_t vertex) {
        SmoothVertexKey key{};
        const std::size_t offset = vertex * kModelVertexStride;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            key[axis] = static_cast<std::int64_t>(std::llround(primitive.vertices[offset + axis] * positionQuantization));
        }
        // Vertices at the same position can belong to different parts of a skin.
        // Keep their normals separate when their joint influences differ.
        if (primitive.skin >= 0 && vertex < primitive.influences.size()) {
            const auto& influence = primitive.influences[vertex];
            for (std::size_t lane = 0; lane < influence.joints.size(); ++lane) {
                key[3 + lane] = influence.joints[lane];
                key[7 + lane] = static_cast<std::int64_t>(std::llround(influence.weights[lane] * weightQuantization));
            }
        }
        return key;
    };
    const auto positionFor = [&](std::size_t vertex) {
        const std::size_t offset = vertex * kModelVertexStride;
        return Vec3{primitive.vertices[offset], primitive.vertices[offset + 1], primitive.vertices[offset + 2]};
    };
    const auto cornerAngle = [](Vec3 first, Vec3 second) {
        const float firstLength = std::sqrt(dot(first, first)), secondLength = std::sqrt(dot(second, second));
        if (firstLength <= 1e-7f || secondLength <= 1e-7f) {
            return 0.0f;
        }
        const float cosine = dot({first.x / firstLength, first.y / firstLength, first.z / firstLength},
                                 {second.x / secondLength, second.y / secondLength, second.z / secondLength});
        return std::acos(std::clamp(cosine, -1.0f, 1.0f));
    };

    std::map<SmoothVertexKey, Vec3> normalSums;
    const auto addNormal = [&](const SmoothVertexKey& key, Vec3 normal, float weight) {
        auto& sum = normalSums[key];
        sum.x += normal.x * weight;
        sum.y += normal.y * weight;
        sum.z += normal.z * weight;
    };
    const auto accumulateTriangle = [&](std::size_t first, std::size_t second, std::size_t third) {
        if (first >= vertexCount || second >= vertexCount || third >= vertexCount) {
            return;
        }
        const Vec3 a = positionFor(first), b = positionFor(second), c = positionFor(third);
        const Vec3 face = cross(b - a, c - a);
        const float faceLength = std::sqrt(dot(face, face));
        if (faceLength <= 1e-8f) {
            return;
        }
        const Vec3 faceNormal{face.x / faceLength, face.y / faceLength, face.z / faceLength};
        addNormal(keyFor(first), faceNormal, cornerAngle(b - a, c - a));
        addNormal(keyFor(second), faceNormal, cornerAngle(c - b, a - b));
        addNormal(keyFor(third), faceNormal, cornerAngle(a - c, b - c));
    };
    if (primitive.indices.empty()) {
        for (std::size_t vertex = 0; vertex + 2 < vertexCount; vertex += 3) {
            accumulateTriangle(vertex, vertex + 1, vertex + 2);
        }
    } else {
        for (std::size_t index = 0; index + 2 < primitive.indices.size(); index += 3) {
            accumulateTriangle(primitive.indices[index], primitive.indices[index + 1], primitive.indices[index + 2]);
        }
    }

    for (std::size_t vertex = 0; vertex < vertexCount; ++vertex) {
        const auto sum = normalSums.find(keyFor(vertex));
        if (sum == normalSums.end() || dot(sum->second, sum->second) <= 1e-14f) {
            continue;
        }
        const Vec3 normal = normalize(sum->second);
        const std::size_t offset = vertex * kModelVertexStride + 3;
        primitive.vertices[offset] = normal.x;
        primitive.vertices[offset + 1] = normal.y;
        primitive.vertices[offset + 2] = normal.z;
    }
}

int bone(const engine::assets::ModelData& data, const char* name) {
    if (!data.skins.empty()) {
        for (int node : data.skins.front().joints) {
            if (data.nodes[node].name == name)
                return node;
        }
    }
    throw std::runtime_error(std::string("Player cat rig is missing joint: ") + name);
}
bool inBranch(const engine::assets::ModelData& data, int node, int root) {
    for (int current = node; current >= 0; current = data.nodes[current].parent) {
        if (current == root)
            return true;
    }
    return false;
}
void repositionShoulder(engine::assets::ModelData& data, int shoulder, int elbow) {
    auto& shoulderNode = data.nodes[shoulder];
    const Mat4 parentWorld = shoulderNode.parent >= 0 ? data.nodes[shoulderNode.parent].worldBind : identity();
    const Mat4 inverseParent = inverseAffine(parentWorld);
    const Vec3 origin = transformPoint(shoulderNode.worldBind, {});
    const Vec3 elbowPosition = transformPoint(data.nodes[elbow].worldBind, {});
    const Vec3 alongArm = normalize(elbowPosition - origin);
    const Vec3 displacedOrigin{origin.x + alongArm.x * .32f, origin.y + alongArm.y * .32f,
                               origin.z + alongArm.z * .32f};
    const Vec3 parentOffset = transformPoint(inverseParent, displacedOrigin) - transformPoint(inverseParent, origin);

    std::vector<std::vector<Mat4>> bindProducts(data.skins.size());
    for (std::size_t skinIndex = 0; skinIndex < data.skins.size(); ++skinIndex) {
        const auto& skin = data.skins[skinIndex];
        bindProducts[skinIndex].reserve(skin.joints.size());
        for (std::size_t joint = 0; joint < skin.joints.size(); ++joint) {
            bindProducts[skinIndex].push_back(
                multiply(data.nodes[skin.joints[joint]].worldBind, skin.inverseBindMatrices[joint]));
        }
    }

    shoulderNode.localBind.values[12] += parentOffset.x;
    shoulderNode.localBind.values[13] += parentOffset.y;
    shoulderNode.localBind.values[14] += parentOffset.z;
    for (int node : data.nodeOrder) {
        if (!inBranch(data, node, shoulder))
            continue;
        auto& rigNode = data.nodes[node];
        rigNode.worldBind = rigNode.parent >= 0 ? multiply(data.nodes[rigNode.parent].worldBind, rigNode.localBind)
                                                : rigNode.localBind;
    }
    for (std::size_t skinIndex = 0; skinIndex < data.skins.size(); ++skinIndex) {
        auto& skin = data.skins[skinIndex];
        for (std::size_t joint = 0; joint < skin.joints.size(); ++joint) {
            const int node = skin.joints[joint];
            if (inBranch(data, node, shoulder)) {
                skin.inverseBindMatrices[joint] =
                    multiply(inverseAffine(data.nodes[node].worldBind), bindProducts[skinIndex][joint]);
            }
        }
    }
}
float smoothStep(float from, float to, float value) {
    const float t = std::clamp((value - from) / (to - from), 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}
} // namespace
void preparePlayerModel(engine::assets::ModelData& data) {
    for (auto& primitive : data.primitives) {
        smoothNormals(primitive);
    }
    const int head = bone(data, "Bone.25");
    // Move the centerline shoulder pivots partway out along their upper arms while preserving
    // the exported rest-pose skinning matrices.
    repositionShoulder(data, bone(data, "Bone.04"), bone(data, "Bone.03"));
    repositionShoulder(data, bone(data, "Bone.21"), bone(data, "Bone.13"));
    // The export's separate ear mesh has no skin weights. Attach it rigidly to the head and tuck it into the fur.
    for (auto& primitive : data.primitives) {
        if (primitive.skin >= 0 || primitive.node < 0 || data.nodes[primitive.node].name != "Plane")
            continue;
        auto& node = data.nodes[primitive.node];
        node.localBind = multiply(inverseAffine(data.nodes[head].worldBind), node.worldBind);
        node.parent = head;
        node.localBind.values[13] -= .28f;
        std::erase(data.nodeOrder, primitive.node);
        data.nodeOrder.push_back(primitive.node);
        primitive.material = data.primitives.front().material;
        // The texture's center is an opaque white-fur patch. The ear UVs otherwise sample the yellow atlas islands.
        for (std::size_t vertex = 0; vertex + engine::assets::kModelVertexStride <= primitive.vertices.size();
             vertex += engine::assets::kModelVertexStride) {
            primitive.vertices[vertex + 6] = .5f;
            primitive.vertices[vertex + 7] = .5f;
        }
    }
}
PlayerCharacter::Joint PlayerCharacter::joint(const char* name) const {
    const auto& data = pose_.data();
    const int node = bone(data, name);
    const auto inverse = inverseAffine(data.nodes[node].worldBind);
    const auto direction = [&](Vec3 vector) {
        return normalize(transformPoint(inverse, vector) - transformPoint(inverse, {}));
    };
    return {node, direction({1, 0, 0}), direction({0, 1, 0}), direction({0, 0, 1})};
}
PlayerCharacter::PlayerCharacter(const engine::assets::ModelData& data)
    : pose_(data), root_(joint("Bone")), head_(joint("Bone.25")), shoulders_{joint("Bone.04"), joint("Bone.21")},
      elbows_{joint("Bone.06"), joint("Bone.16")}, wrists_{joint("Bone.08"), joint("Bone.18")},
      thighs_{joint("Bone.11"), joint("Bone.01")}, knees_{joint("Bone.15"), joint("Bone.05")},
      tail_{joint("Bone.30"), joint("Bone.31"), joint("Bone.32"),
            joint("Bone.33"), joint("Bone.29"), joint("Bone.35")} {
    anchor_ = transformPoint(data.normalization, transformPoint(data.nodes[bone(data, "Bone.00")].worldBind, {}));
    anchor_.y = 0;
    update(0, 0, 0, 0);
}
void PlayerCharacter::rotate(const Joint& joint, float x, float y, float z) {
    const auto offset = multiply(rotation(joint.z, z), multiply(rotation(joint.x, x), rotation(joint.y, y)));
    pose_.setLocal(joint.node, multiply(pose_.data().nodes[joint.node].localBind, offset));
}
void PlayerCharacter::update(float playerX, float distance, float time, float speed) {
    if (!std::isfinite(playerX) || !std::isfinite(distance) || !std::isfinite(time) || !std::isfinite(speed)) {
        throw std::invalid_argument("Non-finite player animation input");
    }
    speed = std::max(speed, 0.f);
    const bool restart = time == 0 && distance == 0;
    if (speed == 0 && previousSpeed_ > 0 && !restart) {
        // Removing fixed-step interpolation on game over can move presentation backward.
        // Preserve the last running pose; restarting at time/distance zero resets normally.
        previousTime_ = time;
        previousDistance_ = distance;
        previousX_ = playerX;
        previousSpeed_ = speed;
        return;
    }
    if (time == previousTime_ && distance == previousDistance_ && playerX == previousX_ && speed == previousSpeed_) {
        // Game over freezes the last complete pose even when the movement speed switches off.
        return;
    }
    const bool reset = previousTime_ < 0 || time < previousTime_ || distance < previousDistance_;
    const float dt = reset ? 0 : std::max(time - previousTime_, 0.f);
    const float response = reset ? 1 : 1 - std::exp(-GameConfig::kPlayerAnimationResponse * dt);
    motion_ = std::lerp(reset ? 0.f : motion_, speed > 0 ? 1.f : 0.f, response);
    const float lateralSpeed = !reset && dt > 0 ? (playerX - previousX_) / dt : 0;
    const float lean = std::clamp(lateralSpeed / GameConfig::kLaneChangeSpeed, -1.f, 1.f);
    laneLean_ = std::lerp(reset ? 0.f : laneLean_, lean, response);
    heading_ = -laneLean_ * radians(GameConfig::kPlayerLaneTurnDegrees);
    if (speed > 0 || reset)
        phase_ = std::fmod(distance / GameConfig::kPlayerRunStride, 1.f) * 2 * kPi;
    const float cadence = std::clamp(speed / GameConfig::kInitialSpeed, .45f, 1.f);
    pose_.reset();
    const float torsoPitch = motion_ * (6 + 1.5f * std::sin(phase_ * 2));
    rotate(root_, laneLean_ * radians(GameConfig::kPlayerLaneLeanDegrees), 0, -radians(torsoPitch));
    rotate(head_, 0, motion_ * radians(2) * std::sin(phase_),
           motion_ * (radians(3) - radians(2) * std::cos(phase_ * 2)));
    for (int side = 0; side < 2; ++side) {
        const float sign = side == 0 ? 1.f : -1.f;
        const float cycle = phase_ + side * kPi;
        const float stride = std::sin(cycle);
        const float armSwing = -std::sin(phase_) * motion_ * cadence * radians(30);
        // Arms extend along opposite Z; a shared Y swing moves them in opposite forward/back
        // directions, counter to the legs, instead of flapping them up and down.
        rotate(shoulders_[side], sign * radians(48), armSwing);
        rotate(elbows_[side], 0, sign * motion_ * radians(28));
        rotate(wrists_[side], 0, sign * motion_ * radians(5) * std::sin(cycle - .3f));
        // The cat faces local +X, so Z-axis hip/knee motion swings the legs through the running
        // plane. Matching knee directions keep the lower legs from bowing toward each other.
        rotate(thighs_[side], 0, 0, stride * motion_ * cadence * radians(30));
        const float knee = motion_ * (radians(7) + radians(38) * std::max(0.f, std::cos(cycle + .15f)));
        rotate(knees_[side], 0, 0, knee);
    }
    for (std::size_t i = 0; i < tail_.size(); ++i) {
        const float taper = 1 - .07f * static_cast<float>(i);
        rotate(tail_[i], radians(5) * std::sin(phase_ - i * .35f) * motion_ * taper,
               radians(9) * std::sin(phase_ - i * .45f) * motion_ * taper, radians(-2));
    }
    pose_.evaluate();
    lift_ = GameConfig::kPlayerRunBobHeight * motion_ * (.5f + .5f * std::cos(phase_ * 2));
    previousTime_ = time;
    previousDistance_ = distance;
    previousX_ = playerX;
    previousSpeed_ = speed;
}
void PlayerCharacter::updateMenu(float time) {
    if (!std::isfinite(time)) {
        throw std::invalid_argument("Non-finite player menu animation time");
    }
    const float phase = time * (2 * kPi / GameConfig::kPlayerMenuTapPeriod);
    const float tapLeft = std::pow(std::max(0.f, std::sin(phase)), 8.f);
    const float tapRight = std::pow(std::max(0.f, std::sin(phase + kPi)), 8.f);
    constexpr float scratchPeriod = 3.8f;
    const float scratchPhase = std::fmod(std::max(time, 0.0f), scratchPeriod) / scratchPeriod;
    const float scratch = smoothStep(.08f, .20f, scratchPhase) * (1 - smoothStep(.72f, .86f, scratchPhase));
    const float scratchFidget = std::sin(time * 17.0f);
    pose_.reset();
    rotate(root_, radians(.8f * std::sin(phase * .43f)), 0, radians(1.2f * std::sin(phase * .71f)));
    // The cat faces the camera while keeping its attention on the floating menu beside it.
    rotate(head_, 0, radians(10 + 2 * std::sin(phase * .55f)), radians(1.5f * std::sin(phase * .8f)));
    for (int side = 0; side < 2; ++side) {
        const float sign = side == 0 ? 1.f : -1.f;
        const float fidget = std::sin(phase * .7f + side * 1.3f);
        const float footTap = side == 0 ? tapLeft : tapRight;
        float shoulder = sign * radians(42 + 1.5f * fidget);
        float elbow = sign * radians(18 + 1.5f * fidget);
        float wrist = sign * radians(3 * fidget);
        if (side == 1) {
            // Lift the far-side upper arm, then fold the forearm back toward the head.
            // This rig needs a large positive elbow bend to bring the paw in from its long side reach.
            shoulder = std::lerp(shoulder, radians(50 + 2 * scratchFidget), scratch);
            elbow = std::lerp(elbow, radians(156 + 5 * scratchFidget), scratch);
            wrist = std::lerp(wrist, radians(2 + 4 * scratchFidget), scratch);
        }
        rotate(shoulders_[side], shoulder);
        rotate(elbows_[side], elbow);
        rotate(wrists_[side], wrist);
        rotate(thighs_[side], (side == 0 ? -1 : 1) * radians(2 + 7 * footTap));
        const float kneeBend = radians(3 + 12 * footTap);
        rotate(knees_[side], -sign * kneeBend);
    }
    for (std::size_t i = 0; i < tail_.size(); ++i) {
        const float taper = 1 - .07f * static_cast<float>(i);
        rotate(tail_[i], radians(1.5f) * std::sin(phase * .45f - i * .35f) * taper,
               radians(3.5f) * std::sin(phase * .6f - i * .4f) * taper, radians(-2));
    }
    pose_.evaluate();
    lift_ = 0;
    motion_ = 0;
    laneLean_ = 0;
    heading_ = 0;
    previousTime_ = time;
    previousDistance_ = 0;
    previousX_ = 0;
    previousSpeed_ = 0;
}
} // namespace game
