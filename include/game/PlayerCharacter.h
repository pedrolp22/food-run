#pragma once

#include "engine/assets/ModelPose.h"
#include "game/Entities.h"
#include <array>
#include <vector>

namespace game {
// Game-specific rig mapping and procedural gait; the importer and renderer know no cat bone names.
void preparePlayerModel(engine::assets::ModelData& data);
class PlayerCharacter {
  public:
    explicit PlayerCharacter(const engine::assets::ModelData& data);
    void update(float playerX, float distance, float time, float speed);
    void updateMenu(float time);
    void updateDeath(GameOverReason reason, float progress);
    void clearDeath();
    const engine::assets::ModelPose& pose() const { return pose_; }
    engine::math::Vec3 anchor() const { return anchor_; }
    float heading() const { return heading_; }
    float lift() const { return lift_; }
    float backward() const { return backward_; }

  private:
    struct Joint {
        int node;
        engine::math::Vec3 x, y, z; // World bind axes expressed in the joint's local space.
    };
    Joint joint(const char* name) const;
    void rotate(const Joint& joint, float x = 0, float y = 0, float z = 0);
    void rotateFrom(const Joint& joint, const engine::math::Mat4& base, float x = 0, float y = 0, float z = 0);
    engine::assets::ModelPose pose_;
    Joint root_, head_;
    std::array<Joint, 2> shoulders_, elbows_, wrists_, thighs_, knees_;
    std::array<Joint, 6> tail_;
    engine::math::Vec3 anchor_{};
    float previousTime_ = -1, previousDistance_ = 0, previousX_ = 0, previousSpeed_ = 0;
    float motion_ = 0, laneLean_ = 0, heading_ = 0, phase_ = 0, lift_ = 0;
    std::vector<engine::math::Mat4> deathStartLocals_;
    GameOverReason deathReason_ = GameOverReason::None;
    float backward_ = 0;
    bool deathStarted_ = false;
};
} // namespace game
