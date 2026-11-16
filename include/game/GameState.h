#pragma once

#include "GameConfig.h"
#include "game/DayNightCycle.h"
#include "game/Entities.h"
#include <vector>

namespace game {
struct GameState {
    bool gameOver = false;
    GameOverReason gameOverReason = GameOverReason::None;
    int targetLane = 1;
    float playerX = GameConfig::kLanePositions[1];
    float speed = GameConfig::kInitialSpeed;
    float roadScroll = 0;
    float time = 0;
    int score = 0;
    int healthyFoods = 0;
    int health = GameConfig::kMaxHealth;
    int purpleStreak = 0;
    DayNightCycle dayNight;
    std::vector<Entity> entities;
    std::vector<CollectEffect> collectEffects;
};
float laneToX(int lane);
// Deterministic domain operation, independent of rendering, input and wall-clock time.
void resolveCollisions(GameState& state);
// Transient feedback keeps advancing when gameplay is frozen.
void updateCollectEffects(GameState& state, float deltaTime);
} // namespace game
