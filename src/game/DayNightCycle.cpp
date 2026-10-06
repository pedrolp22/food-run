#include "game/DayNightCycle.h"
#include <algorithm>
#include <cmath>

namespace game {
using namespace GameConfig;
static_assert(kDayNightScoreInterval > 0);
static_assert(kDayNightTransitionSeconds > kDuskHoldSeconds && kDuskHoldSeconds >= 0);
namespace {
float smooth(float t) {
    return t * t * (3 - 2 * t);
}
float mix(float a, float b, float t) {
    return a + (b - a) * t;
}
} // namespace
void DayNightCycle::update(int score, float deltaTime) {
    if (!std::isfinite(deltaTime) || deltaTime <= 0) {
        return;
    }
    const int milestone = std::max(score, 0) / kDayNightScoreInterval;
    if (milestone != milestone_) {
        from_ = nightBlend();
        milestone_ = milestone;
        target_ = milestone % 2 ? 1.0f : 0.0f;
        elapsed_ = from_ == target_ ? kDayNightTransitionSeconds : 0;
    } else {
        elapsed_ = std::min(elapsed_ + deltaTime, kDayNightTransitionSeconds);
    }
}
float DayNightCycle::nightBlend(float timeAhead) const {
    const float ahead = std::isfinite(timeAhead) ? std::max(timeAhead, 0.0f) : 0;
    const float elapsed = std::min(elapsed_ + ahead, kDayNightTransitionSeconds);
    if (elapsed >= kDayNightTransitionSeconds) {
        return target_;
    }
    constexpr float travel = (kDayNightTransitionSeconds - kDuskHoldSeconds) * .5f;
    if (elapsed < travel) {
        return mix(from_, .5f, smooth(elapsed / travel));
    }
    if (elapsed <= travel + kDuskHoldSeconds) {
        return .5f;
    }
    return mix(.5f, target_, smooth((elapsed - travel - kDuskHoldSeconds) / travel));
}
} // namespace game
