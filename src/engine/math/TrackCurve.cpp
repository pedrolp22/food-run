#include "engine/math/TrackCurve.h"
#include <algorithm>
#include <stdexcept>

namespace engine::math {
TrackCurve::TrackCurve(TrackCurveSettings settings) : settings_(settings) {
    if (settings.rampDistance <= 0 || settings.farZ >= settings.anchorZ) {
        throw std::invalid_argument("Invalid track curve range");
    }
}

float TrackCurve::ease(float z) const {
    const float anchorZ = settings_.anchorZ;
    const float rampDistance = settings_.rampDistance;
    const float dz = anchorZ - z;
    if (dz <= 0.0f) {
        return 0.0f;
    }
    const float ramp = std::min(dz / rampDistance, 1.0f);
    return ramp * ramp * (3.0f - 2.0f * ramp);
}

float TrackCurve::drop(float z) const {
    const float anchorZ = settings_.anchorZ;
    const float dz = anchorZ - z;
    if (dz <= 0.0f) {
        return 0.0f;
    }
    return settings_.curvature * dz * dz * ease(z);
}

float TrackCurve::offset(float z) const {
    const float anchorZ = settings_.anchorZ;
    const float dz = anchorZ - z;
    if (dz <= 0.0f) {
        return 0.0f;
    }
    const float dzToFar = std::max(dz, anchorZ - settings_.farZ);
    const float arc = (dz / dzToFar) * (dz / dzToFar);
    return settings_.lateralSway * arc * ease(z);
}

Vec3 TrackCurve::normal(float z, float length) const {
    if (length <= 0) {
        throw std::invalid_argument("Invalid track segment length");
    }
    return normalize({0.0f, 1.0f, (drop(z + length * 0.5f) - drop(z - length * 0.5f)) / length});
}

void TrackCurve::bendSegment(Mat4& model, float z, float length, int axis) const {
    if (length <= 0) {
        throw std::invalid_argument("Invalid track segment length");
    }
    if (axis != 1 && axis != 2) {
        throw std::invalid_argument("Track axis must be Y or Z");
    }
    const float nearZ = z + length * 0.5f;
    const float farZ = z - length * 0.5f;
    // Adjacent segments share endpoints, so receivers have no steps or shadow seams.
    model.values[axis * 4] = offset(nearZ) - offset(farZ);
    model.values[axis * 4 + 1] = drop(farZ) - drop(nearZ);
    model.values[12] += (offset(nearZ) + offset(farZ)) * 0.5f - offset(z);
    model.values[13] += drop(z) - (drop(nearZ) + drop(farZ)) * 0.5f;
}
} // namespace engine::math
