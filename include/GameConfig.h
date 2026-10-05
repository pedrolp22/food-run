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

// Collectible animation and item extrusion, shared by all rendering modes.
constexpr float kCollectibleSpinPeriod = 5.0f; // Seconds per full turn, at constant angular speed.
constexpr float kCollectibleBobPeriod = 2.4f;
constexpr float kCollectibleBobHeight = 0.045f;
constexpr float kCollectibleDepth = 0.05f;        // Relative to the sprite's scale (1.5 world units).
constexpr float kCollectibleOutlinePixels = 2.0f; // Source texture pixels, drawn inside the silhouette.
constexpr int kCollectibleRippleRadiusPixels = 24; // Maximum outward reach from the sprite's contour.
constexpr float kCollectibleRipplePeriod = 1.2f; // Seconds between expanding waves.
constexpr float kCollectibleRippleWidthPixels = 2.0f;
constexpr float kCollectibleRippleOpacity = 0.9f;
constexpr float kCollectBurstDuration = 0.28f; // Immediate flash, then a short outward pop.
constexpr float kCollectBurstWidthPixels = 3.5f;
constexpr float kCollectBurstOpacity = 1.0f;

constexpr float kSpawnZ = -65.0f;
constexpr float kDespawnZ = 8.0f;
constexpr float kCollisionZRange = 0.95f;
constexpr float kCollisionXRange = 0.85f;

constexpr float kLaneSegmentLength = 12.0f;
constexpr float kLaneStartZ = -86.0f;
constexpr int kLaneSegmentCount = 10;

constexpr bool kMipmappedTextures = false;
constexpr int kPsxInternalWidth = 480;
constexpr int kPsxInternalHeight = 640;
constexpr float kPsxDitherStrength = 1.0f; // Color/shade transitions and shadow-edge blending; 0 disables dithering.
constexpr bool kPsxDitheredShadows = true; // Saturn-style checkerboard transparency; H toggles it during play.
// Ambient/sky attenuation in covered shadow pixels, [0, 1]; 0 restores the lighter shadows.
constexpr float kPsxDitheredShadowDarkness = 0.8f;
constexpr float kPsxSidewalkLodBias = -0.75f; // Nearest mip texels reduce shimmer; negative values retain finer grain.
constexpr bool kPsxDistanceFog = true; // Gentle distance fade for the PSX view; F toggles it during play.
constexpr float kPsxFogStart = 45.0f; // Camera-space distances; keep the player and nearby street clear.
constexpr float kPsxFogEnd = 125.0f; // Distant geometry disappears into the mist by this distance.
constexpr std::array<float, 3> kPsxFogColor = {0.74f, 0.86f, 0.97f}; // Custom colors when sky matching is disabled.
constexpr std::array<float, 3> kPsxNightFogColor = {0.38f, 0.31f, 0.40f};
constexpr bool kPsxFogFollowsSky = true; // Match the bright sky's horizon and its regular day/night palette.
constexpr float kWorldCurvature = 0.0008f;
constexpr float kRoadSway = 0.7f;

constexpr float kCameraHeight = 6.8f;
constexpr float kCameraZ = 11.0f;
constexpr float kCameraTargetHeight = 0.5f;
constexpr float kCameraTargetZ = -14.0f;
constexpr float kCameraVerticalFov = 60.0f;
constexpr float kCameraFisheyeStrength = 0.095f;
} // namespace GameConfig

#endif // SUBWAY_GAMECONFIG_H
