#pragma once

namespace game {
enum class EntityType { Obstacle, SlowCollectible, ScoreCollectible };
struct Entity {
    EntityType type = EntityType::Obstacle;
    int lane = 1;
    float z = 0;
    float spawnTime = 0;
    int textureIndex = 0;
};
struct CollectEffect {
    EntityType type = EntityType::ScoreCollectible;
    int lane = 1;
    float z = 0;
    float time = 0;
    int textureIndex = 0;
    float spawnTime = 0;
    float age = 0; // Advances independently of the frozen world after game over.
};
} // namespace game
