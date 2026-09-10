#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace engine::time {
class FixedStep {
  public:
    explicit FixedStep(float step = 1.0f / 60.0f, float maxFrame = 0.05f) : step_(step), maxFrame_(maxFrame) {
        if (!std::isfinite(step) || !std::isfinite(maxFrame) || step <= 0 || maxFrame < step) {
            throw std::invalid_argument("Invalid fixed timestep");
        }
    }
    template <class Update> void advance(float elapsed, Update&& update) {
        if (!std::isfinite(elapsed) || elapsed <= 0) {
            return;
        }
        accumulator_ += std::min(elapsed, maxFrame_);
        while (accumulator_ >= step_) {
            update(step_);
            accumulator_ -= step_;
        }
    }
    void reset() { accumulator_ = 0; }
    float interpolation() const { return accumulator_ / step_; }

  private:
    float step_;
    float maxFrame_;
    float accumulator_ = 0;
};
} // namespace engine::time
