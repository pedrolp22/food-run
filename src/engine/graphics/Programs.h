#pragma once

#include "ShaderSources.h"
#include "engine/graphics/ShaderProgram.h"
#include <string>

namespace engine::graphics::detail {
inline const std::string litTextureSource =
    std::string(shaders::lighting_glsl) + std::string(shaders::texture_filter_glsl);
struct PlainProgram {
    ShaderProgram shader;
    int mvp;
    int model;
    int lightViewProj;
    int snap;
    int color;
    int alpha;
    int light;
    int ambientLight;
    int receivesShadow;
    int shadowMap;
    PlainProgram()
        : shader(shaders::plain_vert, shaders::plain_frag, "plain", shaders::lighting_glsl, shaders::vertex_snap_glsl),
          mvp(shader.uniform("uMvp")), model(shader.uniform("uModel")), lightViewProj(shader.uniform("uLightViewProj")),
          snap(shader.uniform("uSnap")), color(shader.uniform("uColor")), alpha(shader.uniform("uAlpha")),
          light(shader.uniform("uLight")), ambientLight(shader.uniform("uAmbientLight")),
          receivesShadow(shader.uniform("uReceivesShadow")), shadowMap(shader.uniform("uShadowMap")) {}
};

struct SpriteProgram {
    ShaderProgram shader;
    int mvp;
    int model;
    int extruded;
    int outlinePixels;
    int sunDir;
    int snap;
    int texture;
    int threePoint;
    int textureRepeat;
    int maxTextureLod;
    int time;
    int spawnTime;
    int rippleColor;
    int aspect;
    int affine;
    SpriteProgram()
        : shader(shaders::sprite_vert, shaders::sprite_frag, "sprite", shaders::texture_filter_glsl,
                 shaders::vertex_snap_glsl),
          mvp(shader.uniform("uMvp")), model(shader.uniform("uModel")), extruded(shader.uniform("uExtruded")),
          outlinePixels(shader.uniform("uOutlinePixels")), sunDir(shader.uniform("uSunDir")),
          snap(shader.uniform("uSnap")), texture(shader.uniform("uTexture")), threePoint(shader.uniform("uThreePoint")),
          textureRepeat(shader.uniform("uTextureRepeat")), maxTextureLod(shader.uniform("uMaxTextureLod")),
          time(shader.uniform("uTime")), spawnTime(shader.uniform("uSpawnTime")),
          rippleColor(shader.uniform("uRippleColor")), aspect(shader.uniform("uAspect")),
          affine(shader.uniform("uAffine")) {}
};

struct RippleProgram {
    ShaderProgram shader;
    int mvp;
    int snap;
    int distanceField;
    int color;
    int radius;
    int phase;
    int width;
    int opacity;
    int burst;
    int burstProgress;
    RippleProgram()
        : shader(shaders::ripple_vert, shaders::ripple_frag, "ripple", {}, shaders::vertex_snap_glsl),
          mvp(shader.uniform("uMvp")), snap(shader.uniform("uSnap")), distanceField(shader.uniform("uDistanceField")),
          color(shader.uniform("uColor")), radius(shader.uniform("uRadius")), phase(shader.uniform("uPhase")),
          width(shader.uniform("uWidth")), opacity(shader.uniform("uOpacity")), burst(shader.uniform("uBurst")),
          burstProgress(shader.uniform("uBurstProgress")) {}
};

struct SkyProgram {
    ShaderProgram shader;
    int mvp;
    int topColor;
    int bottomColor;
    SkyProgram()
        : shader(shaders::sky_vert, shaders::sky_frag, "sky"), mvp(shader.uniform("uMvp")),
          topColor(shader.uniform("uTopColor")), bottomColor(shader.uniform("uBottomColor")) {}
};

struct SunProgram {
    ShaderProgram shader;
    int mvp;
    int snap;
    int color;
    int core;
    int haze;
    SunProgram()
        : shader(shaders::sun_vert, shaders::sun_frag, "sun", {}, shaders::vertex_snap_glsl),
          mvp(shader.uniform("uMvp")), snap(shader.uniform("uSnap")), color(shader.uniform("uColor")),
          core(shader.uniform("uCore")), haze(shader.uniform("uHaze")) {}
};

struct RoadProgram {
    ShaderProgram shader;
    int mvp;
    int model;
    int lightViewProj;
    int texScale;
    int texOffset;
    int snap;
    int texture;
    int threePoint;
    int textureRepeat;
    int maxTextureLod;
    int lod;
    int light;
    int ambientLight;
    int alpha;
    int time;
    int heat;
    int affine;
    int receivesShadow;
    int color;
    int shadowMap;
    RoadProgram()
        : shader(shaders::road_vert, shaders::road_frag, "road", litTextureSource, shaders::vertex_snap_glsl),
          mvp(shader.uniform("uMvp")), model(shader.uniform("uModel")), lightViewProj(shader.uniform("uLightViewProj")),
          texScale(shader.uniform("uTexScale")), texOffset(shader.uniform("uTexOffset")), snap(shader.uniform("uSnap")),
          texture(shader.uniform("uTexture")), threePoint(shader.uniform("uThreePoint")),
          textureRepeat(shader.uniform("uTextureRepeat")), maxTextureLod(shader.uniform("uMaxTextureLod")),
          lod(shader.uniform("uLod")), light(shader.uniform("uLight")), ambientLight(shader.uniform("uAmbientLight")),
          alpha(shader.uniform("uAlpha")), time(shader.uniform("uTime")), heat(shader.uniform("uHeat")),
          affine(shader.uniform("uAffine")), receivesShadow(shader.uniform("uReceivesShadow")),
          color(shader.uniform("uColor")), shadowMap(shader.uniform("uShadowMap")) {}
};

struct ModelProgram {
    ShaderProgram shader;
    int mvp;
    int model;
    int lightViewProj;
    int snap;
    int texture;
    int threePoint;
    int textureRepeat;
    int maxTextureLod;
    int baseColor;
    int hasTexture;
    int sunDir;
    int ambient;
    int affine;
    int shadowMap;
    ModelProgram()
        : shader(shaders::model_vert, shaders::model_frag, "model", litTextureSource, shaders::vertex_snap_glsl),
          mvp(shader.uniform("uMvp")), model(shader.uniform("uModel")), lightViewProj(shader.uniform("uLightViewProj")),
          snap(shader.uniform("uSnap")), texture(shader.uniform("uTexture")), threePoint(shader.uniform("uThreePoint")),
          textureRepeat(shader.uniform("uTextureRepeat")), maxTextureLod(shader.uniform("uMaxTextureLod")),
          baseColor(shader.uniform("uBaseColor")), hasTexture(shader.uniform("uHasTexture")),
          sunDir(shader.uniform("uSunDir")), ambient(shader.uniform("uAmbient")), affine(shader.uniform("uAffine")),
          shadowMap(shader.uniform("uShadowMap")) {}
};

struct CubeProgram {
    ShaderProgram shader;
    int mvp;
    int model;
    int lightViewProj;
    int snap;
    int color;
    int sunDir;
    int ambient;
    int shadowMap;
    CubeProgram()
        : shader(shaders::cube_vert, shaders::cube_frag, "cube", shaders::lighting_glsl, shaders::vertex_snap_glsl),
          mvp(shader.uniform("uMvp")), model(shader.uniform("uModel")), lightViewProj(shader.uniform("uLightViewProj")),
          snap(shader.uniform("uSnap")), color(shader.uniform("uColor")), sunDir(shader.uniform("uSunDir")),
          ambient(shader.uniform("uAmbient")), shadowMap(shader.uniform("uShadowMap")) {}
};

struct RetroProgram {
    ShaderProgram shader;
    int scale;
    int texture;
    RetroProgram()
        : shader(shaders::retro_vert, shaders::retro_frag, "retro"), scale(shader.uniform("uScale")),
          texture(shader.uniform("uTexture")) {}
};

struct N64Program {
    ShaderProgram shader;
    int scale;
    int texture;
    N64Program()
        : shader(shaders::retro_vert, shaders::n64_frag, "n64"), scale(shader.uniform("uScale")),
          texture(shader.uniform("uTexture")) {}
};

struct ShadowProgram {
    ShaderProgram shader;
    int mvp;
    int texture;
    int textured;
    int opacity;
    ShadowProgram()
        : shader(shaders::shadow_vert, shaders::shadow_frag, "shadow"), mvp(shader.uniform("uMvp")),
          texture(shader.uniform("uTexture")), textured(shader.uniform("uTextured")),
          opacity(shader.uniform("uOpacity")) {}
};

struct Programs {
    PlainProgram plain;
    SpriteProgram sprite;
    RippleProgram ripple;
    SkyProgram sky;
    SunProgram sun;
    RoadProgram road;
    ModelProgram model;
    CubeProgram cube;
    RetroProgram retro;
    N64Program n64;
    ShadowProgram shadow;
};
} // namespace engine::graphics::detail
