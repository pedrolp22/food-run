# Architecture and extension guide

## Boundaries

`Game` is the application loop: it maps SDL input, advances `Simulation` through `FixedStep`, updates the title,
and presents the latest state. Visual animation adds `FixedStep::remainder()` to simulation time so it advances at
presentation frequency; game over freezes the world animation and restarting clears the accumulator.
Collection feedback has its own elapsed age and receives the fractional timestep separately, so a final pickup's pop
can finish smoothly while gameplay is frozen.
`Simulation` and the domain types under `include/game/` have no renderer or SDL dependency.
An explicit random seed allows reproducible simulation within the same standard-library implementation.
`GameState::dayNight` starts in daylight. `Simulation` checks distance and pickup points after each scoring update:
every 5,000 points alternates the target between night and day. `DayNightCycle` eases to rose dusk, holds it for two seconds,
then eases to the target, taking 12 seconds in total. These values are set in `GameConfig.h`. Rapid milestone changes
retarget from the current phase without a jump. Presentation samples the fixed-step remainder without mutating the cycle;
game over freezes it and restart clears it along with the score.

`Renderer` coordinates a window, the graphics renderer, game assets, and a `WorldScene` builder. Its implementation
is private, so consumers of `Renderer.h` do not include SDL, GLAD, shader locations, or GPU resource definitions.
The former nested entity types are compatibility aliases for `game::Entity`, `game::EntityType`, and `game::CollectEffect`.

`WorldScene` builds ordered scene commands once per frame. Track geometry, buildings, sidewalks, props, entities,
and effects each have their own builder method. Both graphics passes consume those same transforms;
world generation is not repeated for the shadow pass. `SceneCommands.cpp` translates game transforms and animation
into generic commands. Reusable track deformation lives in `engine::math::TrackCurve` with independent settings.

`SceneRenderer` accepts a `Scene` containing a camera, directional light, sky, borrowed asset references, and draw commands.
It owns the passes and shader bindings but has no game configuration, gameplay state, SDL calls, or asset paths.
Its targets can be used independently through `add_subdirectory`; the platform library can be disabled to remove
the SDL build requirement while retaining OpenGL rendering.

## Resource ownership

1. Create and make current the window's OpenGL context.
2. Construct `SceneRenderer`, textures, meshes, and models on that thread.
3. Keep referenced textures, meshes, and models alive and unmoved while their scene commands are used.
4. Destroy GPU resources while their context is current, before destroying the window.

The game's facade declares its window first, so destruction automatically occurs in the required reverse order.
A `Window` balances its own SDL video-subsystem reference. It exposes `makeCurrent()` for applications with multiple contexts.
OpenGL resource operations require the owning context to be current; they do not switch contexts implicitly.

`ShaderProgram`, `Texture`, `TextureSampler`, `Mesh`, and `RenderTarget` own their handles and support moves, not copying. `Model` owns
its mesh primitives and textures. A failed shader, texture, framebuffer, or model construction unwinds previously
created resources automatically. CPU `loadGltf()` is independent from GPU `Model` upload, so import and validation can
be tested or prepared before a graphics context exists.

A caller supplying its own context must load the bundled GLAD function table before using graphics resources.
Link `subway_glad`, include `<glad/glad.h>`, and call `gladLoadGLLoader` with the platform's procedure resolver.
The provided `Window` performs that initialization and requests a core OpenGL 3.3 context.

## Rendering contracts

The renderer controls GL state during a frame. It renders to framebuffer zero and returns with depth testing and
alpha blending enabled, depth writes enabled, polygon offset disabled, the default framebuffer bound,
texture unit zero active, and no shader program, VAO, or sampler on units zero through eleven bound. Multisampling is enabled;
alpha-to-coverage, sample coverage, and sample masking are disabled. It does not preserve arbitrary caller GL state.
Applications with an additional UI/rendering pass should set that pass's state explicitly.

Matrices use column-major storage and column vectors. Camera FOV is in degrees; rotation functions use radians.
Quad normals are world-space normals; cube and model normals use the inverse transpose of their model transform.
Use positive, nonzero model scale components unless a deliberately mirrored transform is required.

Scene objects retain submission order. Cubes, models, sprites, and textured or colored quads participate in the shadow pass.
Quads can opt out of casting or receiving shadows. Their `opacity` fades the whole surface; the quad texture supplies RGB.
`Material.h` defines the shared PBR-like finish. Set `Quad::pbrSurface` for textured or colored street surfaces,
or `Cube::pbrSurface` for solid geometry; both use `surfaceMaterial`. Set `ModelInstance::pbrSurface` to use each imported
primitive's finish, with an optional `materialOverride` for that instance. Commands without these flags retain the original
lighting path. The game enables the shared finish on asphalt, sidewalks, ground, curbs, the player, and model props.
Road and sidewalk maps give each source texel its own roughness and reflectivity: paint is smoother than asphalt, tar and
dirt are duller, and brighter aggregate catches more light. Concrete stays mostly rough, with restrained stone glints.
Brick and roof maps separate mortar, masonry and dirt. Curbs use roughness 0.94, surrounding ground uses 0.96,
and the blue player uses a 0.45 plastic finish. Model props retain their authored roughness/metallic factors: bench wood is
rough and dielectric, its supports are smooth metal, and cones/barriers retain their separate plastic/rubber finishes.
Textured props add local finish variation and use their authored normal maps where available.

`Quad::materialTextures` supplies optional borrowed property, tangent-space normal and occlusion textures, also supported
by building quads. `Model` owns the corresponding maps per primitive. All maps follow the color texture's UV tiling,
offsets and PSX affine sample location. The property map's G and B channels multiply roughness and metallic factors;
occlusion uses R from the occlusion texture and affects indirect light. Set `packedReflectivity` to use property-map A as
a reflectivity multiplier for custom maps. Standard imported glTF maps leave that flag off, so their R/A channels never
alter roughness, metallic or reflectivity. These are linear data textures, independent of albedo gamma and RGB555 color.
`normalScale` controls tangent-space X/Y strength; `occlusionStrength` blends between unoccluded and mapped indirect light.
Missing maps retain the scalar finish. Bindings and flags are reset between commands; overrides change factors while
retaining the primitive's maps.

