#include "game/GameState.h"
#include <algorithm>
#include <cmath>

namespace game {
using namespace GameConfig;
float laneToX(int lane) {
    const int clampedLane = std::clamp(lane, 0, kLaneCount - 1);
    return kLanePositions[static_cast<std::size_t>(clampedLane)];
}

void resolveCollisions(GameState& state) {
    for (auto entity = state.entities.begin(); entity != state.entities.end();) {
        const float entityX = laneToX(entity->lane);
        const bool touchesPlayer =
            std::abs(entity->z - kPlayerZ) <= kCollisionZRange && std::abs(entityX - state.playerX) <= kCollisionXRange;

        if (!touchesPlayer) {
            ++entity;
            continue;
        }

        switch (entity->type) {
        case EntityType::Obstacle:
            state.gameOver = true;
            ++entity;
            continue;
        case EntityType::SlowCollectible:
            state.purpleStreak += 1;
            state.health = std::max(0, state.health - 1);
            state.speed = std::max(kMinSpeed, state.speed - 1.8f);
            if (state.health == 0) {
                state.gameOver = true;
            }
            state.collectEffects.push_back(
                {entity->type, entity->lane, entity->z, state.time, entity->textureIndex, entity->spawnTime});
            break;
        case EntityType::ScoreCollectible:
            state.score += 100;
            state.healthyFoods += 1;
            state.health = std::min(kMaxHealth, state.health + 1);
            state.purpleStreak = std::max(0, state.purpleStreak - 1);
            state.speed = std::min(kMaxSpeed, state.speed + 1.8f);
            state.collectEffects.push_back(
                {entity->type, entity->lane, entity->z, state.time, entity->textureIndex, entity->spawnTime});
            break;
        }

        entity = state.entities.erase(entity);
    }
}

} // namespace game
