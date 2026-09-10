#pragma once

#include "game/GameState.h"
#include <cstdint>
#include <random>

namespace game {
class Simulation {
  public:
    explicit Simulation(std::uint32_t seed = std::random_device{}());
    void reset();
    void moveLane(int direction);
    void update(float deltaTime);
    const GameState& state() const { return state_; }

  private:
    void updatePlayer(float deltaTime);
    void updateWorld(float deltaTime);
    void updateEntities(float deltaTime);
    void spawnEntity();
    GameState state_;
    float spawnTimer_ = 1;
    float scoreAccumulator_ = 0;
    std::mt19937 randomEngine_;
};
} // namespace game
