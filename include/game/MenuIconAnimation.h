#pragma once

#include "engine/math/Math.h"
#include <algorithm>
#include <cmath>

namespace game {
// A rigid, finite reaction: portraits remain completely still until clicked.
class MenuIconAnimation {
  public:
    struct Pose {
        float offsetX = 0; // In icon widths.
        float roll = 0;
        float yaw = 0;
    };
    void click(bool doubleClick) {
        if (doubleClick) {
            spinStart_ = spinAngle_;
            spinEnd_ = spinning() ? spinEnd_ + 2 * engine::math::kPi : 2 * engine::math::kPi;
            spinTime_ = 0;
        } else {
            shakeTime_ = 0;
        }
    }
    void advance(float dt) {
        if (!std::isfinite(dt) || dt <= 0)
            return;
        shakeTime_ = std::min(shakeTime_ + dt, kShakeSeconds);
        if (spinning()) {
            spinTime_ = std::min(spinTime_ + dt, kSpinSeconds);
            const float remaining = 1 - spinTime_ / kSpinSeconds;
            spinAngle_ = std::lerp(spinStart_, spinEnd_, 1 - remaining * remaining * remaining);
            if (!spinning())
                spinAngle_ = 0; // A complete turn ends exactly at the original orientation.
        }
    }
    Pose pose() const {
        const float t = shakeTime_ / kShakeSeconds;
        const float shake = std::sin(t * 6 * engine::math::kPi) * (1 - t) * (1 - t);
        return {.offsetX = .065f * shake, .roll = engine::math::radians(9) * shake,
                .yaw = spinAngle_ + engine::math::radians(5) * shake};
    }
    bool spinning() const { return spinTime_ < kSpinSeconds; }
    bool shaking() const { return shakeTime_ < kShakeSeconds; }

  private:
    static constexpr float kShakeSeconds = .44f, kSpinSeconds = .90f;
    float shakeTime_ = kShakeSeconds, spinTime_ = kSpinSeconds;
    float spinStart_ = 0, spinEnd_ = 0, spinAngle_ = 0;
};
} // namespace game
