#include "game/WorldScene.h"
#include "GameConfig.h"
#include "engine/math/Noise.h"
#include "game/GameState.h"
#include "game/SceneryLayout.h"
#include <algorithm>
#include <cmath>

namespace game {
using namespace engine::math;
using namespace engine::graphics;
using namespace GameConfig;
using namespace scenery;
namespace {
constexpr int kRoadSubdivisions = 6;
constexpr float subLength = kLaneSegmentLength / kRoadSubdivisions;

} // namespace
WorldScene::WorldScene(const GameAssets& assets)
    : assets_(assets), track_({kWorldCurvature, kRoadSway, -5, 24, kLaneStartZ}) {
    scene_.camera.position = {0, kCameraHeight, kCameraZ};
    scene_.camera.target = {0, kCameraTargetHeight, kCameraTargetZ};
    scene_.camera.verticalFov = kCameraVerticalFov;
    scene_.camera.fisheyeStrength = kCameraFisheyeStrength;
    scene_.light.direction = {kDayLightDirection[0], kDayLightDirection[1], kDayLightDirection[2]};
    scene_.light.shadow.center = {0, 4, -32};
    const auto skyColor = [](const std::array<float, 3>& rgb) { return Color{rgb[0], rgb[1], rgb[2]}; };
    scene_.sky.top = skyColor(kDaySkyTop);
    scene_.sky.bottom = skyColor(kDaySkyHorizon);
    scene_.sky.dusk = {skyColor(kDuskSkyTop), skyColor(kDuskSkyHorizon), skyColor(kDuskCloudTint)};
    scene_.sky.night = {skyColor(kNightSkyTop), skyColor(kNightSkyHorizon), skyColor(kNightCloudTint)};
    constexpr TextureId clouds[] = {TextureId::Cloud1, TextureId::Cloud2, TextureId::Cloud3};
    for (std::size_t index = 0; index < scene_.sky.texturedClouds.size(); ++index) {
        scene_.sky.texturedClouds[index] = {&assets_.texture(clouds[index]),
                                            kCloudWidthsDegrees[index],
                                            kCloudElevationsDegrees[index],
                                            kCloudRepeatDegrees[index],
                                            kCloudScrollSpeeds[index],
                                            kCloudOffsets[index],
                                            kCloudOpacities[index],
                                            kCloudHorizontalScatter,
                                            kCloudElevationScatterDegrees,
                                            kCloudSizeVariation,
                                            kCloudCurvature,
                                            kCloudWarpStrength,
                                            kCloudWarpSpeed};
    }
    scene_.sky.layoutSeed = kSkyLayoutSeed;
    scene_.sky.stars = {kNightStarDensity, kNightStarSizeDegrees, kNightStarBrightness, kNightStarTwinkleSpeed,
                        kNightStarTwinkleStrength};
    scene_.sky.moon = {&assets_.texture(TextureId::Moon),
                       {kNightLightDirection[0], kNightLightDirection[1], kNightLightDirection[2]},
                       kMoonSizeDegrees,
                       kMoonBrightness,
                       kMoonGlowStrength};
    scene_.objects.reserve(2000);
    scene_.effects.reserve(144);
    scene_.particles.reserve(kMaxVisibleLeaves);
    trees_.reserve(24);
}
const Scene& WorldScene::build(float playerX, float roadScroll, float gameTime, std::span<const Entity> entities,
                               std::span<const CollectEffect> effects, float effectRemainder, float nightBlend) {
    scene_.time = gameTime;
    scene_.sky.nightBlend = std::isfinite(nightBlend) ? std::clamp(nightBlend, 0.0f, 1.0f) : 0;
    const float phase = scene_.sky.nightBlend;
    const auto& from = phase < .5f ? kDayLightDirection : kDuskLightDirection;
    const auto& to = phase < .5f ? kDuskLightDirection : kNightLightDirection;
    const Vec3 fromDirection = normalize({from[0], from[1], from[2]});
    const Vec3 toDirection = normalize({to[0], to[1], to[2]});
    const float lightBlend = phase < .5f ? phase * 2 : phase * 2 - 1;
    scene_.light.direction = normalize({std::lerp(fromDirection.x, toDirection.x, lightBlend),
                                        std::lerp(fromDirection.y, toDirection.y, lightBlend),
                                        std::lerp(fromDirection.z, toDirection.z, lightBlend)});
    scene_.light.ambient = phase < .5f ? std::lerp(kSkyAmbientLight[0], kSkyAmbientLight[1], phase * 2)
                                       : std::lerp(kSkyAmbientLight[1], kSkyAmbientLight[2], phase * 2 - 1);
    scene_.sky.sunColor = scene_.sky.palette().cloudTint;
    if (phase > .5f) {
        const float t = phase * 2 - 1;
        scene_.sky.sunColor = {std::lerp(scene_.sky.dusk.cloudTint.r, kNightLightColor[0], t),
                               std::lerp(scene_.sky.dusk.cloudTint.g, kNightLightColor[1], t),
                               std::lerp(scene_.sky.dusk.cloudTint.b, kNightLightColor[2], t)};
    }
    scene_.objects.clear();
    scene_.effects.clear();
    scene_.particles.clear();
    trees_.clear();
    addGround(roadScroll);
    addBuildings(roadScroll);
    addSidewalk(roadScroll);
    addParks(roadScroll);
    addLeaves(playerX, roadScroll);
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
    static_assert(kBrickBuildingWidthTiles > 0 && kBrickBuildingDepthTiles > 0);
    static_assert(kBrickBuildingMinFloors >= 2 && kBrickBuildingMaxFloors >= kBrickBuildingMinFloors);
    static_assert(kBrickBuildingGap > 0 && kBrickBuildingSetback >= 0 && kBrickBuildingRoofBandHeight >= 0);
    static_assert(kStreetBuildingChance >= 0 && kStreetBuildingChance <= 1);
    static_assert(kStreetNearSidewalkChance >= 0 && kStreetNearSidewalkChance <= 1);
    static_assert(kStreetBrickChance >= 0 && kStreetBrickChance <= 1);
    static_assert(kStreetExtraSetbackMin >= 0 && kStreetExtraSetbackMax >= kStreetExtraSetbackMin);
    constexpr int kBuildingsPerSide = 4;
    constexpr float buildingSides[] = {-1.0f, 1.0f};
    const float brickFloorHeight = kBrickFacadeTileWidth / assets_.texture(TextureId::BrickWindow).aspect();
    constexpr int floorVariants = kBrickBuildingMaxFloors - kBrickBuildingMinFloors + 1;
    constexpr float kCellWidth = buildingCellWidth; // Separate both brick buildings and towers.
    constexpr float kCellStartX = buildingCellStartX;

    struct Building {
        float x;
        float cy;
        float z;
        float w;
        float h;
        float d;
        const Texture* texture;
        float fade;
        float side;
        bool streetAccess;
        unsigned int seed;
        std::uint64_t reflectionId;
    };

    std::vector<Building> buildings;
    buildings.reserve(static_cast<std::size_t>(kLaneSegmentCount * 2 * kBuildingsPerSide));

    constexpr float kBuildingSpacing = buildingSpacing;
    constexpr float kBuildingNearZ = -100.0f;
    constexpr float kBuildingFarZ = 12.0f;

    for (float side : buildingSides) {
        const unsigned int sideIndex = side < 0.0f ? 0u : 1u;

        for (int b = 0; b < kBuildingsPerSide; ++b) {
            const int firstI = static_cast<int>(std::floor((roadScroll - kBuildingFarZ) / kBuildingSpacing));
            const int lastI = static_cast<int>(std::ceil((roadScroll - kBuildingNearZ) / kBuildingSpacing));

            for (int i = firstI; i <= lastI; ++i) {
                const unsigned int lotSeed = streetLotSeed(i, side);
                const bool streetBuilding = hashFloat(lotSeed, 10) < kStreetBuildingChance;
                if ((b == 0 && !streetBuilding) || (b < kParkBuildingCells && hasPocketPark(lotSeed))) {
                    continue;
                }
                const bool nearSidewalk = hashFloat(lotSeed, 11) < kStreetNearSidewalkChance;
                // Recess the whole lot, including its background columns, to keep every footprint clear.
                const float extraSetback =
                    streetBuilding && !nearSidewalk
                        ? std::lerp(kStreetExtraSetbackMin, kStreetExtraSetbackMax, hashFloat(lotSeed, 12))
                        : 0;
                const unsigned int seed =
                    static_cast<unsigned int>(i) * 97u + sideIndex * 131u + static_cast<unsigned int>(b) * 353u;

                const float baseWidth = 1.6f + hashFloat(seed, 0) * 0.7f;
                const float roll = hashFloat(seed, 1);
                const bool brick = roll < (b == 0 ? kStreetBrickChance : .55f);
                float h;
                float wFactor;
                if (brick) {
                    const int floorIndex =
                        (i % floorVariants + floorVariants + static_cast<int>(sideIndex) + b) % floorVariants;
                    h = (kBrickBuildingMinFloors + floorIndex) * brickFloorHeight + kBrickBuildingRoofBandHeight;
                    wFactor = 1;
                } else if (roll < 0.85f) {
                    h = 11.0f + hashFloat(seed, 5) * 4.0f;
                    wFactor = 1.45f;
                } else {
                    h = 17.0f + hashFloat(seed, 5) * 8.0f;
                    wFactor = 1.75f;
                }
                const float w = brick ? brickWidth : baseWidth * wFactor;
                const float depthFactor = 1.0f + (wFactor - 1.0f) * 0.8f;
                const float d = brick ? brickDepth : (2.5f + hashFloat(seed, 2) * 4.5f) * depthFactor;

                const float cellCenterX = kCellStartX + (static_cast<float>(b) + 0.5f) * kCellWidth + extraSetback;
                const float xJitterMax = std::max(0.05f, (kCellWidth - w) * 0.5f - 0.1f);
                const float zJitterMax = std::max(0.0f, 5.5f - d * 0.5f);
                const float zJitter = brick ? 0 : (hashFloat(seed, 4) - .5f) * 2 * zJitterMax;
                const float xJitter = brick ? 0 : (hashFloat(seed, 3) - .5f) * 2 * xJitterMax;
                const float bz = roadScroll - (static_cast<float>(i) * kBuildingSpacing + zJitter);
                const float bx = side * (cellCenterX + xJitter) + track_.offset(bz);

                const float fadeInStart = kBuildingNearZ;
                const float fadeRange = 42.0f;
                const float fade = std::max(0.0f, std::min(1.0f, (bz - fadeInStart) / fadeRange));

                buildings.push_back(
                    {bx, h * 0.5f - 0.08f - track_.drop(bz), bz, w, h, d,
                     brick ? &assets_.texture(TextureId::BrickWall) : &assets_.texture(TextureId::Skyscraper), fade,
                     side, b == 0, lotSeed,
                     (static_cast<std::uint64_t>(static_cast<std::uint32_t>(i)) << 32) |
                         (static_cast<std::uint64_t>(sideIndex) << 16) | (static_cast<std::uint64_t>(b) << 8)});
            }
        }
    }

    for (const Building& building : buildings) {
        if (building.fade <= 0.0f) {
            continue;
        }
        const bool skyscraperWindows = building.texture == &assets_.texture(TextureId::Skyscraper);
        if (!skyscraperWindows) {
            if (building.streetAccess) {
                addBrickGarden(building.side,
                               building.side * (building.x - track_.offset(building.z)) - building.w * .5f, building.z,
                               building.d, building.seed);
            }
            addBrickFacade({building.x, building.cy, building.z + building.d * .5f, building.w, building.h, 1},
                           Face::FrontBack, {0, 0, 1}, building.fade, true, false, building.reflectionId | 1);
            addBrickFacade({building.x, building.cy, building.z - building.d * .5f, building.w, building.h, 1},
                           Face::FrontBack, {0, 0, -1}, building.fade, false, false, building.reflectionId | 2);
            const bool rightFacesStreet = building.side < 0;
            addBrickFacade({building.x + building.w * .5f, building.cy, building.z, building.d, building.h, 1},
                           Face::Side, {1, 0, 0}, building.fade, rightFacesStreet,
                           rightFacesStreet && building.streetAccess, building.reflectionId | 3);
            addBrickFacade({building.x - building.w * .5f, building.cy, building.z, building.d, building.h, 1},
                           Face::Side, {-1, 0, 0}, building.fade, !rightFacesStreet,
                           !rightFacesStreet && building.streetAccess, building.reflectionId | 4);
            continue;
        }
        // A skyscraper image contains its complete facade; brick facades use whole tiles below.
        addTexturedQuad(*building.texture,
                        {building.x, building.cy, building.z + building.d * 0.5f, building.w, building.h, 1.0f},
                        Face::FrontBack, 1, 1, {0, 0, 1}, building.fade, true, true)
            .reflectionId = building.reflectionId | 1;
        addTexturedQuad(*building.texture,
                        {building.x, building.cy, building.z - building.d * 0.5f, building.w, building.h, 1.0f},
                        Face::FrontBack, 1, 1, {0, 0, -1}, building.fade, true, true)
            .reflectionId = building.reflectionId | 2;
        addTexturedQuad(*building.texture,
                        {building.x + building.w * 0.5f, building.cy, building.z, building.d, building.h, 1.0f},
                        Face::Side, 1, 1, {1, 0, 0}, building.fade, true, true)
            .reflectionId = building.reflectionId | 3;
        addTexturedQuad(*building.texture,
                        {building.x - building.w * 0.5f, building.cy, building.z, building.d, building.h, 1.0f},
                        Face::Side, 1, 1, {-1, 0, 0}, building.fade, true, true)
            .reflectionId = building.reflectionId | 4;
    }

    for (const Building& building : buildings) {
        if (building.fade <= 0.0f) {
            continue;
        }
        addTexturedQuad(
            assets_.texture(TextureId::BuildingTop),
            {building.x, building.h - 0.08f - track_.drop(building.z), building.z, building.w, building.d, 1.0f},
            Face::Top, 1.0f, 1.0f, {0, 1, 0}, building.fade, false, true)
            .reflectionGroupId = building.reflectionId | 128;
    }
    for (auto& object : scene_.objects) {
        if (auto* surface = std::get_if<Quad>(&object); surface && surface->reflectionId) {
            surface->reflectionGroupId = (surface->reflectionId & ~std::uint64_t{255}) | 128;
        }
    }
}

void WorldScene::addBrickFacade(const Transform& transform, Face face, Vec3 normal, float fade, bool windows, bool door,
                                std::uint64_t reflectionId) {
    static_assert(kBrickFacadeTileWidth > 0);
    const float aspect = assets_.texture(TextureId::BrickWindow).aspect();
    // Fit at least one complete source tile even on the smallest single-story building.
    const float tileWidth = std::min({kBrickFacadeTileWidth, transform.scaleX, transform.scaleY * aspect});
    const float tileHeight = tileWidth / aspect;
    const int columns = std::max(1, static_cast<int>(std::floor(transform.scaleX / tileWidth + .0001f)));
    const int rows = std::max(1, static_cast<int>(std::floor(transform.scaleY / tileHeight + .0001f)));
    const float panelWidth = std::min(transform.scaleX, columns * tileWidth);
    const float baseY = transform.y - transform.scaleY * .5f;
    const auto patch = [&](TextureId id, float left, float bottom, float width, float height) {
        if (width <= .0001f || height <= .0001f) {
            return;
        }
        Transform part = transform;
        const float horizontal = left + width * .5f;
        if (face == Face::Side) {
            part.z -= horizontal; // rotateY(+90) maps local +X to world -Z.
        } else {
            part.x += horizontal;
        }
        part.y = baseY + bottom + height * .5f;
        part.scaleX = width;
        part.scaleY = height;
        auto& quad = addTexturedQuad(assets_.texture(id), part, face, width / tileWidth, height / tileHeight, normal,
                                     fade, false, true);
        if (id == TextureId::BrickWall) {
            quad.textureOffset = {(left + panelWidth * .5f) / tileWidth, bottom / tileHeight};
        }
        quad.facadeVerticalRange = {bottom / transform.scaleY, (bottom + height) / transform.scaleY};
        quad.reflectionId = reflectionId;
    };
    const float halfWidth = transform.scaleX * .5f;
    if (!windows) {
        patch(TextureId::BrickWall, -halfWidth, 0, transform.scaleX, transform.scaleY);
        return;
    }
    const float firstWindowY = door ? tileHeight : 0;
    patch(TextureId::BrickWindow, -panelWidth * .5f, firstWindowY, panelWidth, rows * tileHeight - firstWindowY);
    if (door) {
        // Only an accessible street-facing facade gets an entrance, never a repeating door row.
        patch(TextureId::BrickDoor, -tileWidth * .5f, 0, tileWidth, tileHeight);
        const float flankWidth = (transform.scaleX - tileWidth) * .5f;
        patch(TextureId::BrickWall, -halfWidth, 0, flankWidth, tileHeight);
        patch(TextureId::BrickWall, tileWidth * .5f, 0, flankWidth, tileHeight);
    }
    const float margin = (transform.scaleX - panelWidth) * .5f;
    patch(TextureId::BrickWall, -halfWidth, firstWindowY, margin, transform.scaleY - firstWindowY);
    patch(TextureId::BrickWall, panelWidth * .5f, firstWindowY, margin, transform.scaleY - firstWindowY);
    patch(TextureId::BrickWall, -panelWidth * .5f, rows * tileHeight, panelWidth, transform.scaleY - rows * tileHeight);
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