The game loads `assets/materials/*_surface.png` (R occlusion, G roughness, B metallic multiplier, A reflectivity) and
matching `*_normal.png` files. Road, sidewalk, brick and roof roughness/reflectivity factors are one so their maps define
the finish directly. Streets, plain brick and roofs use a zero metallic factor; mapped window facades and brick doors
use a factor of one, with their property maps limiting metallic response to metal texels.
For textured models without authored metallic/roughness or normal maps, the game catalog uses adjacent `*_surface.png`
and `*_normal.png` companions if present. Generated model companions share the base texture's wrapping, preserving
the tree atlas's UV coordinates slightly outside its bounds. Authored maps and imported finish factors remain in use.
`scripts/generate_material_maps.py` builds these numerical maps from the unchanged source palettes;
Pillow is only needed to regenerate them. Surface-map generation is deterministic and has no animated noise.
Road relief separates black tar pits, dark aggregate and thin paint; sidewalk relief recesses slab joints and raises
stone grains. Their `*_height.png` files expose the generator's intermediate relief for inspection; lighting uses the
matching tangent-space normal maps. The default street normal strength is 3.0; override it with `--normal-strength`,
or adjust `MaterialTextures::normalScale` at runtime. Street relief wraps at both texture borders, and the green normal
channel accounts for the loader's vertical image flip.

Grass, fence and leaf quads use matching property and normal maps from `assets/materials/`. Grass separates rough tufts
from shallow dirt crevices. Fence paint is dielectric, while brighter exposed iron uses metallic response and lower
roughness. Leaves have subtle waxy reflectivity and vein relief. The tree's companion maps distinguish foliage from
rough bark grooves; their normal-map Y direction follows the unflipped glTF UVs. All four profiles also write height maps.
The generator ignores transparent neighbors when deriving relief gradients and emits neutral data in transparent texels,
so alpha cutouts do not introduce raised edges or dark occlusion rims. Maps share the color UV tiling, offsets and clipping,
including partial tiles where front gardens meet buildings. `kGrassNormalScale`, `kFenceNormalScale`, `kLeafNormalScale`
and `kTreeNormalScale` tune the game defaults; generated finishes define grass, fence and leaf roughness/reflectivity directly.

Skyscrapers also load `skyscraper_glass.png`: its linear R channel marks the glass coat (1) and masonry frames (0).
`MaterialTextures::glass` exposes that mask to facade commands. With both a glass mask and a property map, every texel's
roughness, metallic and reflectivity values come from the property map multiplied by `surfaceMaterial`; the game's
facades set all three factors to 1. This includes the blue panes, so their finishes can be edited in the maps.
Normal and occlusion maps apply to panes and frames alike. The coat mask controls the darker opaque backing and follows
the same UVs, affine coordinates, wrapping and mip filtering as the other data maps. Masks can identify glass of any color.
The generator detects connected blue pane regions in the source texture, gives them subtle individual finishes and
recessed edge detail, and adds restrained masonry wear. Glass and masonry use zero metallic values; the map's B channel
can be authored for metal trim. These data maps keep their native precision through lighting before final retro color conversion.

Brick buildings use the tileable `brick_building_window.png` and `brick_building_wall.png`, plus the single entrance
`brick_building_door.png`. Front and street-facing sides receive complete window tiles, preserving their source aspect;
rear and outward sides, facade margins and roof bands use plain brick. `GameConfig::kBrickFacadeTileWidth` sets
their world size (1.8 units). Brick footprints use whole tiles: the default `kBrickBuildingWidthTiles` (2) and
`kBrickBuildingDepthTiles` (6) produce a 3.6-by-10.8-unit building with six window bays along the street. Heights alternate
between `kBrickBuildingMinFloors` and `kBrickBuildingMaxFloors` (3–4), with whole floor heights derived from the window
texture's aspect and a `kBrickBuildingRoofBandHeight` (0.18) above them. Brick lots have no lateral or longitudinal jitter.
The frontage row uses stable per-lot random choices. `kStreetBuildingChance` (0.68) controls occupancy; empty lots reveal
the scenery behind them. Occupied lots use `kStreetBrickChance` (0.72) to mix brick buildings and towers.
`kStreetNearSidewalkChance` (0.45) places some at the regular `kBrickBuildingSetback` (0.3); others sit farther back by
`kStreetExtraSetbackMin`–`kStreetExtraSetbackMax` (1.4–3.4). The whole lot, including its background columns, shares that
extra setback to prevent intersections. Brick dimensions and floor/window alignment stay regular.
Longitudinal spacing is at least 12 units to clear the tower lots, increasing with building depth and
`kBrickBuildingGap` (1.2). Outer columns keep mixed brick/tower scenery, with tower variation within separate lots.
Buildings retain their identity, floor count and spacing as the world scrolls, including when indices pass zero.
Only the innermost building column receives doors: one tile at the bottom center of its street-facing side.
Facade patches partition the surface without overlapping quads.
`Quad::facadeVerticalRange` gives each patch its normalized bottom/top within the whole building, so foundation occlusion
does not restart above an entrance or at a roof band.

All three brick sources load matching surface and normal maps; their height maps expose the generated relief for
inspection. `brick_building_window_glass.png` marks only the blue panes and enables the same live planar and sky reflections
as skyscrapers. Green door panels have a restrained metallic finish and raised frame/panel detail; surrounding brick
has zero metallic response. Pale mortar and dark pits are recessed and occluded, with high roughness and low reflectivity.
Repeating wall/window normals wrap their texture borders; single door normals clamp them. These maps share the albedo's
UVs and filtering, including PSX affine coordinates.

