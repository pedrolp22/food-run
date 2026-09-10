#ifndef SUBWAY_GAMECONFIG_H
#define SUBWAY_GAMECONFIG_H

#include <array>

namespace GameConfig {
constexpr const char* kWindowTitle = "Subway Money";

constexpr int kLaneCount = 3;
constexpr std::array<float, kLaneCount> kLanePositions = {-2.2f, 0.0f, 2.2f};

constexpr float kPlayerZ = 3.0f;
constexpr float kPlayerScale = 0.9f;
constexpr float kLaneChangeSpeed = 12.0f;
constexpr float kInitialSpeed = 10.0f;
constexpr float kMinSpeed = 4.0f;
constexpr float kMaxSpeed = 18.0f;
constexpr int kMaxHealth = 3;

constexpr float kSpawnZ = -65.0f;
constexpr float kDespawnZ = 8.0f;
constexpr float kCollisionZRange = 0.95f;
constexpr float kCollisionXRange = 0.85f;

constexpr float kLaneSegmentLength = 12.0f;
constexpr float kLaneStartZ = -86.0f;
constexpr int kLaneSegmentCount = 10;

constexpr bool kMipmappedTextures = true;
constexpr int kPsxInternalWidth = 240;
constexpr int kPsxInternalHeight = 320;
constexpr int kN64InternalWidth = kPsxInternalWidth;
constexpr int kN64InternalHeight = kPsxInternalHeight;
constexpr float kWorldCurvature = 0.0008f;
constexpr float kRoadSway = 0.7f;

constexpr float kCameraHeight = 6.8f;
constexpr float kCameraZ = 11.0f;
constexpr float kCameraTargetHeight = 0.5f;
constexpr float kCameraTargetZ = -14.0f;
constexpr float kCameraVerticalFov = 60.0f;
} // namespace GameConfig

#endif // SUBWAY_GAMECONFIG_H
