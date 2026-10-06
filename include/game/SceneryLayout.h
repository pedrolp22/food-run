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
inline bool hasPocketPark(unsigned int lotSeed) {
    return engine::math::hashFloat(lotSeed, 10) >= GameConfig::kStreetBuildingChance &&
           engine::math::hashFloat(parkSeed(lotSeed), 0) < GameConfig::kPocketParkChance;
}
} // namespace game::scenery
