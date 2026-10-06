#include "game/Simulation.h"
#include <algorithm>
#include <cmath>

namespace game {
using namespace GameConfig;
Simulation::Simulation(std::uint32_t seed) : randomEngine_(seed) {}
void Simulation::reset() {
    state_ = GameState{};
    spawnTimer_ = 1;
    scoreAccumulator_ = 0;
}
void Simulation::moveLane(int direction) {
    if (!state_.gameOver) {
        state_.targetLane = std::clamp(state_.targetLane + std::clamp(direction, -1, 1), 0, kLaneCount - 1);
    }
}
void Simulation::update(float deltaTime) {
    if (deltaTime <= 0 || !std::isfinite(deltaTime)) {
        return;
    }
    // Advance old bursts before detecting pickups, so new feedback starts at full brightness.
    updateCollectEffects(state_, deltaTime);
    if (state_.gameOver) {
        return;
    }

    state_.time += deltaTime;

    updatePlayer(deltaTime);
    updateWorld(deltaTime);
    updateEntities(deltaTime);
    resolveCollisions(state_);

    scoreAccumulator_ += state_.speed * deltaTime * 3.0f;
    const int distancePoints = static_cast<int>(scoreAccumulator_);
    if (distancePoints > 0) {
        state_.score += distancePoints;
        scoreAccumulator_ -= static_cast<float>(distancePoints);
    }
    state_.dayNight.update(state_.score, deltaTime);
}

void updateCollectEffects(GameState& state, float deltaTime) {
    if (deltaTime <= 0 || !std::isfinite(deltaTime)) {
        return;
    }
    for (auto& effect : state.collectEffects) {
        effect.age += deltaTime;
    }
    std::erase_if(state.collectEffects,
                  [](const CollectEffect& effect) { return effect.age >= kCollectBurstDuration; });
}

void Simulation::updatePlayer(float deltaTime) {
    const float targetX = laneToX(state_.targetLane);
    const float blend = std::min(1.0f, kLaneChangeSpeed * deltaTime);
    state_.playerX += (targetX - state_.playerX) * blend;
}

void Simulation::updateWorld(float deltaTime) {
    state_.roadScroll += state_.speed * deltaTime;

    spawnTimer_ -= deltaTime;
    if (spawnTimer_ <= 0.0f) {
        spawnEntity();

        std::uniform_real_distribution<float> delayDistribution(0.55f, 1.15f);
        spawnTimer_ = delayDistribution(randomEngine_);
    }
}

void Simulation::updateEntities(float deltaTime) {
    for (Entity& entity : state_.entities) {
        entity.z += state_.speed * deltaTime;
    }

    state_.entities.erase(std::remove_if(state_.entities.begin(), state_.entities.end(),
                                         [](const Entity& entity) { return entity.z > kDespawnZ; }),
                          state_.entities.end());
}

void Simulation::spawnEntity() {
    std::uniform_int_distribution<int> laneDistribution(0, kLaneCount - 1);
    std::uniform_real_distribution<float> typeDistribution(0.0f, 1.0f);

    const float typeRoll = typeDistribution(randomEngine_);
    EntityType type = EntityType::Obstacle;
    if (typeRoll > 0.78f) {
        type = EntityType::ScoreCollectible;
    } else if (typeRoll > 0.56f) {
        type = EntityType::SlowCollectible;
    }

    Entity entity;
    entity.type = type;
    entity.lane = laneDistribution(randomEngine_);
    entity.z = kSpawnZ;
    entity.spawnTime = state_.time;
    entity.textureIndex = (type == EntityType::ScoreCollectible || type == EntityType::SlowCollectible)
                              ? std::uniform_int_distribution<int>(0, 2)(randomEngine_)
                              : std::uniform_int_distribution<int>(0, 1)(randomEngine_);
    state_.entities.push_back(entity);
}

} // namespace game
