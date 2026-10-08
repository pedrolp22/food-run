#pragma once

#include "GameConfig.h"
#include "engine/math/Noise.h"
#include <algorithm>

namespace game::scenery {
inline constexpr float laneWidth = 1.85f;
inline constexpr float roadWidth = GameConfig::kLanePositions.back() - GameConfig::kLanePositions.front() + laneWidth;
inline constexpr float roadHalfEdge = roadWidth * .5f;
inline constexpr float sidewalkHalfWidth = roadHalfEdge + 1.1f;
inline constexpr float sidewalkTopY = .04f;
inline constexpr float brickWidth = GameConfig::kBrickFacadeTileWidth * GameConfig::kBrickBuildingWidthTiles;
inline constexpr float brickDepth = GameConfig::kBrickFacadeTileWidth * GameConfig::kBrickBuildingDepthTiles;
inline constexpr float buildingCellWidth = std::max(4.4f, brickWidth + .8f);
inline constexpr float buildingCellStartX =
    sidewalkHalfWidth + GameConfig::kBrickBuildingSetback - (buildingCellWidth - brickWidth) * .5f;
inline constexpr float buildingSpacing = std::max(12.0f, brickDepth + GameConfig::kBrickBuildingGap);
inline unsigned int streetLotSeed(int lot, float side) {
    return static_cast<unsigned int>(lot) * 97u + (side < 0 ? 0u : 131u);
}
inline unsigned int parkSeed(unsigned int lotSeed) {
    return lotSeed ^ engine::math::hashSeed(GameConfig::kParkLayoutSeed);
}
inline unsigned int streetBlockSeed(int lot, float side) {
    static_assert(GameConfig::kStreetBlockLots >= 2);
    const int block = lot / GameConfig::kStreetBlockLots - (lot % GameConfig::kStreetBlockLots < 0 ? 1 : 0);
    return streetLotSeed(block, side) ^ engine::math::hashSeed(GameConfig::kStreetLayoutSeed);
}
inline bool hasStreetBuilding(int lot, float side) {
    const float chance = std::clamp(GameConfig::kStreetBuildingChance +
                                        (engine::math::hashFloat(streetBlockSeed(lot, side), 10) - .5f) * .5f,
                                    0.0f, 1.0f);
    return GameConfig::kStreetBuildingChance >= 1 ||
           (GameConfig::kStreetBuildingChance > 0 && engine::math::hashFloat(streetLotSeed(lot, side), 10) < chance);
}
inline bool streetBrick(int lot, float side, int column = 0) {
    const float base = column == 0 ? GameConfig::kStreetBrickChance : .55f;
    const float chance =
        std::clamp(base + (engine::math::hashFloat(streetBlockSeed(lot, side), 1) - .5f) * .7f, 0.0f, 1.0f);
    return base >= 1 ||
           (base > 0 &&
            engine::math::hashFloat(streetLotSeed(lot, side) + static_cast<unsigned int>(column) * 353u, 1) < chance);
}
inline bool streetNearSidewalk(int lot, float side) {
    return engine::math::hashFloat(streetBlockSeed(lot, side), 11) < GameConfig::kStreetNearSidewalkChance;
}
inline bool hasPocketPark(int lot, float side) {
    return !hasStreetBuilding(lot, side) &&
           engine::math::hashFloat(parkSeed(streetLotSeed(lot, side)), 0) < GameConfig::kPocketParkChance;
}
} // namespace game::scenery
