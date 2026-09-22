#pragma once

#include "engine/graphics/Scene.h"
#include "engine/math/TrackCurve.h"
#include "game/GameAssets.h"
#include <span>

namespace game {
// Builds game scenery once per frame. It never issues GL calls or owns a graphics context.
class WorldScene {
  public:
    explicit WorldScene(const GameAssets& assets);
    const engine::graphics::Scene& build(float playerX, float roadScroll, float gameTime, int score,
                                         std::span<const Entity> entities, std::span<const CollectEffect> effects,
                                         float effectRemainder = 0);

  private:
    enum class Face { FrontBack, Side, Top };
    using Color = engine::graphics::Color;
    struct Transform {
        float x, y, z, scaleX, scaleY, scaleZ;
    };
    void addGround(float scroll);
    void addBuildings(float scroll);
    void addSidewalk(float scroll);
    void addDecor(float scroll);
    void addEntities(std::span<const Entity> entities);
    void addEffect(const CollectEffect& effect, float effectRemainder);
    void addCube(const Transform& transform, Color color, bool followsGround = false);
    void addModel(const engine::graphics::Model& model, const Transform& transform);
    void addTexturedQuad(const engine::graphics::Texture& texture, const Transform& transform, Face face,
                         float texScaleU, float texScaleV, engine::math::Vec3 normal, float alpha = 1);
    void addColoredQuad(Color color, const Transform& transform, Face face, engine::math::Vec3 normal, float alpha = 1,
                        bool receivesShadow = true, bool followsGround = false);
    engine::graphics::Sprite makeCollectibleSprite(const Entity& entity, float animationTime) const;
    const GameAssets& assets_;
    engine::math::TrackCurve track_;
    engine::graphics::Scene scene_;
    float nightTransitionStart_ = -1;
};
} // namespace game
