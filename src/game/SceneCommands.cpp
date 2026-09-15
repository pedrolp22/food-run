#include "GameConfig.h"
#include "game/GameState.h"
#include "game/WorldScene.h"
#include <cmath>

namespace game {
using namespace engine::math;
using namespace engine::graphics;
void WorldScene::addCube(const Transform& transform, Color color, bool followsGround) {
    Mat4 model = multiply(translation({transform.x, transform.y, transform.z}),
                          scale({transform.scaleX, transform.scaleY, transform.scaleZ}));
    if (followsGround) {
        track_.bendSegment(model, transform.z, transform.scaleZ, 2);
    }
    scene_.objects.emplace_back(Cube{model, color});
}
void WorldScene::addModel(const Model& model, const Transform& transform) {
    scene_.objects.emplace_back(ModelInstance{multiply(translation({transform.x, transform.y, transform.z}),
                                                       scale({transform.scaleX, transform.scaleY, transform.scaleZ})),
                                              &model});
}
void WorldScene::addTexturedQuad(const Texture& texture, const Transform& transform, Face face, float texScaleU,
                                 float texScaleV, Vec3 normal, float alpha) {
    const Mat4 rotation = face == Face::Side  ? rotateY(kPi * 0.5f)
                          : face == Face::Top ? rotateX(kPi * 0.5f)
                                              : identity();
    Quad command;
    command.model = multiply(translation({transform.x, transform.y, transform.z}),
                             multiply(rotation, scale({transform.scaleX, transform.scaleY, transform.scaleZ})));
    command.texture = &texture;
    command.normal = normal;
    command.textureScale = {texScaleU, texScaleV};
    command.worldSize = {transform.scaleX, transform.scaleY};
    command.opacity = alpha;
    scene_.objects.emplace_back(command);
}
void WorldScene::addColoredQuad(Color color, const Transform& transform, Face face, Vec3 normal, float alpha,
                                bool receivesShadow, bool followsGround) {
    const Mat4 rotation = face == Face::Side  ? rotateY(kPi * 0.5f)
                          : face == Face::Top ? rotateX(kPi * 0.5f)
                                              : identity();
    Quad command;
    command.model = multiply(translation({transform.x, transform.y, transform.z}),
                             multiply(rotation, scale({transform.scaleX, transform.scaleY, transform.scaleZ})));
    command.normal = normal;
    if (followsGround) {
        track_.bendSegment(command.model, transform.z, transform.scaleY, 1);
        command.normal = track_.normal(transform.z, transform.scaleY);
    }
    command.color = color;
    command.opacity = alpha;
    command.receivesShadow = receivesShadow;
    if (receivesShadow) {
        scene_.objects.emplace_back(command);
    } else {
        scene_.effects.push_back(command);
    }
}
Sprite WorldScene::makeCollectibleSprite(const Entity& entity, float animationTime) const {
    const auto& texture = assets_.collectible(entity);
    const float aspect = texture.aspect();
    const Transform transform{
        laneToX(entity.lane) + track_.offset(entity.z), .82f - track_.drop(entity.z), entity.z, 1.5f, 1.5f, 1.5f};
    const Color edgeColor =
        entity.type == EntityType::SlowCollectible ? Color{.9f, .15f, .15f} : Color{.04f, .85f, .30f};
    const float width = transform.scaleX * (aspect < 1 ? aspect : 1);
    const float height = transform.scaleY / (aspect >= 1 ? aspect : 1);
    // Use a shared, constant-speed turn; spawn times must not stagger identical items' rotation.
    const float turn =
        2 * kPi * std::fmod(animationTime, GameConfig::kCollectibleSpinPeriod) / GameConfig::kCollectibleSpinPeriod;
    const float bob =
        2 * kPi * std::fmod(animationTime, GameConfig::kCollectibleBobPeriod) / GameConfig::kCollectibleBobPeriod;
    const float bobY = transform.y + std::sin(bob) * GameConfig::kCollectibleBobHeight;
    const Mat4 model =
        multiply(translation({transform.x, bobY, transform.z}),
                 multiply(rotateY(turn), scale({width, height, transform.scaleZ * GameConfig::kCollectibleDepth})));
    Sprite sprite{model, &texture, edgeColor, entity.spawnTime, width / height};
    sprite.profile = &assets_.collectibleMesh(entity);
    sprite.outlinePixels = GameConfig::kCollectibleOutlinePixels;
    sprite.ripple = {&assets_.collectibleOutline(entity), GameConfig::kCollectibleRipplePeriod,
                     GameConfig::kCollectibleRippleWidthPixels, GameConfig::kCollectibleRippleOpacity};
    return sprite;
}
} // namespace game
