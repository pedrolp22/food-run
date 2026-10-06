#include "game/SceneryLayout.h"
#include "game/WorldScene.h"
#include <algorithm>
#include <cmath>

namespace game {
using namespace engine::math;
using namespace engine::graphics;
using namespace GameConfig;
using namespace scenery;
namespace {
float smooth(float low, float high, float value) {
    const float t = std::clamp((value - low) / (high - low), 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}
float closestFraction(Vec2 a, Vec2 b, Vec2 point) {
    const float x = b.x - a.x, z = b.y - a.y;
    const float squared = x * x + z * z;
    return squared > .000001f ? std::clamp(((point.x - a.x) * x + (point.y - a.y) * z) / squared, 0.0f, 1.0f) : 1;
}
} // namespace

void WorldScene::addLeaves(float playerX, float scroll) {
    static_assert(kFallingLeavesPerTree >= 0 && kRoadLeavesPerTree >= 0 && kRoadLeavesPerTree < 65536);
    static_assert(kMaxVisibleLeaves > 0 && kLeafSize > 0 && kLeafFallSpeed > 0);
    static_assert(kLeafWakeRadius > 0 && kLeafLiftSeconds > 0 && kLeafLiftHeight > 0);
    const float time = scene_.time;
    if (!std::isfinite(time) || !std::isfinite(scroll) || !std::isfinite(playerX)) {
        return;
    }
    if (time < previousLeafTime_ || scroll < previousLeafScroll_) {
        liftedLeaves_.clear(); // Restart/replay: no leaf from an earlier run remains spent.
        previousLeafTime_ = -1;
    }
    const float dt = previousLeafTime_ >= 0 ? time - previousLeafTime_ : 0;
    const Vec2 previous = {previousLeafPlayerX_, kPlayerZ - previousLeafScroll_};
    const Vec2 current = {playerX, kPlayerZ - scroll};
    const bool moving = dt > 0 && (std::abs(scroll - previousLeafScroll_) > .00001f ||
                                   std::abs(playerX - previousLeafPlayerX_) > .00001f);
    const float speed = dt > 0 ? std::clamp((scroll - previousLeafScroll_) / dt, 0.0f, kMaxSpeed) : 0;
    std::erase_if(liftedLeaves_, [&](const auto& entry) {
        const float z = entry.second.baseZ + scroll;
        return z > 24 || z < -100;
    });
    const auto leaf = [&](Vec3 position, const Mat4& rotation, float size, float opacity) {
        if (opacity <= .001f) {
            return;
        }
        Quad particle;
        particle.texture = &assets_.texture(TextureId::Leaf);
        particle.materialTextures = assets_.materialTextures(TextureId::Leaf);
        particle.model =
            multiply(translation(position), multiply(rotation, scale({size, size / particle.texture->aspect(), 1})));
        particle.normal = transformNormal(rotation, {0, 0, 1});
        particle.opacity = opacity;
        particle.alphaCutoff = .1f;
        particle.affineStrength = 0;
        particle.doubleSided = particle.pbrSurface = true;
        particle.castsShadow = false;
        particle.surfaceMaterial = {1, 0, 1}; // Waxy foliage and source veins remain dielectric.
        particle.worldSize = {size, size / particle.texture->aspect()};
        scene_.particles.push_back(particle);
    };
    for (const auto& tree : trees_) {
        if (tree.position.z < -72 || tree.position.z > 16) {
            continue;
        }
        const float side = tree.position.x - track_.offset(tree.position.z) < 0 ? -1.0f : 1.0f;
        const float radius = tree.height * .476f;
        for (int index = 0; index < kFallingLeavesPerTree; ++index) {
            const auto seed = hashSeed(tree.seed + index * 349u);
            const float duration = tree.height * .85f / kLeafFallSpeed;
            const float phase = std::fmod(time / duration + hashFloat(seed, 0), 1.0f);
            const float flutter = time * (2.5f + hashFloat(seed, 1)) + hashFloat(seed, 2) * 2 * kPi;
            const float z =
                tree.position.z + (hashFloat(seed, 3) - .5f) * radius * 1.3f + std::sin(flutter * .45f) * .35f;
            const float x = tree.position.x + (hashFloat(seed, 4) - .5f) * radius * 1.3f - side * phase * .7f +
                            std::sin(flutter) * .3f;
            const Vec3 position = {x, sidewalkTopY - track_.drop(z) + tree.height * .85f * (1 - phase), z};
            const auto rotation =
                multiply(rotateY(flutter * .7f), rotateX(-kPi * .5f + phase * 2 * kPi + std::sin(flutter) * .5f));
            const float opacity = smooth(0, .06f, phase) * (1 - smooth(.82f, 1, phase));
            leaf(position, rotation, kLeafSize * std::lerp(.75f, 1.25f, hashFloat(seed, 5)), opacity);
        }
        for (int index = 0; index < kRoadLeavesPerTree; ++index) {
            const auto seed = hashSeed(tree.seed + index * 719u + 913u);
            const auto id = tree.id | std::uint64_t(index);
            const float baseZ = tree.position.z - scroll + (hashFloat(seed, 0) - .5f) * radius * 2;
            const float groundZ = baseZ + scroll;
            const float localX = side * (roadHalfEdge - .1f - hashFloat(seed, 1) * .65f);
            auto lifted = liftedLeaves_.find(id);
            if (lifted == liftedLeaves_.end() && moving) {
                const Vec2 point = {localX, baseZ};
                const float fraction = closestFraction(previous, current, point);
                const float dx = point.x - std::lerp(previous.x, current.x, fraction);
                const float dz = point.y - std::lerp(previous.y, current.y, fraction);
                if (dx * dx + dz * dz < kLeafWakeRadius * kLeafWakeRadius) {
                    lifted = liftedLeaves_
                                 .emplace(id, LiftedLeaf{std::lerp(previousLeafTime_, time, fraction), baseZ, speed})
                                 .first;
                }
            }
            float z = groundZ, x = localX, rise = 0, opacity = 1;
            const float spin = hashFloat(seed, 2) * 2 * kPi;
            Mat4 rotation;
            if (lifted != liftedLeaves_.end()) {
                const float age = std::max(0.0f, time - lifted->second.time);
                const float progress = age / kLeafLiftSeconds;
                if (progress >= 1) {
                    continue; // Keep the spent ID until it leaves the scenery, so it cannot reappear behind the runner.
                }
                z -= std::max(2.5f, lifted->second.speed * .65f) * age;
                x += side * age * .65f + std::sin(age * 7 + spin) * age * .3f;
                rise = kLeafLiftHeight * 4 * progress * (1 - progress);
                opacity = 1 - smooth(.2f, 1, progress);
                rotation =
                    multiply(rotateY(spin + age * 6), rotateX(-kPi * .5f + age * 4 + std::sin(age * 9 + spin) * .6f));
            } else {
                const auto normal = track_.normal(z, kLeafSize);
                rotation =
                    multiply(rotateX(std::atan2(normal.z, normal.y)), multiply(rotateY(spin), rotateX(-kPi * .5f)));
            }
            if (z >= -65 && z <= 16) {
                const Vec3 position = {x + track_.offset(z), -.04f - track_.drop(z) + .018f + rise, z};
                leaf(position, rotation, kLeafSize * std::lerp(.75f, 1.2f, hashFloat(seed, 3)), opacity);
            }
        }
    }
    if (scene_.particles.size() > kMaxVisibleLeaves) {
        const auto distance = [&](const Quad& particle) {
            const auto& m = particle.model.values;
            const Vec3 delta = {m[12] - scene_.camera.position.x, m[13] - scene_.camera.position.y,
                                m[14] - scene_.camera.position.z};
            return dot(delta, delta);
        };
        std::stable_sort(scene_.particles.begin(), scene_.particles.end(),
                         [&](const auto& a, const auto& b) { return distance(a) < distance(b); });
        scene_.particles.resize(kMaxVisibleLeaves);
    }
    previousLeafTime_ = time;
    previousLeafScroll_ = scroll;
    previousLeafPlayerX_ = playerX;
}
} // namespace game
