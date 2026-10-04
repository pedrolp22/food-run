#include "TestSupport.h"
#include "engine/math/Math.h"
#include "engine/math/TrackCurve.h"
#include "engine/time/FixedStep.h"
#include <limits>

using namespace engine::math;
void mathTests() {
    const Mat4 model = multiply(translation({2, 3, 4}), scale({2, 3, 4}));
    const Vec3 position = transformPoint(model, {1, 1, 1});
    CHECK(near(position.x, 4) && near(position.y, 6) && near(position.z, 8));
    CHECK(multiply(identity(), model).values == model.values);
    for (float sx : {-2.0f, 0.001f, 2.0f}) {
        const Mat4 transform = multiply(rotateY(kPi * 0.5f), scale({sx, 3, 0.5f}));
        const Vec3 normal = transformNormal(transform, {1, 1, 1});
        CHECK(near(dot(normal, normal), 1));
        CHECK(near(dot(normal, transformPoint(transform, {1, -1, 0})), 0));
        CHECK(near(dot(normal, transformPoint(transform, {1, 0, -1})), 0));
        CHECK(normal.z * sx < 0);
    }
    CHECK(std::isfinite(transformNormal(Mat4{}, {1, 0, 0}).y));
    const Mat4 view = lookAt({0, 0, 5}, {0, 0, 0}, {0, 1, 0});
    CHECK(near(transformPoint(view, {0, 0, 0}).z, -5));
    const auto ortho = orthographic(10, 20, 1, 101);
    CHECK(near(transformPoint(ortho, {10, 20, -1}).x, 1));
    CHECK(near(transformPoint(ortho, {0, 0, -101}).z, 1));
    TrackCurve track({0.0008f, 0.7f, -5, 24, -86});
    CHECK(track.drop(0) == 0 && track.offset(-5) == 0);
    CHECK(track.drop(-70) > track.drop(-30));
    for (int axis : {1, 2}) {
        for (float z = -100; z < 30; z += 0.3f) {
            Mat4 segment = multiply(translation({track.offset(z), -track.drop(z), z}),
                                    axis == 1 ? multiply(rotateX(kPi * 0.5f), scale({1, 2, 1})) : scale({1, 1, 2}));
            track.bendSegment(segment, z, 2, axis);
            for (float end : {-0.5f, 0.5f}) {
                const Vec3 p = transformPoint(segment, axis == 1 ? Vec3{0, end, 0} : Vec3{0, 0, end});
                CHECK(near(p.z, z + end * 2));
                CHECK(near(p.y, -track.drop(p.z)));
                CHECK(near(p.x, track.offset(p.z)));
            }
        }
    }
    checkThrows([] { TrackCurve bad({0, 0, -5, 0, -100}); });
    checkThrows([&] { track.normal(0, 0); });
    engine::time::FixedStep clock(0.125f, 0.5f);
    int steps = 0;
    float time = 0;
    auto update = [&](float dt) {
        ++steps;
        time += dt;
    };
    clock.advance(0.25f, update);
    CHECK(steps == 2 && near(time, 0.25f));
    clock.advance(5.0f, update);
    CHECK(steps == 6 && near(time, 0.75f));
    clock.advance(0.0625f, update);
    CHECK(near(clock.interpolation(), 0.5f));
    CHECK(near(clock.remainder(), .0625f));
    clock.advance(-1, update);
    clock.advance(std::numeric_limits<float>::quiet_NaN(), update);
    CHECK(steps == 6);
    clock.reset();
    CHECK(clock.interpolation() == 0);
    CHECK(clock.remainder() == 0);
    // Presentation time advances evenly across sub-step frames and fixed-step boundaries.
    engine::time::FixedStep animationClock;
    float simulationTime = 0;
    float previousVisualTime = 0;
    for (int frame = 0; frame < 240; ++frame) {
        constexpr float dt = 1.0f / 144;
        animationClock.advance(dt, [&](float step) { simulationTime += step; });
        const float visualTime = simulationTime + animationClock.remainder();
        CHECK(near(visualTime - previousVisualTime, dt));
        previousVisualTime = visualTime;
    }
    checkThrows([] { engine::time::FixedStep bad(0); });
}
