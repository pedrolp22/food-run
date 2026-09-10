#pragma once

#include "engine/math/Math.h"

namespace engine::math {
struct TrackCurveSettings {
    float curvature = 0;
    float lateralSway = 0;
    float anchorZ = -5;
    float rampDistance = 24;
    float farZ = -100;
};
// A world-space bend for runner tracks. Segment endpoints coincide at every subdivision.
class TrackCurve {
  public:
    explicit TrackCurve(TrackCurveSettings settings = {});
    float drop(float z) const;
    float offset(float z) const;
    Vec3 normal(float z, float length) const;
    void bendSegment(Mat4& model, float z, float length, int longitudinalAxis) const;

  private:
    float ease(float z) const;
    TrackCurveSettings settings_;
};
} // namespace engine::math
