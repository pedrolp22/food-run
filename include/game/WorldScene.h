#pragma once

#include "engine/graphics/Scene.h"
#include "engine/math/TrackCurve.h"
#include "game/GameAssets.h"
#include "game/PlayerCharacter.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <unordered_map>

namespace game {
// The menu and the first transition frame must use the same aspect-dependent camera.
engine::graphics::Camera menuCamera(float aspect);
// Builds game scenery once per frame. It never issues GL calls or owns a graphics context.
class WorldScene {
  public:
    explicit WorldScene(const GameAssets& assets);
    void setFoliageParticles(bool enabled);
    const engine::graphics::Scene& build(float playerX, float roadScroll, float gameTime,
                                         std::span<const Entity> entities, std::span<const CollectEffect> effects,
                                         float effectRemainder = 0, float nightBlend = 0, float playerSpeed = 0,
                                         float cameraBlend = 1, float aspect = 1);

  private:
    enum class Face { FrontBack, Side, Top };
    using Color = engine::graphics::Color;
    struct Transform {
        float x, y, z, scaleX, scaleY, scaleZ;
    };
    void addGround(float scroll);
    void addBuildings(float scroll);
    void addBrickFacade(const Transform& transform, Face face, engine::math::Vec3 normal, float fade, bool windows,
                        bool door, std::uint64_t reflectionId);
    void addSidewalk(float scroll);
    void addParks(float scroll);
    void addBrickGarden(float side, float facadeX, float centerZ, float depth, unsigned int seed);
    struct GrassLayout {
        std::function<bool(int, int)> occupied;
        std::vector<int> entrances;
        engine::math::Vec2 textureOffset;
    };
    void addGrassPatch(float side, float innerX, float startZ, int columns, int rows, unsigned int seed, bool notched,
                       bool fenced, bool entrance, std::optional<engine::math::Vec2> buildingWall = std::nullopt,
                       const GrassLayout* layout = nullptr);
    void addLeaves(float playerX, float scroll);
    struct TreeEmitter {
        std::uint64_t id;
        engine::math::Vec3 position;
        float height;
        unsigned int seed;
    };
    struct LiftedLeaf {
        float time;
        float baseZ;
        float speed;
    };
    std::vector<TreeEmitter> trees_;
    std::unordered_map<std::uint64_t, LiftedLeaf> liftedLeaves_;
    float previousLeafTime_ = -1;
    float previousLeafScroll_ = 0;
    float previousLeafPlayerX_ = 0;
    bool foliageParticles_ = true;
    void addDecor();
    void addStreetLamps(float scroll);
    std::vector<engine::math::Vec3> lampPosts_;
    struct SidewalkAccess {
        float side, z;
        unsigned int seed;
        bool park;
    };
    std::vector<SidewalkAccess> sidewalkAccess_;
    void addEntities(std::span<const Entity> entities);
    void addEffect(const CollectEffect& effect, float effectRemainder);
    void addCube(const Transform& transform, Color color, bool followsGround = false);
    void addModel(const engine::graphics::Model& model, const Transform& transform);
    engine::graphics::Quad& addTexturedQuad(const engine::graphics::Texture& texture, const Transform& transform,
                                            Face face, float texScaleU, float texScaleV, engine::math::Vec3 normal,
                                            float alpha = 1, bool skyscraperWindows = false,
                                            bool buildingSurface = false);
    void addColoredQuad(Color color, const Transform& transform, Face face, engine::math::Vec3 normal, float alpha = 1,
                        bool receivesShadow = true, bool followsGround = false);
    engine::graphics::Sprite makeCollectibleSprite(const Entity& entity, float animationTime) const;
    const GameAssets& assets_;
    PlayerCharacter player_;
    engine::math::TrackCurve track_;
    engine::graphics::Scene scene_;
};
} // namespace game
