#include "GameConfig.h"
#include "TestSupport.h"
#include "engine/graphics/SceneRenderer.h"
#include "game/SceneryLayout.h"
#include "game/WorldScene.h"
#include <algorithm>
#include <cmath>
#include <glad/glad.h>

using namespace engine::graphics;
using namespace engine::math;
namespace {
std::vector<unsigned char> frame(SceneRenderer& renderer, const Scene& scene, Size size) {
    renderer.render(scene, size);
    std::vector<unsigned char> pixels(size.width * size.height * 3);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, size.width, size.height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    GLint state = -1;
    for (GLuint unit = 0; unit <= 13; ++unit) {
        glGetIntegeri_v(GL_SAMPLER_BINDING, unit, &state);
        CHECK(state == 0);
    }
    glGetIntegerv(GL_ACTIVE_TEXTURE, &state);
    CHECK(state == GL_TEXTURE0 && glGetError() == GL_NO_ERROR);
    return pixels;
}
std::vector<ModelInstance> lamps(const Scene& scene, const game::GameAssets& assets) {
    std::vector<ModelInstance> result;
    for (const auto& object : scene.objects) {
        if (const auto* model = std::get_if<ModelInstance>(&object);
            model && model->model == &assets.model(game::ModelId::StreetLamp)) {
            result.push_back(*model);
        }
    }
    return result;
}
void placementChecks(const game::GameAssets& assets, game::WorldScene& world) {
    const auto data = engine::assets::loadGltf(std::filesystem::path(TEST_ASSET_DIR) / "models/street_lamp/scene.gltf");
    const TrackCurve track({GameConfig::kWorldCurvature, GameConfig::kRoadSway, -5, 24, GameConfig::kLaneStartZ});
    CHECK(assets.model(game::ModelId::StreetLamp).primitives()[0].material.emissive.id() != 0);
    bool skippedSlot = false;
    for (float scroll : {0.f, 5.f, 37.f, 99.f, 1217.f}) {
        const Scene day = world.build(0, scroll, 24, {}, {}, 0, 0);
        const Scene dusk = world.build(0, scroll, 24, {}, {}, 0, .5f);
        const Scene evening = world.build(0, scroll, 24, {}, {}, 0, .7f);
        const Scene night = world.build(0, scroll, 24, {}, {}, 0, 1);
        const auto daylightLamps = lamps(day, assets), nightLamps = lamps(night, assets);
        CHECK(day.pointLights.empty() && dusk.pointLights.empty());
        CHECK(nightLamps.size() >= 3 && nightLamps.size() <= 9 && nightLamps.size() <= kMaxPointLights);
        CHECK(nightLamps.size() == daylightLamps.size() && nightLamps.size() == night.pointLights.size());
        CHECK(evening.pointLights.size() == night.pointLights.size());
        bool left = false, right = false;
        for (std::size_t i = 0; i < nightLamps.size(); ++i) {
            const auto& lamp = nightLamps[i];
            const auto& m = lamp.transform.values;
            CHECK(m[5] >= GameConfig::kStreetLampHeight * .96f && m[5] <= GameConfig::kStreetLampHeight * 1.04f);
            CHECK(lamp.pbrSurface && lamp.emissiveScale == 1 && daylightLamps[i].emissiveScale == 0);
            CHECK(m == daylightLamps[i].transform.values); // Switching light does not respawn or move the props.
            CHECK(near(m[13], game::scenery::sidewalkTopY - track.drop(m[14])));
            const float side = m[12] - track.offset(m[14]) < 0 ? -1.f : 1.f;
            left |= side < 0;
            right |= side > 0;
            float nextDistance = 1000;
            for (const auto& other : nightLamps) {
                const auto& n = other.transform.values;
                if (side * (n[12] - track.offset(n[14])) > 0 && n[14] > m[14]) {
                    nextDistance = std::min(nextDistance, n[14] - m[14]);
                }
            }
            CHECK(nextDistance > GameConfig::kStreetLampSpacing - 10);
            skippedSlot |= nextDistance < 1000 && nextDistance > GameConfig::kStreetLampSpacing * 1.5f;
            for (const auto& object : night.objects) {
                if (const auto* door = std::get_if<Quad>(&object);
                    door && door->texture == &assets.texture(game::TextureId::BrickDoor) && -door->normal.x == side) {
                    CHECK(std::abs(m[14] - door->model.values[14]) >= GameConfig::kGardenDoorPathWidth * .5f + .2f);
                }
            }
            for (std::size_t j = 0; j < data.primitives[0].vertices.size(); j += engine::assets::kModelVertexStride) {
                const auto& v = data.primitives[0].vertices;
                if (v[j + 1] > .001f) {
                    continue;
                }
                const auto foot = transformPoint(lamp.transform, {v[j], v[j + 1], v[j + 2]});
                const float x = side * (foot.x - track.offset(foot.z));
                CHECK(x > game::scenery::roadHalfEdge && x < game::scenery::sidewalkHalfWidth);
            } // The model's actual base sits entirely on the sidewalk, clear of garden entrances.
            const auto& light = night.pointLights[i];
            CHECK(side * (light.position.x - m[12]) < 0); // The luminous arm faces inward over the street.
            CHECK(light.position.y > m[13] + GameConfig::kStreetLampHeight * .9f);
            CHECK(light.color.r > light.color.g && light.color.g > light.color.b);
            CHECK(light.intensity > 0 && evening.pointLights[i].intensity > 0);
            CHECK(evening.pointLights[i].intensity < light.intensity);
        }
        CHECK(left && right);
        const Scene moved = world.build(0, scroll + .05f, 24, {}, {}, 0, 1);
        const auto movingLamps = lamps(moved, assets);
        for (const auto& lamp : nightLamps) {
            const auto& a = lamp.transform.values;
            if (a[14] > 23.9f) {
                continue;
            }
            const auto match = std::find_if(movingLamps.begin(), movingLamps.end(), [&](const auto& other) {
                const auto& b = other.transform.values;
                return near(b[14], a[14] + .05f, .0002f) && near(b[0], a[0]) && near(b[5], a[5]);
            });
            CHECK(match != movingLamps.end());
            const auto& b = match->transform.values;
            CHECK(near(a[12] - track.offset(a[14]), b[12] - track.offset(b[14]), .0002f));
        }
        const Scene adjacent = world.build(0, scroll, 24, {}, {}, 0, .701f);
        CHECK(std::abs(adjacent.pointLights[0].intensity - evening.pointLights[0].intensity) < .02f);
    }
    CHECK(skippedSlot); // Occasional gaps break up the rhythm beyond the wider base spacing.
}
Scene darkScene() {
    Scene scene;
    scene.sky.top = scene.sky.bottom = scene.sky.sunColor = {0, 0, 0};
    scene.sky.cloudCoverage = scene.sky.sunSize = 0;
    scene.light.ambient = 1;
    return scene;
}
void illuminationChecks(Size size) {
    RenderSettings settings;
    settings.retroSize = size;
    settings.shadowMapSize = 32;
    settings.worldReflectionMapSize = 0;
    settings.retroDitherStrength = 0;
    SceneRenderer renderer(settings);
    Scene scene = darkScene();
    scene.camera.position = {0, 6, 6};
    scene.camera.target = {0, 0, 0};
    Quad ground;
    ground.model = multiply(rotateX(kPi * .5f), scale({12, 12, 1}));
    ground.worldSize = {12, 12};
    ground.color = {.35f, .35f, .35f};
    ground.pbrSurface = true;
    ground.castsShadow = ground.receivesShadow = false;
    scene.objects = {ground};
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        renderer.setMode(mode);
        scene.pointLights.clear();
        const auto unlit = frame(renderer, scene, size);
        scene.pointLights = {{{0, 2, 0}, {1, .78f, .36f}, 2.2f, 4}};
        const auto lit = frame(renderer, scene, size);
        const int center = (size.height / 2 * size.width + size.width / 2) * 3;
        CHECK(lit[center] > unlit[center] + 30);
        CHECK(lit[center] > lit[center + 1] && lit[center + 1] > lit[center + 2]);
        CHECK(frame(renderer, scene, size) == lit);
        scene.pointLights[0].radius = 2;
        CHECK(frame(renderer, scene, size) == unlit); // The finite light fades completely outside its radius.
        scene.pointLights[0].radius = 4;
        scene.pointLights[0].position.x = 20;
        CHECK(frame(renderer, scene, size) == unlit);
        scene.pointLights[0].position.x = 0;
        scene.pointLights[0].intensity = 0;
        CHECK(frame(renderer, scene, size) == unlit); // No retained uniforms when all lamps switch off.
        scene.pointLights[0].intensity = 2.2f;
        std::get<Quad>(scene.objects[0]).normal = {0, -1, 0};
        CHECK(frame(renderer, scene, size) == unlit); // Facing-away surfaces do not receive the pool of light.
        std::get<Quad>(scene.objects[0]).normal = {0, 1, 0};
    }
}
void bulbChecks(const game::GameAssets& assets, Size size) {
    RenderSettings settings;
    settings.retroSize = size;
    settings.shadowMapSize = 32;
    settings.worldReflectionMapSize = 0;
    SceneRenderer renderer(settings);
    Scene scene = darkScene();
    scene.camera.position = {0, 2.8f, 6};
    scene.camera.target = {0, 2.8f, 0};
    ModelInstance lamp;
    lamp.model = &assets.model(game::ModelId::StreetLamp);
    lamp.transform = scale({5.6f, 5.6f, 5.6f});
    lamp.pbrSurface = true;
    Quad ordinary;
    ordinary.model = scale({4, 4, 1});
    ordinary.pbrSurface = true;
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        renderer.setMode(mode);
        scene.objects = {ordinary};
        const auto baseline = frame(renderer, scene, size);
        lamp.emissiveScale = 0;
        scene.objects = {lamp};
        const auto off = frame(renderer, scene, size);
        std::get<ModelInstance>(scene.objects[0]).emissiveScale = 1;
        const auto on = frame(renderer, scene, size);
        int luminous = 0;
        for (std::size_t i = 0; i < on.size(); i += 3) {
            luminous += on[i] > off[i] + 80 && on[i + 1] > off[i + 1] + 40;
        }
        CHECK(luminous > 20); // The actual authored bulb mask lights up without a point light or sun.
        scene.objects = {ordinary};
        CHECK(frame(renderer, scene, size) == baseline); // Emission must not leak into later surfaces.
    }
}
void reflectedLightingChecks(Size size) {
    constexpr float backingPixel[] = {.02f}, glassPixel[] = {1};
    Texture backing = Texture::fromRedChannel({1, 1}, backingPixel);
    Texture glass = Texture::fromRedChannel({1, 1}, glassPixel);
    Quad window;
    window.model = scale({4, 4, 1});
    window.normal = {0, 0, 1};
    window.worldSize = {4, 4};
    window.texture = &backing;
    window.materialTextures.glass = &glass;
    window.skyscraperWindows = true;
    window.castsShadow = window.receivesShadow = false;
    window.affineStrength = 0;
    window.windowMaterial.roughness = .08f;
    window.windowMaterial.reflectance = .8f;
    Quad surface;
    surface.model = multiply(translation({0, 0, 8}), scale({2, 2, 1}));
    surface.normal = {0, 0, -1};
    surface.worldSize = {2, 2};
    surface.color = {.5f, .5f, .5f};
    surface.pbrSurface = true;
    surface.castsShadow = surface.receivesShadow = false;
    for (bool planar : {false, true}) {
        RenderSettings settings;
        settings.retroSize = size;
        settings.shadowMapSize = 32;
        settings.worldReflectionMapSize = 64;
        settings.planarReflectionSize = planar ? 128 : 0;
        settings.maxPlanarReflections = 1;
        settings.planarReflectionBlendSeconds = 0;
        SceneRenderer renderer(settings);
        Scene scene = darkScene();
        scene.camera.position = {0, 0, 4};
        scene.camera.target = {0, 0, 0};
        scene.objects = {window, surface}; // The illuminated surface is behind the main camera.
        for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
            renderer.setMode(mode);
            scene.pointLights.clear();
            scene.objects = {window};
            const auto emptyGlass = frame(renderer, scene, size);
            scene.objects = {window, surface};
            const auto unlit = frame(renderer, scene, size);
            scene.pointLights = {{{0, 1, 7}, {1, .78f, .36f}, 2.2f, 4}};
            const auto lit = frame(renderer, scene, size);
            const int center = (size.height / 2 * size.width + size.width / 2) * 3;
            CHECK(lit[center] > unlit[center] + 30);
            CHECK(lit[center] > lit[center + 2] + 20);
            scene.objects = {window};
            CHECK(frame(renderer, scene, size) == emptyGlass); // The light itself is out of range of the glass.
            scene.objects = {window, surface};
        }
    }
}
} // namespace
void streetLampChecks(const game::GameAssets& assets, game::WorldScene& world, Size size) {
    placementChecks(assets, world);
    illuminationChecks(size);
    bulbChecks(assets, size);
    reflectedLightingChecks(size);
}
