# Subway Money

A C++20 lane runner with directional lighting, geometry shadows, and an optional retro renderer.

The main menu overlays its pixel-font options on a frozen view of the opening game scene. The world remains still
while browsing the menu; choosing Play starts the simulation.

## Build and run

Requires CMake 3.20+, a C++20 compiler, SDL2 development files, and an OpenGL 3.3 driver.
GLAD and stb_image are included. No dependencies are downloaded during configuration.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/subway
ctest --test-dir build --output-on-failure
```

Controls: **A/D** or **Left/Right** change lanes, **G** toggles PSX rendering, **F** toggles distance fog,
**H** toggles dithered shadow transparency (visible in PSX mode), **R** restarts, **Esc** exits.
Pressing **G** again returns to normal; the window title shows the mode and enabled PSX fog/shadow effects.
Both preferences survive restarting and mode switches; fog and dithered shadows apply only in PSX mode.
PSX uses restrained vertex snapping and affine texture wobble, nearest base-level texels, Gouraud lighting, and
5-bit RGB with the original signed dither pattern applied selectively to color/shading transitions.
Sprites, their outlines and glow effects retain 5-bit color without the dither pattern.
Flat colors and sharp texture edges stay clean. Stable subdivision grids keep roads and windows readable during movement.
Sidewalks use nearest mip texels with gradual distance transitions to reduce gravel shimmer while keeping blocky slab detail.
Shadow edges use an ordered mix of lit and shaded pixels, blending the two colors through the shadow transition.
The shadow pattern follows each surface and fades when its cells become subpixel, reducing shimmer during movement.
The optional Saturn-inspired shadow effect uses a fixed native-pixel checkerboard across the whole shadow, exposing the
lit surface in alternate pixels. The game enables it initially; **H** toggles it during play.
Optional distance fog gradually fades far street geometry toward the regular sky horizon. It preserves the game's bright
sky, cloud contrast and colorful material reflections, with a gentle linear fade that keeps nearby action clear.
Additive collectible effects fade with distance too.
The fog uses eight-bit vertex depth cues and the existing selective RGB555 shade dithering, with no animated noise phase.
It is enabled initially in the game; **F** toggles it during play.
PSX uses a 240 × 320 portrait raster, configurable in `GameConfig.h`.
The game camera has a subtle fisheye lens, adjustable with `kCameraFisheyeStrength` (0 disables it); menu text stays
undistorted over the camera view.
Buildings, roads, sidewalks and props combine the retro raster with shared PBR-like roughness, metallic response,
Fresnel sky reflections and shadowed sun specular. Texture-aligned material maps separate smoother road paint from rough
asphalt, with dull dirt and brighter stone grains in concrete. Brick, roofs and textured props also have local roughness,
reflectivity and occlusion variation; subtle normal detail reacts to the light without changing the pixel-art silhouettes.
Road and sidewalk bump maps add tar pits, raised aggregate and recessed slab joints. Normal rendering uses per-pixel
trilinear filtering so texture detail follows the curved surface continuously.
PSX material detail uses nearest mip texels with gradual transitions to keep distant glints stable. Props retain their
authored glTF finishes and normal maps, including the bench's wood and metal supports.
Skyscraper glass and masonry use generated material maps: each pane has a local finish, while recessed edges and rough
frames respond separately to lighting. An explicit glass mask identifies panes independently of their displayed color.
Brick buildings use the new tiled window and plain-wall textures, with a single bottom-center door on accessible
street-facing sides. Whole window tiles preserve the source proportions; plain brick fills the margins and rear walls.
Their generated material maps give blue panes world reflections, dark green door panels a subtle metallic finish, and
brick recessed mortar, roughness and bump detail. `kBrickFacadeTileWidth` controls their scale in `GameConfig.h`.
Brick buildings use regular lots, consistent footprints, aligned entrances and three or four complete floors.
The default footprint is two window tiles across and six along the street. Random frontage gaps, brick/tower mixes and
forecourts break up the street, with only some buildings close to the sidewalk. Footprint/floor controls and the
`kStreet*` occupancy, mix and setback settings in `GameConfig.h` tune this layout; choices stay stable while scrolling.
Empty frontage lots can contain pocket parks built from tiled `grass.png`, thin `spikedfence_decor.png` fences and the
tree model. Parks reserve two building columns, with wider lawns and taller trees. Recessed brick buildings can have
small planting beds flanking a clear door-to-sidewalk path. These beds meet the facade, trimming the final grass tile
to fit and fencing only the exposed edges. Park widths, lengths, corner notches, entrances, grass tints
and tree groups vary independently on each side; their seeded layouts remain stable as the road scrolls. The `kPark*`,
`kPocketParkChance`, `kGrassTileSize`, `kBrickGardenChance` and `kGarden*` settings in `GameConfig.h` control the layout.
Fence gaps and canopy cutouts retain transparency in the scene, sun shadows and live window reflections.
Generated material maps give grass rough tufts and recessed dirt, fences darker painted iron and smoother exposed metal,
and leaf textures a restrained waxy sheen and shallow veins. The tree atlas separates rough bark grooves from foliage.
Their surface, normal and inspectable height maps follow the existing generator workflow; transparent pixels contain
neutral data so the cutout silhouettes stay clean. `kGrassNormalScale`, `kFenceNormalScale`, `kLeafNormalScale` and
`kTreeNormalScale` in `GameConfig.h` tune relief strength without regenerating textures.
Tree canopies shed fluttering `leaf.png` particles. Scattered curbside leaves lift, tumble and fade when the runner
passes nearby, including between frames; spent leaves remain gone until restart. `kFallingLeavesPerTree`,
`kRoadLeavesPerTree`, `kMaxVisibleLeaves` and the `kLeaf*` settings control density, size and motion. Leaves use alpha
blending, reflect in windows, and retain nearest sampling without PSX color dithering.
The game uses a blue gradient with `cloud1.png`, `cloud2.png` and `cloud3.png` from `assets/textures/sky/`.
Individual clouds have seeded random positions, elevations and sizes. Three groups drift at different speeds over a
curved sky dome, with a gentle concave bow and slow billowing that retains the PNG transparency and nearest sampling.
At night, randomly placed stars fade in and twinkle behind the clouds, including in window reflections.
`kSkyLayoutSeed`, the cloud scatter/curvature/warp settings and the `kNightStar*` settings tune these effects.
Every 5,000 points
alternates day and night through a rose-tinted dusk: each transition takes 30 seconds, including a six-second dusk hold.
Sky, clouds, fog, lighting and window reflections share the same continuous phase. Game over freezes the cycle; restart
returns to daylight. `kDayNightScoreInterval`, `kDayNightTransitionSeconds`, `kDuskHoldSeconds` and the cloud/palette
settings in `GameConfig.h` control the effect. The sky is sampled directly rather than generated into a cubemap.
Building panes reflect the moving character, props, sidewalk and sky through live local captures in Normal and PSX modes.
Coated glass and a darker backing make the reflected detail clearer, with roughness controlling blur. Captures use accurate
mirrored-camera projection on each building; nearby coplanar facades share a capture.
`RenderSettings::planarReflectionSize` (512), `maxPlanarReflections` (6) and `planarReflectionDistance` (70) control local
capture quality and cost. `planarReflectionFadeDistance` (20) blends reflections out gradually from 50 to 70 units;
capture priorities also blend at the budget limit. `worldReflectionMapSize` sets cubemap face resolution for procedural
skies (128; 0 disables world reflections), and
`WindowMaterial::worldReflections` enables captures per facade. The distant probe follows the camera unless
`Scene::reflectionProbePosition` overrides its position when the cubemap fallback is used.
The reusable renderer retains a procedural sky with volumetric clouds, sun, moon and stars when authored sky textures are empty.
Tuning controls are documented in the [retro rendering guide](docs/architecture.md#retro-rendering).

Game tuning is in `include/GameConfig.h`. `kPsxDitherStrength` adjusts color and shadow-edge dithering
(0 disables it, 1 is full strength).
`kPsxDitheredShadows` selects the initial shadow-transparency preference, independently of color dithering.
`kPsxDitheredShadowDarkness` controls ambient/sky dimming in covered shadow pixels (0–1, default 0.8); higher values
give the checkerboard more contrast while its gaps retain fully lit materials.
`kPsxSidewalkLodBias` tunes sidewalk detail: lower values keep more grain, higher values reduce shimmer.
`kPsxDistanceFog` sets the initial fog preference. `kPsxFogStart` and `kPsxFogEnd` set camera-space distances
(default 45–125). `kPsxFogFollowsSky` matches the regular day/night horizon by default; set it to false to use the custom
`kPsxFogColor` and `kPsxNightFogColor` palette.
Game assets are registered in `src/game/GameAssets.cpp`.
Color textures live under `assets/textures/`, grouped into `skyscraper/`, `brick_building/`, `sky/`, `collectibles/` and
`grassy/`; road and sidewalk textures sit directly under `textures/`. Models remain under `assets/models/`.
Material data is stored in `assets/materials/` and beside the textured props. To regenerate these maps from the source
texture palettes, run `python3 scripts/generate_material_maps.py` with Pillow installed; the game and build use the
checked-in PNGs and do not require Python. Map channels and reusable material bindings are described in the
[rendering contracts](docs/architecture.md#rendering-contracts).
To regenerate only the skyscraper maps, run:

```sh
python3 scripts/generate_material_maps.py --texture assets/textures/skyscraper/skyscraper.png --profile skyscraper
```

Add `--seed 7` for another repeatable pane/wear variation, or `--output-dir /tmp/facade-maps` to inspect a variant.
Road and sidewalk generation also writes `*_height.png` for inspecting relief; rendering uses the corresponding normal maps.
Use `--normal-strength` to adjust generated relief, or `MaterialTextures::normalScale` to tune it at runtime.
Brick maps also include inspectable height data. Regenerate one brick texture with `--texture` and the matching
`brick_window`, `brick_wall` or `brick_door` profile, for example:

```sh
python3 scripts/generate_material_maps.py --texture assets/textures/brick_building/brick_building_door.png --profile brick_door
```

Scenery profiles are `grass`, `fence`, `leaf` and `tree`. For example:

```sh
python3 scripts/generate_material_maps.py --texture assets/textures/grassy/grass.png --profile grass
python3 scripts/generate_material_maps.py --texture assets/models/tree/textures/Tree_baseColor.png --profile tree
```

Quad maps go to `assets/materials/`; the tree's maps go beside its glTF texture, where the asset catalog loads them.
All generated maps use the source texture's dimensions. Skyscrapers cover each facade once; brick wall/window maps tile
with their color images, while doors are mapped once at ground level.
The default asset root is selected by `-DSUBWAY_ASSET_DIR=/path/to/assets`; a `Renderer` can also receive a root at runtime.
Shaders are editable files in `shaders/`, embedded into the graphics library during the build.

## Components

| CMake target | Responsibility | Runtime dependency |
| --- | --- | --- |
| `subway::math` | Matrices, vectors, normals, track curvature | None |
| `subway::assets` | CPU glTF import, validation, and sprite silhouette extrusion | None |
| `subway::graphics` | Scene commands, lighting, shadows, retro presentation, GPU resources | A current OpenGL context |
| `subway::platform` | SDL window and OpenGL context ownership | SDL2 |
| `subway::gameplay` | Game state, collisions, seeded simulation | None |
| `subway::world` | Subway assets and procedural scene generation | Graphics resources |
| `subway::application` | Input, timing, window title, game renderer facade | All game components |

`include/Renderer.h` is the small game-facing facade. The reusable API is
`include/engine/graphics/SceneRenderer.h`; it accepts scene data and does not know about lanes, collectibles, or Subway assets.
See [architecture and extension guide](docs/architecture.md) for ownership, rendering contracts, and migration notes.

## Tests and examples

CPU tests cover transform math, curved segment continuity, fixed timestep behavior, collisions,
seeded simulation, real model imports, malformed assets, and sprite contour geometry. They run without a display or SDL:

```sh
cmake -S . -B build/headless -DSUBWAY_BUILD_GRAPHICS=OFF -DSUBWAY_BUILD_PLATFORM=OFF -DSUBWAY_BUILD_GAME=OFF
cmake --build build/headless --parallel
ctest --test-dir build/headless --output-on-failure
```

Graphics tests are opt-in because they create an SDL/OpenGL window. They exercise Normal and PSX modes,
window resizing, mode switches, output stability, frame state, GPU resource lifetimes, affine UV interpolation,
vertex snapping and camera-plane crossings, nearest sampling, texture wrapping and stable mip transitions,
RGB555 dithering, shadow-edge blending, optional checkerboard shadow transparency, fog distance/palette/toggle behavior and horizon continuity,
Gouraud shading, affine subdivision, lens curvature/menu isolation, transparent menu layering over a frozen world,
building/street material roughness/Fresnel/metallic response, texture-aligned surface masks, normal-map light response,
material-state isolation, texture-defined facade/glass finishes, glTF color/factor/map handling, sun color and shadowing,
live world reflections of moving geometry, reflection blur/toggles and cubemap resource lifetimes, actual street bump response,
collectible rotation and visibility through a full turn, contour ripple motion/fading/occlusion,
immediate collection pops with matching poses and smooth fading after game over,
and the application loop with lane changes, restart, retro toggling, and exit:

```sh
cmake -S . -B build -DSUBWAY_ENABLE_GRAPHICS_TESTS=ON -DSUBWAY_BUILD_EXAMPLES=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/renderer_example
```

The example draws a small independent scene and links only `subway::graphics` and `subway::platform`.
Pass a frame count, such as `./build/renderer_example 3`, for a bounded smoke run.

## Reuse in another project

```cmake
set(SUBWAY_BUILD_GAME OFF CACHE BOOL "" FORCE)
set(SUBWAY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(path/to/subway)
add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE subway::graphics subway::platform)
```

Create a `Window`, then a `SceneRenderer`, populate a `Scene`, call `render(scene, window.drawableSize())`,
and call `window.swap()`. An existing OpenGL application can use `subway::graphics` with its own window/context layer;
set `SUBWAY_BUILD_PLATFORM=OFF` to avoid the SDL dependency entirely.
See [the complete example](examples/renderer_example.cpp).

Third-party source notices remain in `external/`; model attribution and licenses remain beside each model in `assets/models/`.
