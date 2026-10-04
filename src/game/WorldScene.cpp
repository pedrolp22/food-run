#include "game/WorldScene.h"
#include "GameConfig.h"
#include "engine/math/Noise.h"
#include "game/GameState.h"
#include <algorithm>
#include <cmath>

namespace game {
using namespace engine::math;
using namespace engine::graphics;
using namespace GameConfig;
namespace {
constexpr float laneWidth = 1.85f;
constexpr float roadWidth = (kLanePositions[kLaneCount - 1] - kLanePositions[0]) + laneWidth;
constexpr float roadHalfEdge = roadWidth * 0.5f;
constexpr float sidewalkHalfWidth = roadHalfEdge + 1.1f;
constexpr float sidewalkTopY = 0.04f;
constexpr int kRoadSubdivisions = 6;
constexpr float subLength = kLaneSegmentLength / kRoadSubdivisions;

} // namespace
WorldScene::WorldScene(const GameAssets& assets)
    : assets_(assets), track_({kWorldCurvature, kRoadSway, -5, 24, kLaneStartZ}) {
    scene_.camera.position = {0, kCameraHeight, kCameraZ};
    scene_.camera.target = {0, kCameraTargetHeight, kCameraTargetZ};
    scene_.camera.verticalFov = kCameraVerticalFov;
    scene_.camera.fisheyeStrength = kCameraFisheyeStrength;
    scene_.light.direction = {-0.7f, 1.4f, -0.9f};
    scene_.light.shadow.center = {0, 4, -32};
    scene_.objects.reserve(1000);
    scene_.effects.reserve(144);
}
const Scene& WorldScene::build(float playerX, float roadScroll, float gameTime, std::span<const Entity> entities,
                               std::span<const CollectEffect> effects, float effectRemainder) {
    scene_.time = gameTime;
    scene_.objects.clear();
    scene_.effects.clear();
    addGround(roadScroll);
    addBuildings(roadScroll);
    addSidewalk(roadScroll);
    addCube(
        {playerX + track_.offset(kPlayerZ), kPlayerScale * 0.5f, kPlayerZ, kPlayerScale, kPlayerScale, kPlayerScale},
        {0.12f, 0.55f, 1.0f});
    addDecor(roadScroll);
    addEntities(entities);
    for (const auto& effect : effects) {
        addEffect(effect, effectRemainder);
    }
    return scene_;
}

void WorldScene::addGround(float roadScroll) {
    constexpr Color groundColor = {0.36f, 0.35f, 0.33f};
    constexpr float groundDepth = 11;
    const float start = kLaneStartZ + std::fmod(roadScroll, kLaneSegmentLength);
    for (float sign : {-1.0f, 1.0f}) {
        const float x = sign * (sidewalkHalfWidth + groundDepth * 0.5f);
        for (int segment = 0; segment < kLaneSegmentCount; ++segment) {
            for (int sub = 0; sub < kRoadSubdivisions; ++sub) {
                const float z = start + segment * kLaneSegmentLength + (sub + 0.5f) * subLength;
                addColoredQuad(groundColor,
                               {x + track_.offset(z), -0.06f - track_.drop(z), z, groundDepth, subLength, 1}, Face::Top,
                               {0, 1, 0}, 1, true, true);
            }
        }
    }
    const Mat4 rotationScale = multiply(rotateX(kPi * 0.5f), scale({roadWidth, subLength, 1}));
    for (int segment = 0; segment < kLaneSegmentCount; ++segment) {
        for (int sub = 0; sub < kRoadSubdivisions; ++sub) {
            const float z = start + segment * kLaneSegmentLength + (sub + 0.5f) * subLength;
            Quad command;
            command.model = multiply(translation({track_.offset(z), -0.04f - track_.drop(z), z}), rotationScale);
            track_.bendSegment(command.model, z, subLength, 1);
            command.texture = &assets_.texture(TextureId::Road);
            command.pbrSurface = true;
            command.surfaceMaterial = {1, 0, 1}; // The map separates dry aggregate, tar and road paint.
            command.materialTextures = assets_.materialTextures(TextureId::Road);
            command.textureScale = {1, 1.0f / kRoadSubdivisions};
            command.textureOffset = {0, static_cast<float>(segment) + static_cast<float>(sub) / kRoadSubdivisions};
            command.worldSize = {roadWidth, subLength};
            command.normal = track_.normal(z, subLength);
            command.heat = std::clamp((9.0f - z - 40.0f) / 50.0f, 0.0f, 1.0f);
            command.castsShadow = false;
            scene_.objects.emplace_back(command);
        }
    }
}

void WorldScene::addBuildings(float roadScroll) {
    constexpr int kBuildingsPerSide = 4;
    constexpr float buildingSides[] = {-1.0f, 1.0f};
    constexpr float kSkyscraperMinHeight = 9.0f;
    constexpr float kCellWidth = 4.4f; // Leave room for the widest tower instead of intersecting its neighbor.
    constexpr float kCellStartX = 4.5f;

    struct Building {
        float x;
        float cy;
        float z;
        float w;
        float h;
        float d;
        const Texture* texture;
        float fade;
    };

    std::vector<Building> buildings;
    buildings.reserve(static_cast<std::size_t>(kLaneSegmentCount * 2 * kBuildingsPerSide));

    constexpr float kBuildingSpacing = 12.0f;
    constexpr float kBuildingNearZ = -100.0f;
    constexpr float kBuildingFarZ = 12.0f;

    for (float side : buildingSides) {
        const unsigned int sideIndex = side < 0.0f ? 0u : 1u;

        for (int b = 0; b < kBuildingsPerSide; ++b) {
            const int firstI = static_cast<int>(std::floor((roadScroll - kBuildingFarZ) / kBuildingSpacing));
            const int lastI = static_cast<int>(std::ceil((roadScroll - kBuildingNearZ) / kBuildingSpacing));

            for (int i = firstI; i <= lastI; ++i) {
                const unsigned int seed =
                    static_cast<unsigned int>(i) * 97u + sideIndex * 131u + static_cast<unsigned int>(b) * 353u;

                const float baseWidth = 1.6f + hashFloat(seed, 0) * 0.7f;
                const float roll = hashFloat(seed, 1);
                float h;
                float wFactor;
                if (roll < 0.18f) {
                    h = 2.2f + hashFloat(seed, 5) * 1.5f;
                    wFactor = 1.0f;
                } else if (roll < 0.55f) {
                    h = 5.0f + hashFloat(seed, 5) * 3.5f;
                    wFactor = 1.15f;
                } else if (roll < 0.85f) {
                    h = 11.0f + hashFloat(seed, 5) * 4.0f;
                    wFactor = 1.45f;
                } else {
                    h = 17.0f + hashFloat(seed, 5) * 8.0f;
                    wFactor = 1.75f;
                }
                const float w = baseWidth * wFactor;
                const float depthFactor = 1.0f + (wFactor - 1.0f) * 0.8f;
                const float d = (2.5f + hashFloat(seed, 2) * 4.5f) * depthFactor;

                const float cellCenterX = kCellStartX + (static_cast<float>(b) + 0.5f) * kCellWidth;
                const float xJitterMax = std::max(0.05f, (kCellWidth - w) * 0.5f - 0.1f);
                const float zJitterMax = std::max(0.0f, 5.5f - d * 0.5f);
                const float bz = roadScroll - (static_cast<float>(i) * kBuildingSpacing +
                                               (hashFloat(seed, 4) - 0.5f) * 2.0f * zJitterMax);
                const float bx =
                    side * (cellCenterX + (hashFloat(seed, 3) - 0.5f) * 2.0f * xJitterMax) + track_.offset(bz);

                const float fadeInStart = kBuildingNearZ;
                const float fadeRange = 42.0f;
                const float fade = std::max(0.0f, std::min(1.0f, (bz - fadeInStart) / fadeRange));

                buildings.push_back({bx, h * 0.5f - 0.08f - track_.drop(bz), bz, w, h, d,
                                     h >= kSkyscraperMinHeight ? &assets_.texture(TextureId::Skyscraper)
                                                               : &assets_.texture(TextureId::Wall),
                                     fade});
            }
        }
    }

    for (const Building& building : buildings) {
        if (building.fade <= 0.0f) {
            continue;
        }
        const bool skyscraperWindows = building.texture == &assets_.texture(TextureId::Skyscraper);
        // The skyscraper image is a complete facade, mapped once onto each side.
        // Brick walls retain their fixed world-size tiles.
        constexpr float tileWidth = 2.8f;
        const float tileHeight = tileWidth / building.texture->aspect();
        const float frontRepeat = skyscraperWindows ? 1.0f : building.w / tileWidth;
        const float sideRepeat = skyscraperWindows ? 1.0f : building.d / tileWidth;
        const float verticalRepeat = skyscraperWindows ? 1.0f : building.h / tileHeight;
        addTexturedQuad(*building.texture,
                        {building.x, building.cy, building.z + building.d * 0.5f, building.w, building.h, 1.0f},
                        Face::FrontBack, frontRepeat, verticalRepeat, {0, 0, 1}, building.fade, skyscraperWindows, true);
        addTexturedQuad(*building.texture,
                        {building.x, building.cy, building.z - building.d * 0.5f, building.w, building.h, 1.0f},
                        Face::FrontBack, frontRepeat, verticalRepeat, {0, 0, -1}, building.fade, skyscraperWindows, true);
        addTexturedQuad(*building.texture,
                        {building.x + building.w * 0.5f, building.cy, building.z, building.d, building.h, 1.0f},
                        Face::Side, sideRepeat, verticalRepeat, {1, 0, 0}, building.fade, skyscraperWindows, true);
        addTexturedQuad(*building.texture,
                        {building.x - building.w * 0.5f, building.cy, building.z, building.d, building.h, 1.0f},
                        Face::Side, sideRepeat, verticalRepeat, {-1, 0, 0}, building.fade, skyscraperWindows, true);
    }

    for (const Building& building : buildings) {
        if (building.fade <= 0.0f) {
            continue;
        }
        addTexturedQuad(
            assets_.texture(TextureId::BuildingTop),
            {building.x, building.h - 0.08f - track_.drop(building.z), building.z, building.w, building.d, 1.0f},
            Face::Top, 1.0f, 1.0f, {0, 1, 0}, building.fade, false, true);
    }
}

void WorldScene::addSidewalk(float roadScroll) {
    const float scrollOffset = std::fmod(roadScroll, kLaneSegmentLength);
    constexpr Color curbColor = {0.34f, 0.34f, 0.37f};
    constexpr float roadSurfaceY = -0.04f;
    constexpr float sidewalkWidth = 1.1f;
    constexpr float sidewalkSigns[] = {-1.0f, 1.0f};
    constexpr float kCurbThickness = 0.04f;
    const auto& sidewalkTexture = assets_.texture(TextureId::Sidewalk);
    const float repeatLength = sidewalkWidth / sidewalkTexture.aspect();
    const Mat4 rotationScale = multiply(rotateX(kPi * 0.5f), scale({sidewalkWidth, subLength, 1}));

    for (int segment = 0; segment < kLaneSegmentCount; ++segment) {
        const float segmentZ = kLaneStartZ + scrollOffset + static_cast<float>(segment) * kLaneSegmentLength;

        for (int sub = 0; sub < kRoadSubdivisions; ++sub) {
            const float centerZ = segmentZ + (static_cast<float>(sub) + 0.5f) * subLength;

            for (float sideSign : sidewalkSigns) {
                const float centerX = sideSign * (roadHalfEdge + sidewalkHalfWidth) * 0.5f + track_.offset(centerZ);
                Quad command;
                command.model =
                    multiply(translation({centerX, sidewalkTopY - track_.drop(centerZ), centerZ}), rotationScale);
                track_.bendSegment(command.model, centerZ, subLength, 1);
                command.texture = &sidewalkTexture;
                command.pbrSurface = true;
                command.surfaceMaterial = {1, 0, 1}; // Cement, dirt and aggregate have separate mapped finishes.
                command.materialTextures = assets_.materialTextures(TextureId::Sidewalk);
                command.psxMinificationLodBias = kPsxSidewalkLodBias;
                command.normal = track_.normal(centerZ, subLength);
                command.worldSize = {sidewalkWidth, subLength};
                command.textureScale = {1, subLength / repeatLength};
                // Anchor UVs to the moving track, including when recycled segments wrap.
                const float startV = (centerZ - subLength * 0.5f - roadScroll) / repeatLength;
                command.textureOffset = {0, startV - std::floor(startV)};
                scene_.objects.emplace_back(command);
            }
        }
    }

    {
        const float curbHeight = sidewalkTopY - roadSurfaceY;
        for (int segment = 0; segment < kLaneSegmentCount; ++segment) {
            const float segmentZ = kLaneStartZ + scrollOffset + static_cast<float>(segment) * kLaneSegmentLength;
            for (int sub = 0; sub < kRoadSubdivisions; ++sub) {
                const float centerZ = segmentZ + (static_cast<float>(sub) + 0.5f) * subLength;
                for (float sideSign : sidewalkSigns) {
                    const float curbX = sideSign * roadHalfEdge + track_.offset(centerZ);
                    const float curbY = (roadSurfaceY + sidewalkTopY) * 0.5f - track_.drop(centerZ);
                    addCube({curbX, curbY, centerZ, kCurbThickness, curbHeight, subLength}, curbColor, true);
                }
            }
        }
    }
}

void WorldScene::addDecor(float roadScroll) {
    const float decorCenterX = (roadHalfEdge + sidewalkHalfWidth) * 0.5f;
    const float sidewalkHalfWidthActual = sidewalkHalfWidth - roadHalfEdge;
    const float benchOffset = sidewalkHalfWidthActual * 0.28f;
    const float trashOffset = sidewalkHalfWidthActual * 0.02f;

    constexpr float kDecorSpacing = 8.0f;
    constexpr float kDecorNearZ = -86.0f;
    constexpr float kDecorFarZ = 12.0f;
    const int decorFirstI = static_cast<int>(std::floor((roadScroll - kDecorFarZ) / kDecorSpacing));
    const int decorLastI = static_cast<int>(std::ceil((roadScroll - kDecorNearZ) / kDecorSpacing));

    for (int i = decorFirstI; i <= decorLastI; ++i) {
        const unsigned int seed = static_cast<unsigned int>(i) * 61u;
        if (hashFloat(seed, 0) > 0.6f) {
            continue;
        }

        const float zJitter = (hashFloat(seed, 1) - 0.5f) * 2.0f;
        const float propZ = roadScroll - static_cast<float>(i) * kDecorSpacing + zJitter;
        const float sideSign = hashFloat(seed, 2) < 0.5f ? -1.0f : 1.0f;
        const bool isBench = hashFloat(seed, 4) < 0.45f;
        const float xJitter = (hashFloat(seed, 3) - 0.5f) * 0.2f;
        const float offset = isBench ? benchOffset : trashOffset;
        const float propX = sideSign * (decorCenterX + offset + xJitter) + track_.offset(propZ);
        const float propY = sidewalkTopY - track_.drop(propZ);

        if (isBench) {
            addModel(assets_.model(ModelId::Bench), {propX, propY, propZ, 1.0f, 1.0f, 1.0f});
        } else {
            addModel(assets_.model(ModelId::TrashCan), {propX, propY, propZ, 1.6f, 1.6f, 1.6f});
        }
    }
}

void WorldScene::addEntities(std::span<const Entity> entities) {
    for (const Entity& entity : entities) {
        if (entity.type == EntityType::Obstacle) {
            const Model& model =
                (entity.textureIndex % 2 == 0) ? assets_.model(ModelId::Cone) : assets_.model(ModelId::Barrier);
            const bool isCone = (&model == &assets_.model(ModelId::Cone));
            const float obstacleX = laneToX(entity.lane) + track_.offset(entity.z);
            const float obstacleY = -track_.drop(entity.z);
            if (isCone) {
                addModel(model, {obstacleX, obstacleY, entity.z, 1.0f, 1.0f, 1.0f});
            } else {
                addModel(model, {obstacleX, obstacleY, entity.z, 0.75f * 0.9f, 0.9f, 0.9f});
            }
        }
    }

    for (const Entity& entity : entities) {
        if (entity.type != EntityType::Obstacle) {
            scene_.objects.emplace_back(makeCollectibleSprite(entity, scene_.time));
        }
    }
}

void WorldScene::addEffect(const CollectEffect& effect, float effectRemainder) {
    if (effect.type == EntityType::Obstacle || scene_.time < effect.time) {
        return;
    }
    // World time covers timestamp-only callers. Explicit age also advances after game over;
    // the fractional timestep keeps the burst smooth between simulation updates.
    const float age = std::max(scene_.time - effect.time, effect.age + std::max(effectRemainder, 0.0f));
    if (age >= kCollectBurstDuration) {
        return;
    }
    const Entity collected{effect.type, effect.lane, effect.z, effect.spawnTime, effect.textureIndex};
    const auto sprite = makeCollectibleSprite(collected, effect.time);
    SpriteBurst burst;
    burst.model = sprite.model;
    burst.shape = sprite.ripple.shape;
    burst.color = sprite.rippleColor;
    burst.progress = age / kCollectBurstDuration;
    burst.initialPhase =
        std::fmod(std::max(effect.time - effect.spawnTime, 0.0f), kCollectibleRipplePeriod) / kCollectibleRipplePeriod;
    burst.widthPixels = kCollectBurstWidthPixels;
    burst.opacity = kCollectBurstOpacity;
    scene_.effects.emplace_back(burst);
}

} // namespace game