Regenerate the game catalog with `python3 scripts/generate_material_maps.py`, or choose one texture using
`--texture assets/textures/skyscraper/skyscraper.png --profile skyscraper`. The facade profile emits surface, normal and glass-mask PNGs
at the original image size, with clamped border normals for a complete, untiled facade. `--seed` chooses repeatable
pane/wear variation; `--output-dir` redirects generated maps for inspection. Recipes live in the script and the resulting
PNGs can also be edited directly; reloading the game's assets picks up those edits without a shader rebuild.
The `brick_window`, `brick_wall` and `brick_door` profiles regenerate each new brick texture independently, e.g.
`--texture assets/textures/brick_building/brick_building_door.png --profile brick_door`. Brick relief defaults to normal strength 3.0, adjustable
with `--normal-strength` or the runtime `MaterialTextures::normalScale`.

Normal mode filters both material albedo and data maps with trilinear mipmaps, using each pixel's UV derivatives
rather than one rounded mip level per road segment or facade. PSX keeps nearest texels and continuously blends adjacent mip
levels, measuring the footprint from perspective-correct UV derivatives even when samples use affine UVs. This preserves
nearby pixel detail and reduces distant aggregate/normal glints without changing the color texture's sampling policy.
The normal basis is reconstructed from UV/world-position derivatives; constant UVs safely use the geometric normal.
Normal detail adjusts local diffuse light on top of the PSX vertex shade and widens unresolved specular lobes.
Retro modes keep 8-bit diffuse vertex shades and screen-space interpolation, while view-dependent material specular uses
the shared pixel shader. Nearest PSX sampling, stable affine subdivisions, nearest mip minification, and selective
RGB555 dithering apply to the shared finish. Normal-mode road heat distortion is retained; console materials have no
heat animation. Street surfaces do not inherit facade-height occlusion or glass detection.
Set `Quad::buildingSurface` for PBR-like facade and roof shading. `surfaceMaterial` supplies perceptual `roughness`
(default 0.85), `metallic` (default 0), and `reflectivity` (default 1), all clamped to zero through one. Masonry is a rough
dielectric; metallic surfaces use the texture color for their specular reflection and lose their diffuse component.
For facade commands without an authored glass mask, `Quad::skyscraperWindows` identifies blue glass texels in the color
texture. Those panes use scalar `windowMaterial.roughness` (default 0.24) and `windowMaterial.reflectivity` (default 1);
gray frames retain the surface material. With paired glass/property maps, the texel's mapped roughness and reflectivity
replace those scalar values. `WindowMaterial::reflectance` and `backingTint` apply to both mapped and unmapped panes.
Glass is represented by a dielectric coat over an opaque backing. Coated glass defaults to 28% reflectance at normal
incidence and a backing brightness of 0.18, making local reflections readable; use `reflectance = 0.04` for uncoated glass.
Both controls are clamped to zero through one. Higher roughness gives a wider, softer lobe and blurred environment response;
zero reflectivity disables both sun and environment specular. Buildings remain opaque without modeled interiors/refraction.
The shared `material.glsl` shader uses GGX, height-correlated Smith visibility, Schlick Fresnel,
and energy-weighted Lambert diffuse
([model reference](https://google.github.io/filament/main/filament.html#materialsystem/standardmodel)). Textures, tints and
light colors are decoded from sRGB for lighting, then encoded once after a highlight shoulder. Imported glTF base-color
factors are already linear and multiply decoded texture colors without a second conversion. Sun specular follows the
actual per-fragment world position, view direction and light color, and shares shadow visibility with diffuse lighting.
Disabling shadow reception does not disable directional shading. Fresnel strengthens reflections at grazing views.
Indirect illumination and rough reflections use an analytic sky/ground environment derived from the scene's sky colors,
with an [analytic environment BRDF fit](https://www.unrealengine.com/en-US/blog/physically-based-shading-on-mobile).
Nearby glass uses live planar captures of the sky, character, roads, sidewalks, buildings and props.
The renderer mirrors the main camera across the facade plane and projects the result
onto the actual window positions. Geometry behind the plane is clipped, preventing the hosting building from covering
the reflected street. A cropped projection concentrates resolution on the visible facade rectangle. Coplanar facades
share a capture; the renderer selects the most visible nearby planes and updates them every frame, including moving objects.
Rendering and sampling use the same mirrored, cropped projection; captures are never stretched across the facade UVs
or rendered from a different street camera. The glass mask is applied once after selecting the reflection, so filtered
pane edges cannot show a displaced cubemap image underneath the planar one.
With `Sky::texturedClouds` or `Sky::panorama`, distant panes sample the same authored sky directly, and no six-face
world/sky cubemap is generated. Nearby planar views still capture moving world geometry. Without either authored sky,
distant panes fall back to a live cubemap
captured from six 90-degree views. `Scene::reflectionProbePosition` optionally places that shared distant probe;
by default it follows the main camera. It does not affect local planar captures or the supplied sky panorama.
All captures reuse the current sun shadow and disable world-reflection sampling to prevent feedback. They omit UI and
additive effects and use Normal rendering without console snapping, affine warp, PSX fog or the presentation lens.
Reflection textures store display colors in sRGB, so mip filtering and sampling supply linear radiance to the BRDF.
Perceptual roughness squared selects continuously filtered mip levels, preserving detail in smooth panes. The glass mask
restricts captured specular to panes; masonry and other materials retain the analytic environment. Fresnel, reflectivity,
occlusion and final PSX RGB555 conversion still apply. Shadowed glass retains indirect reflections.
`RenderSettings::worldReflectionMapSize` sets the cubemap face resolution for procedural skies (default 128);
zero disables world reflections, including local captures and authored sky reflections, while retaining the sky background.
`planarReflectionSize` sets the full-view longest-side resolution (default 512), cropped to each plane's visible area;
zero disables planar captures while retaining the distant environment fallback.
`maxPlanarReflections` sets the high-detail budget (default 6); zero disables planar captures. All other visible planes
retain lower-resolution live views at `distantPlanarReflectionSize` (default 128, positive). The budget changes detail,
not reflection presence. Nonpriority planes gain detail gradually between 90 and 35 units using their shared building
distance, reaching full nearby resolution. `planarReflectionDistance` limits distance to the nearest related face, or to the
individual facade when ungrouped (default 120). `planarReflectionFadeDistance` (default 30) blends local captures into
the distant environment between 90 and 120 units, reaching zero before a capture is dropped.
`Quad::reflectionId` optionally identifies a facade across rebuilt/moving scenes. Game IDs encode the lot, column,
side and face; coplanar groups keep all member IDs. Unidentified static scenes match by plane. `Quad::reflectionGroupId`
links building faces and roofs with one stable building identity, sharing nearest distance and activation strength.
Activation starts when the building enters range, including faces currently outside the view or hidden by another
building. A newly visible side therefore inherits the same fade state instead of starting another fade.
Selected planes receive a 20% detail-priority preference, with resolution changes eased by
`planarReflectionBlendSeconds` (default .25) using `Scene::time`. Frozen time holds blends; rewind, mode and viewport
changes reset selection. Nearby views (within 35 units), detailed planes and ungrouped facades update every frame.
Other grouped views remain populated between staggered refreshes at `distantPlanarReflectionUpdateSeconds` (default .2;
zero updates every frame). Stable building identities distribute refresh phases; new faces capture immediately.
Between refreshes, the saved projection follows the building's translation to retain mapping during small scrolling
movements. Camera translation over .5 units, orientation changes or projection changes also force a refresh.
`WindowMaterial::worldReflections = false` selects only the analytic environment per facade. Capture slots and their
targets persist across priority changes. Cropped viewports use 32-pixel buckets while allocations grow in power-of-two
buckets without shrinking; a UV scale samples only the populated viewport. Blur mip footprints are limited by its
shorter dimension. PSX captures do not exceed the main raster's pixel density. Reflection passes cache world bounds
once per frame, including exact imported-model vertex bounds, and cull them against the reflection frustum/clip plane.
Closed, opaque rectangular building groups also hide captures entirely behind their projected convex silhouettes;
partial visibility, near-plane crossings and gaps in facade coverage conservatively retain captures.
Authored skies draw once before geometry, avoiding a duplicate depth pass; their shared sky uniforms are uploaded once
per program per frame. Shared lighting/environment uniforms upload once per program per view. The expensive
procedural fallback retains an opaque quad/cube depth prepass to restrict sky shading to uncovered pixels.
Additional world views trade rendering time for local detail; scenes without reflective glass skip capture.
There are no timed bands, pane phases or hard-coded reflection shapes. The microfacet lobe widens over unresolved raster
footprints to limit subpixel glints. Source texture sampling, PSX affine geometry, RGB555 and selective dithering still apply;
the PBR lighting is an intentional modern part of the retro hybrid rather than original console lighting emulation.
Foundation occlusion uses whole-face height and affects only indirect light, independent of UV repeats/offsets.
`Quad::haze` blends a surface into the sky haze while retaining opacity and depth occlusion.
Game skyscrapers map the complete, clamped texture once onto each of their four sides. Brick walls use a fixed world size
for each texture tile. Buildings remain opaque as they fade into the distance.
The game uses a blue sky gradient with `cloud1.png`, `cloud2.png` and `cloud3.png` from `assets/textures/sky/` in
`Sky::texturedClouds`.
Their alpha channels reveal the gradient around and through the authored silhouettes. Three groups of sky slots retain
the source proportions, with seeded random horizontal offsets, elevations and sizes for each cloud. Whole slots close
the dome at its longitude seam; neighboring slots are sampled together so wind never changes a cloud's identity at a
slot boundary. The wind offset wraps over a full turn. Clouds use a tangent-plane projection on the sky dome, a slight
concave bow and slow sinusoidal UV billowing. Each group scrolls at its own speed using `Scene::time`; positions follow
world directions, independent of camera translation.
`SkyCloudLayer` exposes scatter, size variation, curvature, warp strength and speed; the corresponding `kCloud*` settings
in `GameConfig.h` configure the game. `Sky::layoutSeed`/`kSkyLayoutSeed` changes the stable random arrangement.
Sky and glass shaders share `sky_clouds.glsl` and use nearest sampling at mip level zero; this path performs no cloud ray
marches or offscreen sky generation.

`Sky::stars` controls sparse random star positions on directional cube-face cells, avoiding a singularity at the zenith.
Stars fade in as `nightBlend` passes 0.6–0.95, stay fixed in world directions, and twinkle independently using the same game
clock. A few brighter stars have small sparkle rays. Clouds composite over the stars so their alpha masks occlude them.
`kNightStarDensity`, `kNightStarSizeDegrees`, `kNightStarBrightness`, `kNightStarTwinkleSpeed` and
`kNightStarTwinkleStrength` tune the game defaults. Stars and cloud animation freeze with game time and are shared with
planar captures and distant window reflections. The procedural fallback keeps its own existing star style.
`Sky::moon` borrows the alpha texture `assets/textures/sky/moon.png` in the game. A world-oriented tangent plane preserves
its angular diameter (default 6.5 degrees) and source aspect at any bearing or viewport shape. Nearest level-zero
sampling retains crater detail. A modest cool halo surrounds the emissive disc; both fade in between night phases .5
and .9 behind the cloud layers and are shared by planar and distant window reflections. `kMoonSizeDegrees`,
`kMoonBrightness` and `kMoonGlowStrength` set the game's appearance. `kNightLightDirection` supplies both the moon's
bearing and the nighttime directional light, so its illumination and shadows agree.
`Sky::nightBlend` continuously interpolates `top`/`bottom` daylight through `dusk` at 0.5 to `night` at 1.
`Sky::palette()` supplies the same sky, horizon and cloud tint to backgrounds, material lighting and fog. The game also
changes directional light color and ambient fill, with blue moonlight keeping nighttime streets readable.
Normalized `kDayLightDirection`, `kDuskLightDirection` and `kNightLightDirection` interpolate with the same phase;
the shadow view-projection matrix follows that light without an angle jump at dusk. Local reflections
render this same sky; distant panes sample it directly. Cloud sizes, elevations, offsets, speeds, opacity and all three
palettes are configured in `GameConfig.h`.
For reusable scenes, `Sky::panorama` still supports an optional upper-hemisphere bitmap. Its elevation is compressed
toward the horizon and mirrored horizontal wrapping joins nonmatching edges. Sky and glass sample source texels without
filtering or pole blending. `Sky::panoramaScrollSpeed` sets drift in texture widths per second, defaulting to zero; its
offset wraps over the full two-width mirrored period. Assigned cloud layers take precedence over the panorama.
Clear both the cloud textures and panorama to select the procedural sky described below.
The procedural sky reconstructs world-space view rays from the camera basis, vertical FOV and render-target aspect. Its continuous
dome follows yaw, pitch and roll, blending continuously into horizon haze. The dome and directionally placed stars, sun
and moon stay fixed when the camera translates. Sun size and distance set its angular diameter, so the disc stays round
when viewed from another bearing. At night a flat-shaded moon
replaces it at the light's direction. Clouds can occlude both discs.
Clouds occupy two bounded volumes at world heights 180–280 and 520–600, with independent offsets and wind speeds.
Broad 2D gradient Perlin noise places separated cloud masses; filtered 3D Perlin detail shapes their billows and interiors.
A rounded height profile gives each mass thickness. Stable 32-sample ray marches accumulate Beer–Lambert opacity from
front to back, with early exits for opaque interiors and cached broad-noise gradients between adjacent ray samples.
Two density samples toward the sun approximate self-shadowing;
edges receive more light than dense interiors. The upper clouds are sparser and more transparent, with perspective,
distance haze and camera parallax separating both levels. The [volume-compositing reference](https://developer.nvidia.com/gpugems/gpugems/part-vi-beyond-triangles/chapter-39-volume-rendering-techniques)
describes front-to-back optical accumulation; this shader uses procedural density rather than a voxel texture.
Wind advances at four ticks per second without reseeding or temporal jitter. Full view-ray differentials filter detail
at the actual render-target pixel footprint, including overhead views. Cloud lighting and
edge opacity are continuous, with no separate coarse pixel grid, shade bands or sky Bayer pattern. PSX resolution and
color conversion still apply through their existing scene/presentation passes.
`Sky::cloudCoverage` ranges from zero (clear) to one (overcast), defaulting to 0.4 for open blue gaps between clouds.
Cloud lighting follows the scene's directional light and sun color.
Use `Sprite` for an alpha-cutout texture silhouette. Sprite transforms are provided by the caller, so animation is not tied
to the renderer. Flat sprites retain the emissive ripple style. Supplying `Sprite::profile` draws an extruded silhouette
with steady dark outlines, readable textured faces, and directionally shaded sides. Both passes use the same profile mesh.
Effects use additive blending and depth testing without depth writes
or shadow casting. General translucent sorting and glass transmission are outside this
renderer's current scope.

To reuse item extrusion, load a `Texture` with `TextureOptions::retainAlpha = true`, then call `makeSpriteMesh(texture)`
and assign that mesh to `Sprite::profile`. Mesh creation happens once during asset loading. The CPU-only
`engine::assets::extrudeSprite()` also accepts a bottom-first alpha mask without requiring an OpenGL context.
Geometry is centered on the visible alpha bounds, preserves the original UVs, and adds walls along every exposed pixel
edge, including holes. Scale X/Y by the desired texture dimensions and Z by the desired thickness. `Sprite::outlinePixels`
sets the inner outline width in source texels (0–8); `rippleColor` supplies its tint. Keep the mesh alive alongside the texture.
Ordinary textures do not retain a CPU alpha copy.

The game's six collectibles share a constant-speed turn and gentle bob. `GameConfig.h` exposes
`kCollectibleSpinPeriod`, `kCollectibleBobPeriod`, `kCollectibleBobHeight`, `kCollectibleDepth`, and
`kCollectibleOutlinePixels`. The artwork is extruded at load time; the PNG assets are used directly.

Collectibles also emit green (score) or red (slowdown) ripples outside their silhouettes. A cached `SpriteOutline`
stores a padded distance field generated from the texture alpha. Outgoing waves follow constant-distance contours,
inherit the sprite's rotation and bob, and fade to zero at the outer radius. Their additive pass runs after opaque
objects with depth testing, no depth writes, and no shadow casting. The textured faces retain their steady shading.
The game's ripple masks ignore isolated alpha specks without shifting the artwork's center.

`kCollectibleRippleRadiusPixels`, `kCollectibleRipplePeriod`, `kCollectibleRippleWidthPixels`, and
`kCollectibleRippleOpacity` in `GameConfig.h` control reach, timing, thickness, and strength. Pixel distances refer to
the source texture, so padding supports effects beyond image borders and keeps the width proportional across modes.
To reuse the effect, build a `SpriteOutline` from a texture loaded with `retainAlpha`, keep it alive beside the texture,
and assign it to `Sprite::ripple.shape`. The ripple uses `Sprite::rippleColor`; setting its opacity to zero disables it.
The CPU-only `makeAlphaDistanceField()` uses a separable Euclidean distance transform; no field is rebuilt per frame.
`Texture::fromRedChannel()` uploads such scalar data with linear filtering and no mipmaps.

On collection, the collision update immediately records the food's texture index, spawn time, position and pickup time.
`WorldScene` uses the same sprite builder for the live item and its `SpriteBurst`, preserving its exact height, bob,
rotation, proportions, tint, and initial ripple phase. The pose stays fixed throughout the pop. A brief silhouette flash
starts at full brightness, and the existing contour waves expand with fast initial motion and a smooth fade.
`kCollectBurstDuration` (0.28 seconds), `kCollectBurstWidthPixels`, and `kCollectBurstOpacity` configure the response.

`Scene::effects` accepts either additive `Quad` commands or `SpriteBurst` commands. Bursts reuse the cached outline
field and ripple shader, with no particle mesh generation or per-frame asset uploads. Set `SpriteBurst::progress` from
0 to 1; the renderer clips finished bursts and does not wrap their animation. Effects remain depth-tested and cast no shadows.
`updateCollectEffects()` advances existing feedback before collisions, so a newly collected item starts at age zero,
including pickups that end the game. The optional `effectRemainder` argument to `Renderer::render()` and
`WorldScene::build()` supplies seconds since the latest simulation tick for smooth rendering between updates.

`Scene::particles` holds alpha-blended quads drawn after opaque objects, sorted back to front for each camera. They
depth-test against the scene without writing depth or casting shadows, and participate in local window captures.
Texture cutouts use `Quad::alphaCutoff`; whole-particle fading uses `Quad::opacity`. The PSX pass removes their coverage
from the color-dither mask, preserving transparent holes and avoiding dithering on the leaf sprites.
`WorldScene` gives each tree a stable emitter identity. Falling leaves use seeded flutter and game time; curbside litter
uses a swept player path in scrolling track coordinates. Lifted leaves tumble and fade once, retain their spent identity
while their lot remains visible, and reset when time or scroll restarts. The visible leaf budget keeps the nearest leaves.
`SceneryLayout.h` shares building lot geometry with parks: two frontage columns are reserved for larger lawns, and
consecutive park lots extend to a shared tile boundary. `Scenery.cpp` draws their combined occupancy on one grass grid,
omitting fences wherever another lawn cell adjoins it. Shared UV phase and tint remove visible lot seams; widths,
outer corner notches, sidewalk entrances and per-lot tree groups preserve variation. Neighbor occupancy is evaluated
outside the visible lot range so scrolling never adds a divider through an existing park. Separate lots retain gaps, and
brick forecourt gardens are split around a clear entrance corridor. Gardens anchor to the straight facade, with partial
grass tiles filling their exact width and no fence against the building wall. Texture repeats follow the trimmed width.
Model scale is limited by neighboring facade clearance.

The directional light's `ShadowVolume` is a stable world-space orthographic region. Size it to cover the scene's casters
and receivers. `RenderSettings` chooses shadow-map resolution, retro resolution, vertex snapping, and model texture distortion.
The camera and lighting data belong to each scene; the render target determines its projection aspect ratio and texture LOD.

`Renderer::renderMenu()` uses a cached copy of the opening `WorldScene` at player X, road scroll and world time zero.
Its scene commands borrow the same persistent game assets as gameplay. Only `MenuOverlay::time` advances for the selection
pulse; clouds, lighting, heat distortion and scenery remain frozen. The application does not advance the simulation while
the menu is open, and Play resets the game before starting it.
`SceneRenderer` composites `Scene::menu` onto the completed camera image after geometry, effects and retro/lens
presentation. Transparent pixels preserve the game view; outlined bitmap text and a translucent selection panel keep the
options readable. The overlay fits the same content rectangle as the scene, including console letterboxing, and never
writes scene depth.

Shaders in `shaders/` are embedded by `cmake/EmbedShaders.cmake`. Editing a shader triggers regeneration on the next build,
and running binaries do not depend on shader files or the working directory. Shared shadow sampling is in `lighting.glsl`;
shared vertex snapping is in `vertex_snap.glsl`. `ShaderProgram` inserts shared sources after each stage's `#version` line.
`Programs.h` resolves required uniform locations once; changing a uniform's name or use requires updating its binding there.

## Retro rendering

`SceneRenderer::setMode(RenderMode::Normal/Psx)` selects the style; `mode()` reports it.
The compatibility `setRetroMode(bool)` and `retroMode()` select/query PSX only. The game facade exposes
`toggleRetroMode()` (G) and `renderMode()`. G switches between PSX and Normal. Restarting preserves the selected style.
`toggleDistanceFog()` (F) changes the stored PSX fog preference; `distanceFog()` reports it. The reusable equivalents are
`SceneRenderer::setPsxDistanceFog(bool)` and `psxDistanceFog()`. The window title shows fog when enabled in PSX mode;
restarting or changing modes preserves the preference. The reusable renderer defaults to fog off; the game enables it.
`toggleDitheredShadows()` (H) changes the stored PSX shadow preference; `ditheredShadows()` reports it. The window title
shows the effect when enabled in PSX mode, and restarting or changing modes preserves the preference.

### PSX style

The reusable renderer defaults to a 240 × 320 portrait PSX target. The game currently passes 480 × 640 from
`GameConfig::kPsxInternalWidth` and `kPsxInternalHeight`. Nearest-neighbor presentation preserves the native pixels. The integer screen grid is independent of window
size. The tuned preset blends 45% toward snapped coordinates, limiting vertex displacement to 0.225 native pixels per axis.
The framebuffer origin anchors the grid, including at odd resolutions.
Snapping changes clip-space XY only; depth remains continuous. Vertices before the near plane bypass snapping so the GPU
can clip them safely, and the fullscreen sky remains fixed. There is no random camera or vertex jitter.

Textured quads, models, and sprites retain GLSL `noperspective` UVs, blended with perspective interpolation at a tuned
`retroAffineStrength` of 0.35. Each quad/sprite's `affineStrength`, or `modelAffineStrength` for models, multiplies that shared
intensity. Setting both factors to 1 selects full affine mapping; 0 disables it. These controls apply to PSX only.
`psxSubdivideQuads` defaults to true: authored surfaces larger than 2 world units use cached 2 × 2 grids; those larger than 6
use 4 × 4 grids. Smaller quads retain two triangles. Topology depends on the surface's `worldSize`, never its distance or
projected screen area, so camera movement cannot cause subdivision/UV popping. Disable subdivision for coarse triangles.
The road also remains subdivided to follow the track curve. Animated heat distortion is disabled in retro mode.

Models and cubes use 8-bit vertex lighting, interpolated in screen space (Gouraud), while normal mode retains per-pixel normals.
`retro.frag` converts composed 8-bit RGB to 5-bit channels, saturating before dropping three bits. The PS1's signed 4 × 4
dither offsets apply selectively to gradual color/shade transitions. A two-pixel native neighborhood measures variation and
curvature per RGB channel: flat channels, isolated shade steps and alternating texels suppress dither. High-contrast edges
also reject it, keeping silhouettes and window frames crisp. This selection is a screen-space style heuristic; it never
blurs or blends neighboring colors. The retro target's alpha stores dither eligibility: visible sprite fragments,
including extruded faces, outlines, ripples and collection bursts, clear it while retaining their normal color blending.
Depth testing and alpha cutouts preserve dithering on the world behind sprites. Sprites still use RGB555 quantization.
Dither is anchored to top-down native framebuffer pixels before lens mapping and
enlargement, so resizing never changes its phase. OpenGL driver dithering is disabled in both console modes.
The affine interpolation, vertex shades and signed dither pattern follow the
[PS1 GPU specifications](https://psx-spx.consoledev.net/ps1/gpu/misc/).

Shadow transitions use an additional 4 × 4 ordered coverage pattern in `lighting.glsl`. The existing nine-tap filtered
visibility determines the density of lit pixels within the shadow edge. Resolved edge cells mix lit and shadowed lighting
spatially; fully lit and fully shadowed regions remain solid. The pattern uses scaled mesh coordinates with a fixed density
of 48 cells per world unit on the surface's dominant local plane. Translation, rotation, camera motion and light movement
cannot reseed its phase. Subpixel cells smoothly fade into filtered visibility without switching grids, and a soft threshold
reduces sudden pixel flips from tiny coverage changes. This works on plain, textured, building, cube and model surfaces,
before lens mapping and enlargement. Coverage dithering is a stylized shadow treatment, not PS1 shadow hardware.
In the dithered path, receiver-plane depth gradients adjust each comparison tap and its half-texel footprint to avoid
turning self-shadow errors on sloped, fully lit surfaces into stippled acne.
Normal mode retains their filtered shadow visibility.

`RenderSettings::psxDitheredShadows` enables an optional Saturn-inspired transparency treatment for the entire shadow,
including its fully shadowed interior. It defaults to false; the game initializes it from `GameConfig::kPsxDitheredShadows`.
`SceneRenderer::setPsxDitheredShadows(bool)` changes it without recreating GPU resources, and `psxDitheredShadows()` reports
the stored preference. It is applied only in PSX mode and is independent of `retroDitherStrength`.
The [VDP1 mesh rule](https://www.infochunk.com/saturn/segahtml_en/hard/vdp1/hon/p06_33.htm) inspires its alternating-pixel coverage:
an even X + Y sum keeps the filtered shadow visibility, and an odd sum exposes the actual fully lit material. The receiver
still writes its normal color and depth; no geometry is discarded or made transparent. This preserves base textures,
with fully lit ambient, sun and material reflections restored in the holes. Covered pixels also attenuate ambient/sky fill
so the half-coverage shadow stays readable on reflective materials and in bright ambient lighting.
`RenderSettings::psxDitheredShadowDarkness` controls this additional attenuation in [0, 1], defaulting to 0.8; the game
initializes it from `GameConfig::kPsxDitheredShadowDarkness`. Zero restores the lighter ambient-filled shadows, while one
removes ambient/sky fill from fully occluded, sun-facing mesh pixels. PBR attenuation happens in linear space before
display conversion, preserving the material's tinted response. Filtered shadow edges fade the attenuation, and sun-away
faces retain uniform ambient light, with a gradual fade near grazing sun angles to avoid self-occlusion checkerboards.
Both settings leave Normal lighting and the ordinary shadow path unchanged.
The checkerboard is anchored to native framebuffer pixels before the lens warp and nearest enlargement, so time,
surface motion, shadow-map resolution and texture tiling cannot reseed it. The ordinary surface-anchored shadow-edge
pattern resumes when the toggle is off. Receiver-plane depth correction is active for either shadow-dither treatment.

Retro `TextureSampler` overrides select nearest base-level texels without modifying assets' filtering parameters.
Repeating surfaces retain UV tiling, and clamped textures retain their edges. Normal mode uses each texture's original
filtering/mipmaps and perspective-correct UVs. Shadow rendering retains its independent, unsnapped projection and sampling.

Textured quads can opt into PSX distance filtering with `Quad::psxMinificationLodBias`. The game enables it only on sidewalks,
using `GameConfig::kPsxSidewalkLodBias`. A perspective UV footprint selects the mip levels independently of affine UV
warping; nearest texel fetches within each mip retain hard edges. A continuous blend between adjacent levels avoids mip
popping, without bilinear texel filtering or moving sample patterns. Magnification keeps the base texels when the biased
footprint is below one texel per pixel. Lower biases retain more grain; higher biases suppress more shimmer. Finite biases
are clamped to [-2, 2]; an empty or nonfinite value disables the opt-in. Single-level textures safely retain their only level.
This is a deliberate addition for the PSX/PBR hybrid style. Normal sampling is unchanged, and all other surfaces
retain the original nearest base-level PSX path. The material program resets this opt-in before drawing props or cubes.

Tune `RenderSettings::retroSize` to change the PSX raster. `retroPixelStep` is measured in target pixels:
1 is the integer-grid default, 0.5 uses a finer grid, 2 makes it coarser, and 0 disables snapping. `retroSnapStrength` controls
the amount of displacement: 0 disables it, 0.45 is the restrained preset, and 1 is full snapping. `retroAffineStrength` defaults
to 0.35 to avoid rubbery texture deformation on large facades. Both intensities and model affine strength must be finite and
within [0, 1]; pixel step must be finite/nonnegative and dimensions positive. These are style controls,
not a hardware emulator.

`RenderSettings::retroDitherStrength` scales the signed offsets within selected color transitions and the ordered shadow-edge
blend: 1 is the default, and 0 disables both while retaining filtered shadows and RGB555 quantization.
Values must be finite and within [0, 1]. The game passes
`GameConfig::kPsxDitherStrength` to this setting. Normal presentation is unaffected.

### PSX distance fog

`RenderSettings::psxDistanceFog` enables gentle distance cueing only in PSX mode. The game forwards
its initial preference from `GameConfig::kPsxDistanceFog`; F toggles it independently of color/shadow dithering.
`psxFogStart` and `psxFogEnd` default to camera-space distances 45 and 125, keeping the player and nearby street clear while
hiding distant road/building detail. Start must be finite and nonnegative; end must be finite and greater than start.
Use the boolean preference to disable fog without changing its range.

The vertex stage computes a linear distance cue from clip W, rounds it to eight bits, and interpolates it with
`noperspective` just like the PSX vertex shades. `fog.glsl` uses that linear cue directly for a gradual fade across the
configured range. Completed material colors blend into the fog; distant specular and
shadows fade together, while roughness, metallic response and glass reflections remain active nearby.
The existing selective RGB555 presentation dithers gradual fog transitions using native framebuffer coordinates;
there is no separate animated noise, transparency mesh, or random sample phase. Disabling color dithering leaves fog active.

`psxFogFollowsSky` defaults to true: the fade targets `Scene::sky.bottom` by day and the sky's regular purple dusk horizon
by night, following its five-step palette blend. This keeps the runner's sky, clouds and material environment colors
independent of the distance effect. Turning fog on preserves their existing brightness and color.
Set `psxFogFollowsSky` to false for a custom distance tint; `psxFogColor` defaults to pale blue `{0.74, 0.86, 0.97}` and
`psxNightFogColor` to `{0.38, 0.31, 0.40}`. Both colors require finite channels in [0, 1]. Custom tints also affect only
distant surfaces. The game exposes corresponding `GameConfig::kPsxFogStart`, `kPsxFogEnd`, `kPsxFogFollowsSky`,
`kPsxFogColor` and `kPsxNightFogColor` controls.
Sprites keep opaque cutouts; additive ripples and collection pops fade their opacity instead of adding fog-colored quads.
Menu text is composited after fog and presentation, so it stays clear over the frozen scene.
Disabling fog restores the unfaded distance appearance; Normal mode ignores the fog preference.

`RenderTarget(size, Kind::ColorDepth, samples)` still supports multisample attachments and a resolve framebuffer for other
renderer clients. Call `bind()` to draw, then `resolve()` before sampling `texture()` or reading pixels. `resolve()` leaves
the resolved framebuffer bound; `samples()` reports the allocated count. Shadow targets remain single-sample and use the
same projection in both styles. `GameConfig::kMipmappedTextures` controls the game asset sampler defaults.

### Camera lens and hardware limits

`Camera::fisheyeStrength` defaults to 0 for reusable scenes. The game sets it from `GameConfig::kCameraFisheyeStrength`.
Presentation applies an aspect-correct inverse barrel warp once to the camera image, fits it inside the original
rectangle without empty corners, and preserves letterboxing. Strength is clamped to [0, 0.25]; nonfinite values disable it.
Menus draw after the lens pass so text stays straight while the game view keeps its camera lens. Normal mode lazily
allocates and reuses a drawable-sized camera target only when the lens is active; strength 0 retains its direct rendering
path. PSX reuses its existing retro target and nearest pixel presentation.
`Camera::viewProjection()` remains a linear perspective matrix.

The PSX path uses the console's distinguishing raster, interpolation and color methods within the existing OpenGL
renderer, with restrained warp/snap defaults. It is not a hardware emulator: a modern depth buffer replaces ordering-table
visibility, the procedural sky, fog, shadows and PBR-like materials remain shader effects, and RGB555 quantization occurs
after scene composition. The fisheye lens is a separate camera effect.

## Adding or changing features

- **Game rules:** edit `Simulation` or the collision operation and add CPU tests in `GameplayTests.cpp`.
- **Assets:** extend the game catalog enums and descriptor tables; pass a different root to `Renderer` for alternate assets.
- **World scenery:** add a builder method that emits generic scene commands. Keep GL calls inside the graphics library.
- **Drawing primitives:** extend `DrawCommand`, the corresponding color/shadow handlers in `SceneRenderer`, and graphics tests.
- **Rendering style:** edit GLSL and its program bindings. Keep game events and procedural layout out of shaders and graphics code.
- **A related game:** link the engine targets, create its own simulation and scene builder, and use the standalone example as a starting point.

## Model import scope

The importer handles glTF JSON with one external binary buffer, an active scene hierarchy, indexed triangle primitives,
FLOAT positions/normals/UVs, unsigned indices, node transforms, base-color textures/factors, scalar roughness/metallic
factors, and external metallic/roughness, normal and occlusion textures using `TEXCOORD_0`.
Both material factors default to 1; metallic/roughness maps use linear G/B channels, tangent-space normal maps use RGB,
and occlusion uses linear R, as specified by [glTF 2.0](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#materials).
Normal scale and occlusion strength are retained, with shared metallic/roughness and occlusion images uploaded once.
Authored `MASK` alpha cutoffs and `doubleSided` lighting are retained, including cutout model shadows using UV0.
Base-color samplers that request nearest minification and magnification retain that sampling; repeating S/T wrap is
also retained. Textured `Quad` commands
can set `alphaCutoff` and `doubleSided` for thin fences: the cutout follows UV tiling/offset in color and shadows, and
is excluded from the opaque reflection depth prepass so transparent gaps reveal the captured world.
Non-numeric or out-of-range factors/occlusion strength, nonfinite normal scale, invalid image references and unsupported
UV sets are rejected. It centers X/Z, grounds Y,
and normalizes to the requested height. Missing normals are generated from triangle geometry. Accessor sizes, byte ranges, layouts, indices, and node cycles are checked before use.
Errors are reported as exceptions rather than dereferencing missing or out-of-range data.

GLB containers, embedded/data-URI buffers/images, multiple buffers, sparse accessors, compressed meshes, animations,
skinning, extra UV sets, emissive maps, and full PBR extensions/blended alpha materials are not implemented.
Add a format adapter producing `assets::ModelData` to extend import without changing GPU ownership or scene submission.
The bundled model licenses remain with their original files.

## Migration from the original layout

`Game.h`, `Renderer.h`, and `GameConfig.h` remain game entry points. `Renderer::initSDL()` forwards to `initialize()` for
existing callers; rendering accepts spans, including vectors. GPU model use moves from the removed `GLTFModel.h` to
`engine/graphics/Model.h`: construct a `Model(path, height)` or upload `assets::ModelData` instead of calling `load()`/`release()`.
Destructors own cleanup. Rendering internals are no longer fields on the public game facade.
