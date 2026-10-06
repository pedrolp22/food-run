#include "TestSupport.h"
#include "game/Simulation.h"
#include <limits>

using namespace game;
using namespace GameConfig;
namespace {
void dayNightChecks() {
    constexpr float travel = (kDayNightTransitionSeconds - kDuskHoldSeconds) * .5f;
    DayNightCycle cycle;
    CHECK(cycle.nightBlend() == 0);
    cycle.update(kDayNightScoreInterval - 1, kDayNightTransitionSeconds);
    CHECK(cycle.nightBlend() == 0); // Scoring below the milestone cannot start the transition.
    cycle.update(kDayNightScoreInterval, 1.0f / 60);
    CHECK(cycle.nightBlend() == 0); // A score milestone starts a fade rather than switching immediately.
    cycle.update(kDayNightScoreInterval, travel * .5f);
    const float fading = cycle.nightBlend();
    CHECK(fading > 0 && fading < .5f);
    CHECK(cycle.nightBlend(.01f) > fading && cycle.nightBlend() == fading); // Presentation interpolation is read-only.
    cycle.update(kDayNightScoreInterval, travel * .5f);
    CHECK(near(cycle.nightBlend(), .5f));
    cycle.update(kDayNightScoreInterval, kDuskHoldSeconds);
    CHECK(near(cycle.nightBlend(), .5f)); // A distinct rose dusk lasts for the configured hold.
    cycle.update(kDayNightScoreInterval, travel);
    CHECK(cycle.nightBlend() == 1);
    cycle.update(kDayNightScoreInterval * 2 - 1, kDayNightTransitionSeconds);
    CHECK(cycle.nightBlend() == 1); // Time alone does not toggle a completed night.
    cycle.update(kDayNightScoreInterval * 2, 1.0f / 60);
    CHECK(cycle.nightBlend() == 1);
    cycle.update(kDayNightScoreInterval * 2, travel);
    CHECK(near(cycle.nightBlend(), .5f));
    cycle.update(kDayNightScoreInterval * 2, kDuskHoldSeconds);
    CHECK(near(cycle.nightBlend(), .5f));
    cycle.update(kDayNightScoreInterval * 2, travel);
    CHECK(cycle.nightBlend() == 0); // The next milestone returns to daylight through the same dusk.
    cycle.update(kDayNightScoreInterval * 3, 1.0f / 60);
    cycle.update(kDayNightScoreInterval * 3, travel * .5f);
    const float beforeRetarget = cycle.nightBlend();
    cycle.update(kDayNightScoreInterval * 4, 1.0f / 60);
    CHECK(cycle.nightBlend() == beforeRetarget); // Rapid score changes preserve the currently displayed phase.
    cycle.update(kDayNightScoreInterval * 4, kDayNightTransitionSeconds);
    CHECK(cycle.nightBlend() == 0);
    cycle.update(kDayNightScoreInterval * 5, 0);
    cycle.update(kDayNightScoreInterval * 5, std::numeric_limits<float>::quiet_NaN());
    CHECK(cycle.nightBlend() == 0); // Frozen/invalid gameplay ticks do not advance the sky.
    GameState pickup;
    pickup.score = kDayNightScoreInterval - 50;
    pickup.entities = {{EntityType::ScoreCollectible, 1, kPlayerZ}};
    resolveCollisions(pickup);
    pickup.dayNight.update(pickup.score, 1.0f / 60);
    pickup.dayNight.update(pickup.score, kDayNightTransitionSeconds);
    CHECK(pickup.dayNight.nightBlend() == 1); // Pickup points also count toward a milestone.
    pickup = GameState{};
    CHECK(pickup.dayNight.nightBlend() == 0); // Restart restores daylight and clears the previous milestone.
}
} // namespace
void gameplayTests() {
    dayNightChecks();
    CHECK(laneToX(-99) == kLanePositions.front());
    CHECK(laneToX(99) == kLanePositions.back());
    Simulation left(42);
    for (int i = 0; i < 10; ++i) {
        left.moveLane(-1);
    }
    CHECK(left.state().targetLane == 0);
    left.update(1.0f / 60.0f);
    CHECK(left.state().playerX < 0 && left.state().playerX > kLanePositions.front());
    left.reset();
    CHECK(left.state().playerX == 0 && left.state().score == 0 && left.state().time == 0);
    left.update(std::numeric_limits<float>::quiet_NaN());
    CHECK(left.state().time == 0);

    GameState state;
    state.entities = {{EntityType::ScoreCollectible, 1, kPlayerZ}};
    resolveCollisions(state);
    CHECK(state.score == 100 && near(state.speed, kInitialSpeed + 1.8f));
    CHECK(state.entities.empty() && state.collectEffects.size() == 1);
    CHECK(state.health == kMaxHealth);
    state = GameState{};
    state.time = 4.75f;
    const float pickupZ = kPlayerZ - kCollisionZRange + .01f;
    state.entities = {{EntityType::ScoreCollectible, 1, pickupZ, .6f, 2}};
    resolveCollisions(state);
    CHECK(state.entities.empty() && state.collectEffects.size() == 1);
    const auto pickedUp = state.collectEffects.front();
    CHECK(pickedUp.time == state.time && pickedUp.age == 0); // Feedback exists in the collision update itself.
    CHECK(pickedUp.textureIndex == 2 && pickedUp.spawnTime == .6f);
    CHECK(pickedUp.z == pickupZ && pickedUp.lane == 1);
    // The last bad-food pickup must finish its burst even when it ends the game.
    state.gameOver = true;
    updateCollectEffects(state, kCollectBurstDuration * .5f);
    CHECK(state.collectEffects.size() == 1 && near(state.collectEffects.front().age, kCollectBurstDuration * .5f));
    CHECK(state.time == 4.75f && state.collectEffects.front().time == pickedUp.time);
    updateCollectEffects(state, std::numeric_limits<float>::quiet_NaN());
    updateCollectEffects(state, -1);
    CHECK(state.collectEffects.size() == 1);
    updateCollectEffects(state, kCollectBurstDuration * .5f);
    CHECK(state.collectEffects.empty());
    state = GameState{};
    for (int i = 0; i < 3; ++i) {
        state.entities = {{EntityType::SlowCollectible, 1, kPlayerZ, .3f, i}};
        resolveCollisions(state);
        CHECK(state.collectEffects.back().textureIndex == i && state.collectEffects.back().age == 0);
    }
    CHECK(state.gameOver && state.purpleStreak == 3 && near(state.speed, 4.6f));
    state = GameState{};
    state.speed = kMinSpeed;
    state.entities = {{EntityType::SlowCollectible, 1, kPlayerZ}};
    resolveCollisions(state);
    CHECK(state.speed == kMinSpeed);
    state.speed = kMaxSpeed;
    state.entities = {{EntityType::ScoreCollectible, 1, kPlayerZ}};
    resolveCollisions(state);
    CHECK(state.speed == kMaxSpeed && state.purpleStreak == 0);
    state = GameState{};
    state.entities = {{EntityType::Obstacle, 0, kPlayerZ}};
    resolveCollisions(state);
    CHECK(!state.gameOver);
    state.entities = {{EntityType::Obstacle, 1, kPlayerZ + kCollisionZRange + 0.01f}};
    resolveCollisions(state);
    CHECK(!state.gameOver);
    state.entities.front().z = kPlayerZ;
    resolveCollisions(state);
    CHECK(state.gameOver);

    Simulation first(1234), second(1234);
    for (int frame = 0; frame < 1500; ++frame) {
        if (frame % 31 == 0) {
            first.moveLane((frame % 3) - 1);
            second.moveLane((frame % 3) - 1);
        }
        first.update(1.0f / 60.0f);
        second.update(1.0f / 60.0f);
        const auto& a = first.state();
        const auto& b = second.state();
        CHECK(a.score == b.score && a.gameOver == b.gameOver && a.playerX == b.playerX && a.time == b.time);
        CHECK(a.dayNight.nightBlend() == b.dayNight.nightBlend());
        CHECK(a.entities.size() == b.entities.size());
        for (std::size_t i = 0; i < a.entities.size(); ++i) {
            CHECK(a.entities[i].type == b.entities[i].type && a.entities[i].z == b.entities[i].z &&
                  a.entities[i].lane == b.entities[i].lane && a.entities[i].textureIndex == b.entities[i].textureIndex);
        }
    }
    CHECK(first.state().time > 1);
}
