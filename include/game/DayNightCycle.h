#pragma once

#include "GameConfig.h"

namespace game {
// Score-triggered environment animation; advances only with gameplay time.
class DayNightCycle {
  public:
    void update(int score, float deltaTime);
    float nightBlend(float timeAhead = 0) const;

  private:
    int milestone_ = 0;
    float from_ = 0;
    float target_ = 0;
    float elapsed_ = GameConfig::kDayNightTransitionSeconds;
};
} // namespace game
