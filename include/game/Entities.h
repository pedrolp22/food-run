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
};
} // namespace game
