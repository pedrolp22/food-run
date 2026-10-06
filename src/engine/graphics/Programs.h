#pragma once

#include "ShaderSources.h"
#include "engine/graphics/ShaderProgram.h"
#include <array>
#include <string>

namespace engine::graphics::detail {
inline const std::string litSource = std::string(shaders::fog_glsl) + std::string(shaders::lighting_glsl);
inline const std::string litTextureSource = litSource + std::string(shaders::texture_filter_glsl);
inline const std::string materialTextureSource =
    litTextureSource + std::string(shaders::material_glsl) + std::string(shaders::material_textures_glsl);
inline const std::string skySource = std::string(shaders::sky_panorama_glsl) + std::string(shaders::sky_clouds_glsl);
struct MaterialMapUniforms {
    int properties;
    int normal;
    int occlusion;
    int hasProperties;
    int hasNormal;
    int hasOcclusion;
    int packedReflectivity;
    int retro;
    int maxPropertiesLod;
    int maxNormalLod;
    int maxOcclusionLod;
    int normalScale;
    int occlusionStrength;
    explicit MaterialMapUniforms(const ShaderProgram& shader)
        : properties(shader.uniform("uMaterialMap")), normal(shader.uniform("uNormalMap")),
          occlusion(shader.uniform("uOcclusionMap")), hasProperties(shader.uniform("uHasMaterialMap")),
          hasNormal(shader.uniform("uHasNormalMap")), hasOcclusion(shader.uniform("uHasOcclusionMap")),
          packedReflectivity(shader.uniform("uPackedReflectivity")), retro(shader.uniform("uMaterialRetro")),
          maxPropertiesLod(shader.uniform("uMaxMaterialLod")), maxNormalLod(shader.uniform("uMaxNormalLod")),
          maxOcclusionLod(shader.uniform("uMaxOcclusionLod")), normalScale(shader.uniform("uNormalScale")),
          occlusionStrength(shader.uniform("uOcclusionStrength")) {}
};
struct PlainProgram {
    ShaderProgram shader;
    int mvp;
    int model;
    int reflectionClipPlane;
    int lightViewProj;
    int snap;
    int snapStrength;
    int fogRange;
    int fogColor;
    int color;
    int alpha;
    int light;
    int ambientLight;
    int receivesShadow;
    int shadowMap;
    int shadowDitherStrength;
    int ditheredShadows;
    int ditheredShadowDarkness;
    PlainProgram()
        : shader(shaders::plain_vert, shaders::plain_frag, "plain", litSource, shaders::vertex_snap_glsl),
          mvp(shader.uniform("uMvp")), model(shader.uniform("uModel")),
          reflectionClipPlane(shader.uniform("uReflectionClipPlane")), lightViewProj(shader.uniform("uLightViewProj")),
          snap(shader.uniform("uSnap")), snapStrength(shader.uniform("uSnapStrength")),
          fogRange(shader.uniform("uFogRange")), fogColor(shader.uniform("uFogColor")), color(shader.uniform("uColor")),
          alpha(shader.uniform("uAlpha")), light(shader.uniform("uLight")),
          ambientLight(shader.uniform("uAmbientLight")), receivesShadow(shader.uniform("uReceivesShadow")),
          shadowMap(shader.uniform("uShadowMap")), shadowDitherStrength(shader.uniform("uShadowDitherStrength")),
          ditheredShadows(shader.uniform("uDitheredShadows")),
          ditheredShadowDarkness(shader.uniform("uDitheredShadowDarkness")) {}
};

struct SpriteProgram {
    ShaderProgram shader;
    int mvp;
    int model;
    int reflectionClipPlane;
    int extruded;
    int outlinePixels;
    int sunDir;
    int snap;
    int snapStrength;
    int fogRange;
    int fogColor;
    int texture;
    int time;
    int spawnTime;
    int rippleColor;
    int aspect;
    int affine;
    SpriteProgram()
        : shader(shaders::sprite_vert, shaders::sprite_frag, "sprite",
                 std::string(shaders::fog_glsl) + std::string(shaders::texture_filter_glsl), shaders::vertex_snap_glsl),
          mvp(shader.uniform("uMvp")), model(shader.uniform("uModel")),
          reflectionClipPlane(shader.uniform("uReflectionClipPlane")), extruded(shader.uniform("uExtruded")),
          outlinePixels(shader.uniform("uOutlinePixels")), sunDir(shader.uniform("uSunDir")),
          snap(shader.uniform("uSnap")), snapStrength(shader.uniform("uSnapStrength")),
          fogRange(shader.uniform("uFogRange")), fogColor(shader.uniform("uFogColor")),
          texture(shader.uniform("uTexture")), time(shader.uniform("uTime")), spawnTime(shader.uniform("uSpawnTime")),
          rippleColor(shader.uniform("uRippleColor")), aspect(shader.uniform("uAspect")),
          affine(shader.uniform("uAffine")) {}
};

struct RippleProgram {
    ShaderProgram shader;
    int mvp;
    int snap;
    int snapStrength;
    int fogRange;
    int distanceField;
    int color;
    int radius;
    int phase;
    int width;
    int opacity;
    int burst;
    int burstProgress;
    RippleProgram()
        : shader(shaders::ripple_vert, shaders::ripple_frag, "ripple", shaders::fog_glsl, shaders::vertex_snap_glsl),
          mvp(shader.uniform("uMvp")), snap(shader.uniform("uSnap")), snapStrength(shader.uniform("uSnapStrength")),
          fogRange(shader.uniform("uFogRange")), distanceField(shader.uniform("uDistanceField")),
          color(shader.uniform("uColor")), radius(shader.uniform("uRadius")), phase(shader.uniform("uPhase")),
          width(shader.uniform("uWidth")), opacity(shader.uniform("uOpacity")), burst(shader.uniform("uBurst")),
          burstProgress(shader.uniform("uBurstProgress")) {}
};

struct CloudSkyUniforms {
    int enabled;
    int topColor;
    int horizonColor;
    int cloudTint;
    int seed;
    int time;
    int stars;
    int twinkle;
    std::array<int, 3> textures;
    std::array<int, 3> layouts;
    std::array<int, 3> settings;
    std::array<int, 3> variation;
    std::array<int, 3> warp;
    explicit CloudSkyUniforms(const ShaderProgram& shader)
        : enabled(shader.uniform("uHasCloudSky")), topColor(shader.uniform("uCloudSkyTop")),
          horizonColor(shader.uniform("uCloudSkyHorizon")), cloudTint(shader.uniform("uCloudTint")),
          seed(shader.uniform("uSkyLayoutSeed")), time(shader.uniform("uSkyEffectTime")),
          stars(shader.uniform("uNightStars")), twinkle(shader.uniform("uStarTwinkle")) {
        for (std::size_t index = 0; index < textures.size(); ++index) {
            const auto suffix = "[" + std::to_string(index) + "]";
            textures[index] = shader.uniform(("uCloudTextures" + suffix).c_str());
            layouts[index] = shader.uniform(("uCloudLayouts" + suffix).c_str());
            settings[index] = shader.uniform(("uCloudSettings" + suffix).c_str());
            variation[index] = shader.uniform(("uCloudVariation" + suffix).c_str());
            warp[index] = shader.uniform(("uCloudWarp" + suffix).c_str());
        }
    }
};

struct SkyProgram {
    ShaderProgram shader;
    int panorama;
    int panoramaOffset;
    int hasPanorama;
    int topColor;
    int bottomColor;
    int time;
    int nightBlend;
    int sunDir;
    int aspect;
    int rasterSize;
    int viewBounds;
    int cloudCoverage;
    int cameraForward;
    int cameraRight;
    int cameraUp;
    int cameraPosition;
    int tanHalfFov;
    int sunColor;
    int sunRadii;
    CloudSkyUniforms clouds;
    SkyProgram()
        : shader(shaders::sky_vert, shaders::sky_frag, "sky", skySource), panorama(shader.uniform("uPanorama")),
          panoramaOffset(shader.uniform("uPanoramaOffset")), hasPanorama(shader.uniform("uHasPanorama")),
          topColor(shader.uniform("uTopColor")), bottomColor(shader.uniform("uBottomColor")),
          time(shader.uniform("uTime")), nightBlend(shader.uniform("uNightBlend")), sunDir(shader.uniform("uSunDir")),
          aspect(shader.uniform("uAspect")), rasterSize(shader.uniform("uRasterSize")),
          viewBounds(shader.uniform("uViewBounds")), cloudCoverage(shader.uniform("uCloudCoverage")),
          cameraForward(shader.uniform("uCameraForward")), cameraRight(shader.uniform("uCameraRight")),
          cameraUp(shader.uniform("uCameraUp")), cameraPosition(shader.uniform("uCameraPosition")),
          tanHalfFov(shader.uniform("uTanHalfFov")), sunColor(shader.uniform("uSunColor")),
          sunRadii(shader.uniform("uSunRadii")), clouds(shader) {}
};

struct MenuProgram {
    ShaderProgram shader;
    int mvp;
    int time;
    int selection;
    MenuProgram()
        : shader(shaders::menu_vert, shaders::menu_frag, "menu"), mvp(shader.uniform("uMvp")),
          time(shader.uniform("uTime")), selection(shader.uniform("uSelection")) {}
};

struct RoadProgram {
    ShaderProgram shader;
    int mvp;
    int model;
    int reflectionClipPlane;
    int lightViewProj;
    int texScale;
    int texOffset;
    int snap;
    int snapStrength;
    int fogRange;
    int fogColor;
    int texture;
    int textureRepeat;
    int maxTextureLod;
    int psxMinification;
    int psxLodBias;
    int lod;
    int light;
    int ambientLight;
    int alpha;
    int alphaCutoff;
    int time;
    int heat;
    int affine;
    int receivesShadow;
    int color;
    int shadowMap;
    int shadowDitherStrength;
    int ditheredShadows;
    int ditheredShadowDarkness;
    RoadProgram()
        : shader(shaders::road_vert, shaders::road_frag, "road", litTextureSource, shaders::vertex_snap_glsl),
          mvp(shader.uniform("uMvp")), model(shader.uniform("uModel")),
          reflectionClipPlane(shader.uniform("uReflectionClipPlane")), lightViewProj(shader.uniform("uLightViewProj")),
          texScale(shader.uniform("uTexScale")), texOffset(shader.uniform("uTexOffset")), snap(shader.uniform("uSnap")),
          snapStrength(shader.uniform("uSnapStrength")), fogRange(shader.uniform("uFogRange")),
          fogColor(shader.uniform("uFogColor")), texture(shader.uniform("uTexture")),
          textureRepeat(shader.uniform("uTextureRepeat")), maxTextureLod(shader.uniform("uMaxTextureLod")),
          psxMinification(shader.uniform("uPsxMinification")), psxLodBias(shader.uniform("uPsxLodBias")),
          lod(shader.uniform("uLod")), light(shader.uniform("uLight")), ambientLight(shader.uniform("uAmbientLight")),
          alpha(shader.uniform("uAlpha")), alphaCutoff(shader.uniform("uAlphaCutoff")), time(shader.uniform("uTime")),
          heat(shader.uniform("uHeat")), affine(shader.uniform("uAffine")),
          receivesShadow(shader.uniform("uReceivesShadow")), color(shader.uniform("uColor")),
          shadowMap(shader.uniform("uShadowMap")), shadowDitherStrength(shader.uniform("uShadowDitherStrength")),
          ditheredShadows(shader.uniform("uDitheredShadows")),
          ditheredShadowDarkness(shader.uniform("uDitheredShadowDarkness")) {}
};

struct SkyscraperProgram {
    ShaderProgram shader;
    int mvp;
    int model;
    int reflectionClipPlane;
    int lightViewProj;
    int texScale;
    int texOffset;
    int facadeVerticalRange;
    int snap;
    int snapStrength;
    int fogRange;
    int fogColor;
    int texture;
    int textureRepeat;
    int maxTextureLod;
    int psxMinification;
    int psxLodBias;
    int lod;
    int ambientLight;
    int alpha;
    int alphaCutoff;
    int affine;
    int receivesShadow;
    int color;
    int shadowMap;
    int shadowDitherStrength;
    int ditheredShadows;
    int ditheredShadowDarkness;
    int faceNormal;
    int sunDir;
    int cameraPosition;
    int skyColor;
    int sunColor;
    int surfaceRoughness;
    int surfaceMetallic;
    int surfaceReflectivity;
    int windowRoughness;
    int windowReflectivity;
    int hasWindows;
    int glassMap;
    int hasGlassMap;
    int maxGlassLod;
    int worldReflection;
    int hasWorldReflection;
    int worldReflectionMaxLod;
    int planarReflection;
    int hasPlanarReflection;
    int planarReflectionViewProjection;
    int planarReflectionMaxLod;
    int planarReflectionStrength;
    int windowReflectance;
    int windowBackingTint;
    int panorama;
    int panoramaOffset;
    int hasSkyPanorama;
    int skyPanoramaNight;
    int haze;
    int hazeColor;
    MaterialMapUniforms maps;
    CloudSkyUniforms clouds;
    SkyscraperProgram()
        : shader(shaders::skyscraper_vert, shaders::skyscraper_frag, "skyscraper", materialTextureSource + skySource,
                 shaders::vertex_snap_glsl),
          mvp(shader.uniform("uMvp")), model(shader.uniform("uModel")),
          reflectionClipPlane(shader.uniform("uReflectionClipPlane")), lightViewProj(shader.uniform("uLightViewProj")),
          texScale(shader.uniform("uTexScale")), texOffset(shader.uniform("uTexOffset")),
          facadeVerticalRange(shader.uniform("uFacadeVerticalRange")), snap(shader.uniform("uSnap")),
          snapStrength(shader.uniform("uSnapStrength")), fogRange(shader.uniform("uFogRange")),
          fogColor(shader.uniform("uFogColor")), texture(shader.uniform("uTexture")),
          textureRepeat(shader.uniform("uTextureRepeat")), maxTextureLod(shader.uniform("uMaxTextureLod")),
          psxMinification(shader.uniform("uPsxMinification")), psxLodBias(shader.uniform("uPsxLodBias")),
          lod(shader.uniform("uLod")), ambientLight(shader.uniform("uAmbientLight")), alpha(shader.uniform("uAlpha")),
          alphaCutoff(shader.uniform("uAlphaCutoff")), affine(shader.uniform("uAffine")),
          receivesShadow(shader.uniform("uReceivesShadow")), color(shader.uniform("uColor")),
          shadowMap(shader.uniform("uShadowMap")), shadowDitherStrength(shader.uniform("uShadowDitherStrength")),
          ditheredShadows(shader.uniform("uDitheredShadows")),
          ditheredShadowDarkness(shader.uniform("uDitheredShadowDarkness")), faceNormal(shader.uniform("uFaceNormal")),
          sunDir(shader.uniform("uSunDir")), cameraPosition(shader.uniform("uCameraPosition")),
          skyColor(shader.uniform("uSkyColor")), sunColor(shader.uniform("uSunColor")),
          surfaceRoughness(shader.uniform("uSurfaceRoughness")), surfaceMetallic(shader.uniform("uSurfaceMetallic")),
          surfaceReflectivity(shader.uniform("uSurfaceReflectivity")),
          windowRoughness(shader.uniform("uWindowRoughness")),
          windowReflectivity(shader.uniform("uWindowReflectivity")), hasWindows(shader.uniform("uHasWindows")),
          glassMap(shader.uniform("uGlassMap")), hasGlassMap(shader.uniform("uHasGlassMap")),
          maxGlassLod(shader.uniform("uMaxGlassLod")), worldReflection(shader.uniform("uWorldReflection")),
          hasWorldReflection(shader.uniform("uHasWorldReflection")),
          worldReflectionMaxLod(shader.uniform("uWorldReflectionMaxLod")),
          planarReflection(shader.uniform("uPlanarReflection")),
          hasPlanarReflection(shader.uniform("uHasPlanarReflection")),
          planarReflectionViewProjection(shader.uniform("uPlanarReflectionViewProjection")),
          planarReflectionMaxLod(shader.uniform("uPlanarReflectionMaxLod")),
          planarReflectionStrength(shader.uniform("uPlanarReflectionStrength")),
          windowReflectance(shader.uniform("uWindowReflectance")),
          windowBackingTint(shader.uniform("uWindowBackingTint")), panorama(shader.uniform("uPanorama")),
          panoramaOffset(shader.uniform("uPanoramaOffset")), hasSkyPanorama(shader.uniform("uHasSkyPanorama")),
          skyPanoramaNight(shader.uniform("uSkyPanoramaNight")), haze(shader.uniform("uHaze")),
          hazeColor(shader.uniform("uHazeColor")), maps(shader), clouds(shader) {}
};

struct MaterialProgram {
    ShaderProgram shader;
    int mvp;
    int model;
    int reflectionClipPlane;
    int lightViewProj;
    int snap;
    int snapStrength;
    int fogRange;
    int fogColor;
    int quad;
    int faceNormal;
    int texture;
    int hasTexture;
    int textureRepeat;
    int maxTextureLod;
    int psxMinification;
    int psxLodBias;
    int texScale;
    int texOffset;
    int lod;
    int color;
    int colorIsLinear;
    int surfaceRoughness;
    int surfaceMetallic;
    int surfaceReflectivity;
    int sunDir;
    int ambient;
    int cameraPosition;
    int skyColor;
    int sunColor;
    int hazeColor;
    int vertexLighting;
    int shadowMap;
    int shadowDitherStrength;
    int ditheredShadows;
    int ditheredShadowDarkness;
    int receivesShadow;
    int affine;
    int alpha;
    int alphaCutoff;
    int baseAlpha;
    int doubleSided;
    int haze;
    int time;
    int heat;
    MaterialMapUniforms maps;
    MaterialProgram()
        : shader(shaders::material_vert, shaders::material_frag, "material", materialTextureSource,
                 shaders::vertex_snap_glsl),
          mvp(shader.uniform("uMvp")), model(shader.uniform("uModel")),
          reflectionClipPlane(shader.uniform("uReflectionClipPlane")), lightViewProj(shader.uniform("uLightViewProj")),
          snap(shader.uniform("uSnap")), snapStrength(shader.uniform("uSnapStrength")),
          fogRange(shader.uniform("uFogRange")), fogColor(shader.uniform("uFogColor")), quad(shader.uniform("uQuad")),
          faceNormal(shader.uniform("uFaceNormal")), texture(shader.uniform("uTexture")),
          hasTexture(shader.uniform("uHasTexture")), textureRepeat(shader.uniform("uTextureRepeat")),
          maxTextureLod(shader.uniform("uMaxTextureLod")), psxMinification(shader.uniform("uPsxMinification")),
          psxLodBias(shader.uniform("uPsxLodBias")), texScale(shader.uniform("uTexScale")),
          texOffset(shader.uniform("uTexOffset")), lod(shader.uniform("uLod")), color(shader.uniform("uColor")),
          colorIsLinear(shader.uniform("uColorIsLinear")), surfaceRoughness(shader.uniform("uSurfaceRoughness")),
          surfaceMetallic(shader.uniform("uSurfaceMetallic")),
          surfaceReflectivity(shader.uniform("uSurfaceReflectivity")), sunDir(shader.uniform("uSunDir")),
          ambient(shader.uniform("uAmbient")), cameraPosition(shader.uniform("uCameraPosition")),
          skyColor(shader.uniform("uSkyColor")), sunColor(shader.uniform("uSunColor")),
          hazeColor(shader.uniform("uHazeColor")), vertexLighting(shader.uniform("uVertexLighting")),
          shadowMap(shader.uniform("uShadowMap")), shadowDitherStrength(shader.uniform("uShadowDitherStrength")),
          ditheredShadows(shader.uniform("uDitheredShadows")),
          ditheredShadowDarkness(shader.uniform("uDitheredShadowDarkness")),
          receivesShadow(shader.uniform("uReceivesShadow")), affine(shader.uniform("uAffine")),
          alpha(shader.uniform("uAlpha")), alphaCutoff(shader.uniform("uAlphaCutoff")),
          baseAlpha(shader.uniform("uBaseAlpha")), doubleSided(shader.uniform("uDoubleSided")),
          haze(shader.uniform("uHaze")), time(shader.uniform("uTime")), heat(shader.uniform("uHeat")), maps(shader) {}
};

struct ModelProgram {
    ShaderProgram shader;
    int mvp;
    int model;
    int reflectionClipPlane;
    int lightViewProj;
    int snap;
    int snapStrength;
    int fogRange;
    int fogColor;
    int texture;
    int baseColor;
    int hasTexture;
    int alphaCutoff;
    int baseAlpha;
    int doubleSided;
    int sunDir;
    int ambient;
    int affine;
    int vertexLighting;
    int shadowMap;
    int shadowDitherStrength;
    int ditheredShadows;
    int ditheredShadowDarkness;
    ModelProgram()
        : shader(shaders::model_vert, shaders::model_frag, "model", litTextureSource, shaders::vertex_snap_glsl),
          mvp(shader.uniform("uMvp")), model(shader.uniform("uModel")),
          reflectionClipPlane(shader.uniform("uReflectionClipPlane")), lightViewProj(shader.uniform("uLightViewProj")),
          snap(shader.uniform("uSnap")), snapStrength(shader.uniform("uSnapStrength")),
          fogRange(shader.uniform("uFogRange")), fogColor(shader.uniform("uFogColor")),
          texture(shader.uniform("uTexture")), baseColor(shader.uniform("uBaseColor")),
          hasTexture(shader.uniform("uHasTexture")), alphaCutoff(shader.uniform("uAlphaCutoff")),
          baseAlpha(shader.uniform("uBaseAlpha")), doubleSided(shader.uniform("uDoubleSided")),
          sunDir(shader.uniform("uSunDir")), ambient(shader.uniform("uAmbient")), affine(shader.uniform("uAffine")),
          vertexLighting(shader.uniform("uVertexLighting")), shadowMap(shader.uniform("uShadowMap")),
          shadowDitherStrength(shader.uniform("uShadowDitherStrength")),
          ditheredShadows(shader.uniform("uDitheredShadows")),
          ditheredShadowDarkness(shader.uniform("uDitheredShadowDarkness")) {}
};

struct CubeProgram {
    ShaderProgram shader;
    int mvp;
    int model;
    int reflectionClipPlane;
    int lightViewProj;
    int snap;
    int snapStrength;
    int fogRange;
    int fogColor;
    int color;
    int sunDir;
    int ambient;
    int vertexLighting;
    int shadowMap;
    int shadowDitherStrength;
    int ditheredShadows;
    int ditheredShadowDarkness;
    CubeProgram()
        : shader(shaders::cube_vert, shaders::cube_frag, "cube", litSource, shaders::vertex_snap_glsl),
          mvp(shader.uniform("uMvp")), model(shader.uniform("uModel")),
          reflectionClipPlane(shader.uniform("uReflectionClipPlane")), lightViewProj(shader.uniform("uLightViewProj")),
          snap(shader.uniform("uSnap")), snapStrength(shader.uniform("uSnapStrength")),
          fogRange(shader.uniform("uFogRange")), fogColor(shader.uniform("uFogColor")), color(shader.uniform("uColor")),
          sunDir(shader.uniform("uSunDir")), ambient(shader.uniform("uAmbient")),
          vertexLighting(shader.uniform("uVertexLighting")), shadowMap(shader.uniform("uShadowMap")),
          shadowDitherStrength(shader.uniform("uShadowDitherStrength")),
          ditheredShadows(shader.uniform("uDitheredShadows")),
          ditheredShadowDarkness(shader.uniform("uDitheredShadowDarkness")) {}
};

struct PresentationProgram {
    ShaderProgram shader;
    int scale;
    int texture;
    int lensStrength;
    int lensAspect;
    PresentationProgram(std::string_view fragment, const char* name)
        : shader(shaders::retro_vert, fragment, name, shaders::lens_glsl), scale(shader.uniform("uScale")),
          texture(shader.uniform("uTexture")), lensStrength(shader.uniform("uLensStrength")),
          lensAspect(shader.uniform("uLensAspect")) {}
};

struct RetroProgram : PresentationProgram {
    int ditherStrength;
    RetroProgram()
        : PresentationProgram(shaders::retro_frag, "retro"), ditherStrength(shader.uniform("uDitherStrength")) {}
};

struct ShadowProgram {
    ShaderProgram shader;
    int mvp;
    int texture;
    int textured;
    int opacity;
    int alphaCutoff;
    int baseAlpha;
    int modelMesh;
    int texScale;
    int texOffset;
    ShadowProgram()
        : shader(shaders::shadow_vert, shaders::shadow_frag, "shadow"), mvp(shader.uniform("uMvp")),
          texture(shader.uniform("uTexture")), textured(shader.uniform("uTextured")),
          opacity(shader.uniform("uOpacity")), alphaCutoff(shader.uniform("uAlphaCutoff")),
          baseAlpha(shader.uniform("uBaseAlpha")), modelMesh(shader.uniform("uModelMesh")),
          texScale(shader.uniform("uTexScale")), texOffset(shader.uniform("uTexOffset")) {}
};

struct Programs {
    PlainProgram plain;
    SpriteProgram sprite;
    RippleProgram ripple;
    SkyProgram sky;
    MenuProgram menu;
    RoadProgram road;
    SkyscraperProgram skyscraper;
    MaterialProgram material;
    ModelProgram model;
    CubeProgram cube;
    PresentationProgram present{shaders::present_frag, "presentation"};
    RetroProgram retro;
    ShadowProgram shadow;
};
} // namespace engine::graphics::detail
