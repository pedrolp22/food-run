#ifndef SUBWAY_GAMECONFIG_H
#define SUBWAY_GAMECONFIG_H

#include <array>

namespace GameConfig {
constexpr const char* kWindowTitle = "Teste";

constexpr int kLaneCount = 3;
constexpr std::array<float, kLaneCount> kLanePositions = {-2.2f, 0.0f, 2.2f};

constexpr float kPlayerZ = 3.0f;
constexpr float kPlayerScale = 0.9f;
constexpr float kPlayerModelHeight = 1.75f;
constexpr float kPlayerRunStride = 4.8f; // Scrolled world units per complete left/right running cycle.
constexpr float kPlayerRunBobHeight = .035f;
constexpr float kPlayerAnimationResponse = 16; // Pose and lane lean response per second.
constexpr float kPlayerLaneLeanDegrees = 12;
constexpr float kPlayerLaneTurnDegrees = 16;
constexpr float kCameraHeight = 6.8f;
constexpr float kCameraZ = 11.0f;
constexpr float kCameraTargetHeight = 0.5f;
constexpr float kCameraTargetZ = -14.0f;
constexpr float kCameraVerticalFov = 60.0f;
constexpr float kCameraFisheyeStrength = 0.095f;
constexpr float kPlayerMenuTapPeriod = 1.15f;
constexpr float kMenuCameraHeight = 0.65f;
constexpr float kMenuCameraZ = 6.1f;
constexpr float kMenuCameraLookHeight = 1.05f;
constexpr float kMenuCameraLookZ = kPlayerZ;
constexpr float kMenuCameraFov = 50.0f;
constexpr float kMenuCharacterYawDegrees = -60.0f;
constexpr std::array<float, 3> kMenuLightDirection = {-.65f, 1.8f, 1.0f};
constexpr float kCameraTransitionSeconds = 1.4f;
constexpr float kMenuCatX = kLanePositions[kLaneCount / 2];
constexpr float kMenuCatZ = kPlayerZ;
constexpr float kLaneChangeSpeed = 12.0f;
constexpr float kInitialSpeed = 10.0f;
constexpr float kMinSpeed = 4.0f;
constexpr float kMaxSpeed = 24.0f;
constexpr int kMaxHealth = 3;

// Collectible animation and item extrusion, shared by all rendering modes.
constexpr float kCollectibleSpinPeriod = 5.0f; // Seconds per full turn, at constant angular speed.
constexpr float kCollectibleBobPeriod = 2.4f;
constexpr float kCollectibleBobHeight = 0.045f;
constexpr float kCollectibleDepth = 0.05f;         // Relative to the sprite's scale (1.5 world units).
constexpr float kCollectibleOutlinePixels = 2.0f;  // Source texture pixels, drawn inside the silhouette.
constexpr int kCollectibleRippleRadiusPixels = 24; // Maximum outward reach from the sprite's contour.
constexpr float kCollectibleRipplePeriod = 1.2f;   // Seconds between expanding waves.
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
constexpr float kConcreteTileSize = 1.8f;     // World width per texture repeat; the image contains three paving slabs.
constexpr float kBrickFacadeTileWidth = 1.8f; // World units; preserve the source aspect for windows and doors.
// Regular residential lots: whole window bays and floors, with a consistent street frontage.
constexpr int kBrickBuildingWidthTiles = 2; // Across the street, from the entrance toward the rear wall.
constexpr int kBrickBuildingDepthTiles = 6; // Along the street: six window bays on the entrance side.
constexpr int kBrickBuildingMinFloors = 3;
constexpr int kBrickBuildingMaxFloors = 4;
constexpr float kBrickBuildingGap = 1.2f;     // Minimum gap between lots; tower lots also retain their clearance.
constexpr float kBrickBuildingSetback = 0.3f; // Distance from the outer sidewalk edge to the entrance facade.
constexpr float kBrickBuildingRoofBandHeight = 0.18f; // Plain brick above the last complete window row.
constexpr float kStreetBuildingChance = 0.68f;        // Occupied frontage lots; gaps reveal the buildings behind them.
constexpr float kStreetNearSidewalkChance = 0.45f;    // Occupied lots at the regular sidewalk setback.
constexpr float kStreetBrickChance = 0.72f;           // Mix towers into the occasional close street frontage.
constexpr float kStreetExtraSetbackMin = 1.4f;        // Other occupied lots have a small forecourt.
constexpr float kStreetExtraSetbackMax = 3.4f;
constexpr int kStreetBlockLots = 4; // Neighboring lots share building character and frontage alignment.
constexpr unsigned int kStreetLayoutSeed = 731;
// Curbside lamps face the road, with staggered, stable placement on both sidewalks.
constexpr float kStreetLampHeight = 4.8f;
constexpr float kStreetLampSpacing = 32.0f; // Per side, with jitter, occasional gaps and an opposite-side stagger.
constexpr float kStreetLampSpawnChance = .80f;
constexpr float kStreetLampCurbInset = .34f;
constexpr float kStreetLampLightRadius = 11.0f;
constexpr float kStreetLampLightIntensity = 2.2f;
constexpr std::array<float, 3> kStreetLampLightColor = {1.0f, .78f, .36f};
constexpr float kStreetLampOnStart = .55f; // Night phase at which the bulbs start to warm up.
constexpr float kStreetLampOnEnd = .90f;
constexpr float kStreetBenchLength = 2.2f; // Along the sidewalk; preserve the imported height and seat depth.
// Pocket parks reserve room behind empty frontage lots; forecourt gardens keep brick entrances open.
constexpr unsigned int kParkLayoutSeed = 947;
constexpr float kPocketParkChance = 0.9f; // Fraction of empty frontage lots with a green space.
constexpr float kGrassTileSize = 1.2f;    // World units per repeat of the tileable grass texture.
constexpr int kParkBuildingCells = 2;     // Across the street, reserving the building lot behind the park too.
constexpr int kParkMinWidthTiles = 4;
constexpr int kParkMaxWidthTiles = 6;
constexpr int kParkMinLengthTiles = 5;
constexpr int kParkMaxLengthTiles = 8;   // Layouts are also limited by the available building lot.
constexpr float kParkNotchChance = 0.5f; // Remove a corner to vary the outline of wider patches.
constexpr float kParkFenceHeight = 1.0f;
constexpr float kParkTreeMinHeight = 4.8f;
constexpr float kParkTreeMaxHeight = 7.8f; // Crowns retain clearance from surrounding buildings.
constexpr float kBrickGardenChance = .85f;
constexpr float kGardenDoorPathWidth = 2.6f; // An open path from the centered door to the sidewalk.
constexpr float kGardenFenceChance = .65f;   // Fence exposed garden edges; the building wall closes the back.
// Strength of the generated scenery normal maps; zero keeps the mapped finish with flat normals.
constexpr float kGrassNormalScale = .8f;
constexpr float kFenceNormalScale = .65f;
constexpr float kLeafNormalScale = .7f;
constexpr float kTreeNormalScale = .85f;
constexpr int kFallingLeavesPerTree = 7;
constexpr int kRoadLeavesPerTree = 14;
constexpr int kMaxVisibleLeaves = 256;
constexpr float kLeafSize = .32f; // World width, varied per leaf while preserving the texture aspect.
constexpr float kLeafFallSpeed = .65f;
constexpr float kLeafWakeRadius = 1.35f; // Outer-lane runners can disturb leaves near the curb.
constexpr float kLeafLiftSeconds = 1.3f;
constexpr float kLeafLiftHeight = 1.6f;
// Score milestones alternate day/night through a brief rose-colored dusk.
constexpr int kDayNightScoreInterval = 5000;
constexpr float kDayNightTransitionSeconds = 12; // Includes the dusk hold.
constexpr float kDuskHoldSeconds = 2;
constexpr std::array<float, 3> kDaySkyTop = {0.22f, 0.51f, 0.91f};
constexpr std::array<float, 3> kDaySkyHorizon = {0.69f, 0.85f, 0.98f};
constexpr std::array<float, 3> kDuskSkyTop = {0.54f, 0.28f, 0.51f};
constexpr std::array<float, 3> kDuskSkyHorizon = {0.98f, 0.62f, 0.67f};
constexpr std::array<float, 3> kNightSkyTop = {0.025f, 0.035f, 0.10f};
constexpr std::array<float, 3> kNightSkyHorizon = {0.12f, 0.16f, 0.29f};
constexpr std::array<float, 3> kDuskCloudTint = {1.0f, 0.67f, 0.73f};
constexpr std::array<float, 3> kNightCloudTint = {0.24f, 0.28f, 0.44f};
constexpr std::array<float, 3> kSkyAmbientLight = {0.48f, 0.34f, 0.62f}; // Ambient share; the night sky is much dimmer.
constexpr std::array<float, 3> kNightLightColor = {0.55f, 0.62f, 0.84f}; // Moonlight keeps the street readable.
constexpr std::array<float, 3> kDayLightDirection = {-0.7f, 1.4f, -0.9f};
constexpr std::array<float, 3> kDuskLightDirection = {-0.65f, 0.55f, -1.0f};
constexpr std::array<float, 3> kNightLightDirection = {0.14f, 0.18f, -1.0f}; // Low moon above the forward skyline.
constexpr float kMoonSizeDegrees = 6.5f; // Angular diameter, independent of camera distance or window aspect.
constexpr float kMoonBrightness = 1.35f;
constexpr float kMoonGlowStrength = 0.16f;
constexpr std::array<float, 3> kCloudWidthsDegrees = {26, 22, 36};
constexpr std::array<float, 3> kCloudElevationsDegrees = {7, 13, 19};
constexpr std::array<float, 3> kCloudRepeatDegrees = {90, 72, 120};
constexpr std::array<float, 3> kCloudOffsets = {0.10f, 0.017f, 0.0f};
constexpr std::array<float, 3> kCloudScrollSpeeds = {0.0010f, 0.0007f, 0.00045f}; // Turns per second.
constexpr std::array<float, 3> kCloudOpacities = {0.95f, 0.85f, 0.55f};
constexpr unsigned int kSkyLayoutSeed = 731;     // Change to shuffle the stable random cloud/star positions.
constexpr float kCloudHorizontalScatter = 0.32f; // Fraction of each cloud slot's spacing.
constexpr float kCloudElevationScatterDegrees = 7;
constexpr float kCloudSizeVariation = 0.28f;
constexpr float kCloudCurvature = 0.14f;     // Concave bow across each cloud, in texture-height units.
constexpr float kCloudWarpStrength = 0.025f; // Slow billowing UV displacement; 0 disables deformation.
constexpr float kCloudWarpSpeed = 0.14f;     // Radians per second.
constexpr float kNightStarDensity = 0.08f;   // Fraction of sky cells containing a randomly positioned star.
constexpr float kNightStarSizeDegrees = 0.11f;
constexpr float kNightStarBrightness = 0.9f;
constexpr float kNightStarTwinkleSpeed = 1.4f;
constexpr float kNightStarTwinkleStrength = 0.6f;
constexpr int kPsxInternalWidth = 240;
constexpr int kPsxInternalHeight = 320;
constexpr float kPsxDitherStrength = 1.0f;  // Color/shade transitions and shadow-edge blending; 0 disables dithering.
constexpr bool kPsxDitheredShadows = false; // Default Saturn-style checkerboard transparency.
// Ambient/sky attenuation in covered shadow pixels, [0, 1]; 0 restores the lighter shadows.
constexpr float kPsxDitheredShadowDarkness = 0.8f;
constexpr float kPsxSidewalkLodBias = -0.75f; // Nearest mip texels reduce shimmer; negative values retain finer grain.
constexpr bool kPsxDistanceFog = true;        // Default distance fade for the PSX view.
constexpr float kPsxFogStart = 45.0f;         // Camera-space distances; keep the player and nearby street clear.
constexpr float kPsxFogEnd = 125.0f;          // Distant geometry disappears into the mist by this distance.
constexpr std::array<float, 3> kPsxFogColor = {0.74f, 0.86f, 0.97f}; // Custom colors when sky matching is disabled.
constexpr std::array<float, 3> kPsxNightFogColor = {0.38f, 0.31f, 0.40f};
constexpr bool kPsxFogFollowsSky = true; // Match the bright sky's horizon and its regular day/night palette.
constexpr float kWorldCurvature = 0.0008f;
constexpr float kRoadSway = 0.7f;

} // namespace GameConfig

#endif // SUBWAY_GAMECONFIG_H
