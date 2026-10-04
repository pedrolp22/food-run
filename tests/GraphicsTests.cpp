#include "GameConfig.h"
#include "TestSupport.h"
#include "engine/graphics/Model.h"
#include "engine/graphics/SceneRenderer.h"
#include "engine/graphics/ShaderProgram.h"
#include "engine/platform/Window.h"
#include "game/WorldScene.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <glad/glad.h>
#include <iostream>
#include <limits>
#include <vector>

using namespace engine::graphics;
namespace {
std::vector<unsigned char> capture(Size size) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(size.width) * size.height * 3);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, size.width, size.height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    CHECK(glGetError() == GL_NO_ERROR);
    return pixels;
}
void checkState() {
    GLboolean depthMask = GL_FALSE;
    GLint framebuffer = -1;
    GLint sampler = -1;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);
    for (GLuint unit = 0; unit <= 5; ++unit) {
        glGetIntegeri_v(GL_SAMPLER_BINDING, unit, &sampler);
        CHECK(sampler == 0);
    }
    CHECK(depthMask == GL_TRUE && framebuffer == 0);
    CHECK(glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_BLEND) && !glIsEnabled(GL_POLYGON_OFFSET_FILL));
    CHECK(!glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE) && !glIsEnabled(GL_SAMPLE_COVERAGE));
    CHECK(glGetError() == GL_NO_ERROR);
}
void collectibleChecks(const game::GameAssets& assets, game::WorldScene& world, SceneRenderer& renderer, Size size) {
    using namespace engine::math;
    const auto spriteAt = [&](float time, const game::Entity& entity) {
        const auto& scene = world.build(0, 0, time, {&entity, 1}, {});
        for (const auto& object : scene.objects) {
            if (const auto* sprite = std::get_if<Sprite>(&object)) {
                return *sprite;
            }
        }
        throw std::runtime_error("Missing collectible sprite");
    };
    game::Entity entity{game::EntityType::ScoreCollectible, 1, 0, 0, 0};
    for (auto type : {game::EntityType::ScoreCollectible, game::EntityType::SlowCollectible}) {
        entity.type = type;
        for (int index = 0; index < 3; ++index) {
            entity.textureIndex = index;
            const auto sprite = spriteAt(0, entity);
            CHECK(sprite.texture == &assets.collectible(entity));
            CHECK(sprite.profile == &assets.collectibleMesh(entity) && sprite.profile->count() > 12);
            CHECK(sprite.ripple.shape == &assets.collectibleOutline(entity));
            CHECK(sprite.ripple.shape->distanceField().id() != 0);
            CHECK(type == game::EntityType::ScoreCollectible ? sprite.rippleColor.g > sprite.rippleColor.r
                                                             : sprite.rippleColor.r > sprite.rippleColor.g);
            const auto alpha = sprite.texture->alphaMask();
            CHECK(alpha.size() ==
                  static_cast<std::size_t>(sprite.texture->size().width) * sprite.texture->size().height);
        }
    }
    float previousAngle = 0;
    for (int frame = 0; frame <= 100; ++frame) {
        const float time = GameConfig::kCollectibleSpinPeriod - .05f + frame * .001f;
        const auto sprite = spriteAt(time, entity);
        const auto normal = transformNormal(sprite.model, {1, 0, 0});
        const float angle = std::atan2(-normal.z, normal.x);
        if (frame) {
            CHECK(near(std::remainder(angle - previousAngle, 2 * kPi),
                       2 * kPi * .001f / GameConfig::kCollectibleSpinPeriod));
        }
        previousAngle = angle;
        entity.spawnTime = time - .1f;
        CHECK(spriteAt(time, entity).model.values == sprite.model.values);
    }

    entity.type = game::EntityType::SlowCollectible;
    entity.textureIndex = 1; // Burger, with transparent padding and a nonrectangular silhouette.
    Sprite item = spriteAt(0, entity);
    item.ripple.shape = nullptr; // Test the solid profile independently of its animated outer ripple.
    Scene scene;
    scene.camera.position = {0, 0, 4};
    scene.camera.target = {0, 0, 0};
    scene.sky.top = scene.sky.bottom = {0.2f, 0.3f, 0.4f};
    scene.sky.cloudCoverage = 0; // Keep the comparison background uniform.
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        renderer.setMode(mode);
        scene.objects.clear();
        renderer.render(scene, size);
        const auto background = capture(size);
        for (float angle : {0.0f, kPi * .25f, kPi * .5f, kPi, kPi * 1.5f}) {
            item.model = multiply(rotateY(angle),
                                  scale({1.5f, 1.5f / item.texture->aspect(), 1.5f * GameConfig::kCollectibleDepth}));
            scene.objects = {item};
            renderer.render(scene, size);
            const auto pixels = capture(size);
            std::size_t visible = 0;
            for (std::size_t i = 0; i < pixels.size(); i += 3) {
                if (std::abs(int(pixels[i]) - background[i]) + std::abs(int(pixels[i + 1]) - background[i + 1]) +
                        std::abs(int(pixels[i + 2]) - background[i + 2]) >
                    40) {
                    ++visible;
                }
            }
            CHECK(visible > 250); // Real side walls must remain visible at both edge-on angles.
            checkState();
            if (angle == 0) {
                scene.time += 1;
                renderer.render(scene, size);
                const auto laterPixels = capture(size);
                CHECK(laterPixels == pixels); // Steady solid shading includes partially covered edge pixels.
            }
        }
    }
    item.model = scale({1.5f, 1.5f / item.texture->aspect(), 1.5f * GameConfig::kCollectibleDepth});
    item.spawnTime = 0;
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        renderer.setMode(mode);
        scene.time = .3f;
        scene.objects.clear();
        renderer.render(scene, size);
        const auto background = capture(size);
        item.ripple.shape = nullptr;
        scene.objects = {item};
        renderer.render(scene, size);
        const auto body = capture(size);
        item.ripple.shape = &assets.collectibleOutline(entity);
        item.rippleColor = {.04f, .85f, .3f};
        scene.objects = {item};
        renderer.render(scene, size);
        const auto green = capture(size);
        std::size_t outside = 0;
        for (std::size_t i = 0; i < green.size(); i += 3) {
            if (body[i] == background[i] && body[i + 1] == background[i + 1] && body[i + 2] == background[i + 2] &&
                green[i + 1] > body[i + 1] + 10 && green[i + 1] - body[i + 1] > 2 * (green[i] - body[i])) {
                ++outside;
            }
        }
        CHECK(outside > 250); // Color extends outside the silhouette, including beyond the original image bounds.
        const auto center = (size.height / 2 * size.width + size.width / 2) * 3;
        CHECK(green[center] == body[center] && green[center + 1] == body[center + 1]);
        scene.time += .3f;
        renderer.render(scene, size);
        CHECK(capture(size) != green);
        // A foreground object must occlude both the collectible and all of its ripple.
        Quad blocker;
        blocker.model = multiply(translation({0, 0, 1}), scale({4, 4, 1}));
        blocker.normal = {0, 0, 1};
        blocker.receivesShadow = blocker.castsShadow = false;
        scene.objects = {blocker};
        renderer.render(scene, size);
        const auto hidden = capture(size);
        scene.objects.emplace_back(item);
        renderer.render(scene, size);
        CHECK(capture(size) == hidden);
        checkState();
    }
    renderer.setMode(RenderMode::Normal);
}
void collectionBurstChecks(game::WorldScene& world, SceneRenderer& renderer, Size size) {
    SpriteBurst preview;
    for (auto type : {game::EntityType::ScoreCollectible, game::EntityType::SlowCollectible}) {
        for (int index = 0; index < 3; ++index) {
            const game::Entity item{type, 1, 2.1f, .7f, index};
            constexpr float pickupTime = 4.99f;
            const auto before = world.build(0, 0, pickupTime, {&item, 1}, {});
            const auto sprite = std::get<Sprite>(before.objects.back());
            game::CollectEffect event{type, item.lane, item.z, pickupTime, index, item.spawnTime};
            const auto& immediate = world.build(0, 0, pickupTime, {}, {&event, 1});
            CHECK(immediate.effects.size() == 1);
            const auto burst = std::get<SpriteBurst>(immediate.effects.front());
            CHECK(burst.model.values == sprite.model.values); // Same position, height, bob, aspect and rotation.
            CHECK(burst.shape == sprite.ripple.shape && burst.progress == 0);
            CHECK(burst.color.r == sprite.rippleColor.r && burst.color.g == sprite.rippleColor.g);
            CHECK(near(burst.initialPhase,
                       std::fmod(pickupTime - item.spawnTime, sprite.ripple.period) / sprite.ripple.period));
            const auto& middle =
                world.build(0, 0, pickupTime + GameConfig::kCollectBurstDuration * .5f, {}, {&event, 1});
            CHECK(near(std::get<SpriteBurst>(middle.effects.front()).progress, .5f));
            CHECK(std::get<SpriteBurst>(middle.effects.front()).model.values == burst.model.values);
            // Frozen world time after game over still interpolates feedback between simulation steps.
            event.age = GameConfig::kCollectBurstDuration * .5f;
            const auto& frozen = world.build(0, 0, pickupTime, {}, {&event, 1}, .004f);
            CHECK(near(std::get<SpriteBurst>(frozen.effects.front()).progress,
                       .5f + .004f / GameConfig::kCollectBurstDuration));
            CHECK(std::get<SpriteBurst>(frozen.effects.front()).model.values == burst.model.values);
            event.age = GameConfig::kCollectBurstDuration;
            CHECK(world.build(0, 0, pickupTime, {}, {&event, 1}).effects.empty());
            event.age = 0;
            CHECK(world.build(0, 0, pickupTime - .01f, {}, {&event, 1}).effects.empty());
            preview = burst;
        }
    }
    Scene scene;
    scene.camera.position = {0, 0, 4};
    scene.camera.target = {0, 0, 0};
    scene.sky.top = scene.sky.bottom = {.03f, .03f, .03f};
    preview.model = engine::math::multiply(engine::math::rotateY(.4f), engine::math::scale({1.5f, 1.5f, .075f}));
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        renderer.setMode(mode);
        scene.effects.clear();
        renderer.render(scene, size);
        const auto background = capture(size);
        double previousEnergy = 1e30;
        for (float progress : {0.0f, .3f, .7f, 1.0f}) {
            preview.progress = progress;
            scene.effects = {preview};
            renderer.render(scene, size);
            const auto pixels = capture(size);
            double energy = 0;
            for (std::size_t i = 0; i < pixels.size(); ++i) {
                energy += std::max(int(pixels[i]) - background[i], 0);
            }
            CHECK(energy < previousEnergy); // Immediate peak, followed by a short fade.
            if (progress == 0) {
                CHECK(energy > 10000);
            }
            if (progress == 1) {
                CHECK(pixels == background);
            }
            previousEnergy = energy;
            checkState();
        }
    }
    renderer.setMode(RenderMode::Normal);
}
void buildingShaderChecks(const game::GameAssets& assets, game::WorldScene& world, float scroll) {
    const auto& scene = world.build(0, scroll, 2, {}, {});
    const auto& skyscraper = assets.texture(game::TextureId::Skyscraper);
    const auto& roof = assets.texture(game::TextureId::BuildingTop);
    int walls = 0;
    int roofs = 0;
    std::vector<const Quad*> roofQuads;
    for (const auto& object : scene.objects) {
        if (const auto* quad = std::get_if<Quad>(&object)) {
            if (quad->texture == &skyscraper) {
                CHECK(quad->skyscraperWindows);
                CHECK(quad->buildingSurface && quad->opacity == 1);
                CHECK(quad->haze >= 0 && quad->haze <= 1);
                CHECK(!skyscraper.repeats());
                CHECK(quad->textureScale.x == 1 && quad->textureScale.y == 1);
                CHECK(quad->textureOffset.x == 0 && quad->textureOffset.y == 0);
                const auto maps = assets.materialTextures(game::TextureId::Skyscraper);
                CHECK(quad->materialTextures.properties == maps.properties && maps.properties);
                CHECK(quad->materialTextures.normal == maps.normal && maps.normal);
                CHECK(quad->materialTextures.glass == maps.glass && maps.glass);
                CHECK(quad->surfaceMaterial.roughness == 1 && quad->surfaceMaterial.metallic == 1);
                ++walls;
            } else if (quad->texture == &roof) {
                CHECK(quad->normal.y > 0.5f); // Textured top face, not a flat-color roof.
                CHECK(quad->buildingSurface && !quad->skyscraperWindows && quad->opacity == 1);
                roofQuads.push_back(quad);
                ++roofs;
            }
        }
    }
    CHECK(walls > 0 && roofs > 0);
    CHECK(walls % 4 == 0); // All four vertical sides use the complete facade.
    for (std::size_t i = 0; i < roofQuads.size(); ++i) {
        for (std::size_t j = i + 1; j < roofQuads.size(); ++j) {
            const auto& a = *roofQuads[i];
            const auto& b = *roofQuads[j];
            const float xDistance = std::abs(a.model.values[12] - b.model.values[12]);
            const float zDistance = std::abs(a.model.values[14] - b.model.values[14]);
            CHECK(xDistance >= (a.worldSize.x + b.worldSize.x) * .5f ||
                  zDistance >= (a.worldSize.y + b.worldSize.y) * .5f);
        }
    }
}

void skyCloudChecks(Size size) {
    using namespace engine::math;
    RenderSettings settings;
    settings.retroSize = size; // Compare projections without letterboxing.
    SceneRenderer renderer(settings);
    Scene base;
    base.camera.position = {0, 8, 0};
    base.camera.target = {0, 8.25f, -1};
    base.sky.sunSize = base.sky.cloudCoverage = 0;
    Scene scene = base;
    auto frame = [&] {
        renderer.render(scene, size);
        checkState();
        return capture(size);
    };
    auto difference = [](const auto& a, const auto& b) {
        double total = 0;
        for (std::size_t i = 0; i < a.size(); ++i) {
            total += std::abs(int(a[i]) - b[i]);
        }
        return total / a.size();
    };
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        renderer.setMode(mode);
        scene = base;
        const auto clear = frame();
        for (std::size_t i = 2; i < clear.size(); i += 3) {
            CHECK(clear[i] > 215); // The blue sky continues to the horizon, without a gray silhouette.
        }
        scene.camera.position = {80, 24, -60};
        scene.camera.target = {80, 24.25f, -61};
        CHECK(frame() == clear); // The distant dome does not follow camera translation.
        scene = base;
        scene.camera.target = {1, 8.25f, 0};
        CHECK(difference(frame(), clear) < .1); // Clear-sky haze has no fabricated horizon geometry.
        scene.camera.target = base.camera.target;
        CHECK(frame() == clear); // A complete turn returns to the same sky.

        // Camera roll rotates the sky gradient with geometry.
        scene.camera.up = {0, -1, 0};
        const auto rolled = frame();
        double rollError = 0;
        for (std::size_t pixel = 0; pixel < clear.size() / 3; ++pixel) {
            for (int channel = 0; channel < 3; ++channel) {
                rollError += std::abs(int(rolled[pixel * 3 + channel]) -
                                      clear[clear.size() - (pixel + 1) * 3 + channel]);
            }
        }
        CHECK(rollError / clear.size() < 1.5);
        scene = base;
        scene.camera.target = {0, 9, -0.125f};
        const auto overhead = frame();
        CHECK(difference(overhead, clear) > 10); // Pitch moves the horizon out of view.
        for (std::size_t i = 2; i < overhead.size(); i += 3) {
            CHECK(overhead[i] > 200); // No black pole at high elevation.
        }

        scene = base;
        scene.sky.cloudCoverage = Sky{}.cloudCoverage;
        const auto day = frame();
        scene.camera.target = {1, 8.25f, 0};
        CHECK(frame() != day); // Clouds retain their world positions as the camera turns.
        scene.camera.target = base.camera.target;
        CHECK(frame() == day);
        scene.time = .05f;
        CHECK(frame() == day); // Wind advances at four ticks per second, without noise reseeding.
        scene.time = 8;
        const auto drift = frame();
        CHECK(drift != day);
        // This view's bottom rays are below the world horizon. Wind must not
        // turn the clear horizon into an animated fog strip.
        const auto horizonEnd = static_cast<std::size_t>(size.height * .15f) * size.width * 3;
        CHECK(std::equal(day.begin(), day.begin() + horizonEnd, drift.begin()));
        const auto visibleCloudPixels = [&](const auto& pixels) {
            std::size_t count = 0;
            for (std::size_t i = 0; i < pixels.size(); i += 3) {
                if (std::abs(int(pixels[i]) - clear[i]) + std::abs(int(pixels[i + 1]) - clear[i + 1]) +
                        std::abs(int(pixels[i + 2]) - clear[i + 2]) > 8) {
                    ++count;
                }
            }
            return count;
        };
        const auto cloudPixels = visibleCloudPixels(day);
        CHECK(cloudPixels > day.size() / 30 && cloudPixels < day.size() * .15); // Default has open blue gaps.
        scene.time = 0;
        scene.sky.cloudCoverage = 1;
        const auto overcast = frame();
        CHECK(overcast != day);
        CHECK(visibleCloudPixels(overcast) > cloudPixels * 1.5); // Coverage fills clouds rather than a fog overlay.
        scene.sky.cloudCoverage = Sky{}.cloudCoverage;
        scene.light.direction = {1, 2, -1};
        CHECK(frame() != day); // Density-derived shading responds to the sun.
        scene.light.direction = base.light.direction;
        scene.camera.position.x = scene.camera.target.x = 20;
        const auto parallax = frame();
        CHECK(parallax != day); // Finite cloud layers have subtle positional depth.
        CHECK(std::equal(day.begin(), day.begin() + horizonEnd, parallax.begin()));

        scene = base;
        scene.sky.cloudCoverage = Sky{}.cloudCoverage;
        scene.camera.target = {-.001f, 8.25f, 1};
        const auto seamLeft = frame();
        scene.camera.target.x = .001f;
        CHECK(difference(frame(), seamLeft) < 3); // No longitude seam through 180 degrees.
        scene.camera.target = {0, 9, 0};
        scene.camera.up = {0, 0, -1};
        const auto zenith = frame();
        CHECK(zenith != day);
        scene.sky.nightBlend = 1;
        CHECK(frame() != zenith);

        // Locate the sun's white core and compare it to the same perspective
        // projection used by world objects; zoom changes angular disc size.
        scene = base;
        scene.camera.target = {0, 8, -1};
        scene.light.direction = {.14f, .25f, -1};
        scene.sky.sunSize = 14;
        auto discArea = [&](const auto& pixels, bool moon = false) {
            double xSum = 0, ySum = 0;
            int count = 0;
            for (int y = 0; y < size.height; ++y) {
                for (int x = 0; x < size.width; ++x) {
                    const auto index = (y * size.width + x) * 3;
                    const bool core = moon ? pixels[index] > 180 && pixels[index] < 220 &&
                                                 pixels[index + 1] > 165 && pixels[index + 2] > 130 &&
                                                 pixels[index + 2] < 165
                                           : pixels[index] > 240 && pixels[index + 1] > 240 && pixels[index + 2] > 240;
                    if (core) {
                        xSum += x + .5;
                        ySum += y + .5;
                        ++count;
                    }
                }
            }
            CHECK(count > 50);
            const auto dir = normalize(scene.light.direction);
            const Vec3 point = {dir.x * 100, scene.camera.position.y + dir.y * 100, dir.z * 100};
            const auto projection = scene.camera.viewProjection(float(size.width) / size.height);
            const auto clip = transformPoint(projection, point);
            const auto& m = projection.values;
            const float w = m[3] * point.x + m[7] * point.y + m[11] * point.z + m[15];
            CHECK(std::abs(xSum / count - (clip.x / w * .5f + .5f) * size.width) < 4);
            CHECK(std::abs(ySum / count - (clip.y / w * .5f + .5f) * size.height) < 4);
            return count;
        };
        const auto wideSun = discArea(frame());
        scene.camera.verticalFov = 40;
        const auto zoomedSun = discArea(frame());
        CHECK(zoomedSun > wideSun * 2 && zoomedSun < wideSun * 3);
        scene.sky.sunSize = 0;
        const auto noSun = frame();
        scene.sky.nightBlend = 1;
        const auto night = frame();
        CHECK(night != noSun);
        CHECK(discArea(night, true) > zoomedSun * 2); // The stylized moon stays at the light's bearing.
        scene.camera.position = {80, 24, -60};
        scene.camera.target = {80, 24, -61};
        CHECK(frame() == night);

        scene = base;
        scene.camera.position = {0, 0, 4};
        scene.camera.target = {0, 0, 0};
        scene.objects = {Cube{identity(), {.95f, .05f, .05f}}};
        const auto occlusion = frame();
        const auto center = (size.height / 2 * size.width + size.width / 2) * 3;
        CHECK(occlusion[center] > occlusion[center + 1] + 40); // Sky never writes scene depth.
    }
}

void buildingMaterialChecks(const game::GameAssets& assets, Size size) {
    using namespace engine::math;
    RenderSettings settings;
    settings.retroSize = size;
    settings.retroPixelStep = 0; // Isolate the material from vertex movement and letterboxing.
    SceneRenderer renderer(settings);
    Scene scene;
    scene.camera.position = {0, 0, 4};
    scene.camera.target = {0, 0, 0};
    scene.sky.sunSize = scene.sky.cloudCoverage = 0;
    Quad facade;
    facade.model = scale({4, 5, 1}); // Cover the viewport, excluding background animation.
    facade.worldSize = {4, 5};
    facade.normal = {0, 0, 1};
    facade.texture = &assets.texture(game::TextureId::Skyscraper);
    facade.skyscraperWindows = true;
    facade.castsShadow = false;
    facade.affineStrength = 0;
    // A source-texel center safely inside the second column's upper blue pane.
    const Vec3 panePoint = {(25.5f / 64 - .5f) * 4, (81.5f / 128 - .5f) * 5, 0};
    const Vec3 framePoint = {(32.5f / 64 - .5f) * 4, panePoint.y, 0};
    const Vec3 alignedSun = {panePoint.x, panePoint.y, 4};
    auto frame = [&](float time = 0) {
        scene.time = time;
        renderer.render(scene, size);
        checkState();
        return capture(size);
    };
    auto sample = [&](const auto& pixels, Vec3 point) {
        const auto projection = scene.camera.viewProjection(float(size.width) / size.height);
        const auto clip = transformPoint(projection, point);
        const auto& m = projection.values;
        const float w = m[3] * point.x + m[7] * point.y + m[11] * point.z + m[15];
        const int x = int((clip.x / w * .5f + .5f) * size.width);
        const int y = int((clip.y / w * .5f + .5f) * size.height);
        CHECK(x > 0 && x < size.width - 1 && y > 0 && y < size.height - 1);
        std::array<int, 3> color{};
        // Average a small native-pixel patch to tolerate RGB555 dither coverage.
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                for (int channel = 0; channel < 3; ++channel) {
                    color[channel] += pixels[((y + dy) * size.width + x + dx) * 3 + channel];
                }
            }
        }
        for (int& channel : color) {
            channel /= 9;
        }
        return color;
    };
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        renderer.setMode(mode);
        scene.camera.position = {0, 0, 4};
        scene.light = DirectionalLight{};
        scene.light.direction = alignedSun;
        scene.sky = Sky{};
        scene.sky.sunSize = scene.sky.cloudCoverage = 0;
        scene.objects = {facade};
        const auto polished = frame();
        CHECK(frame(.05f) == polished && frame(19) == polished); // No autonomous stripes or material noise.
        const auto polishedColor = sample(polished, panePoint);
        std::get<Quad>(scene.objects.front()).windowMaterial.reflectivity = 0;
        const auto pigment = frame();
        const auto pigmentColor = sample(pigment, panePoint);
        CHECK(polishedColor[0] > pigmentColor[0] + 80); // White dielectric specular over blue pigment.
        CHECK(polishedColor[1] > pigmentColor[1] + 60);
        CHECK(sample(polished, framePoint) == sample(pigment, framePoint)); // Glass controls leave masonry alone.

        scene.objects = {facade};
        auto& metalFrame = std::get<Quad>(scene.objects.front()).surfaceMaterial;
        metalFrame.roughness = .2f;
        metalFrame.metallic = 1;
        const auto metalFramed = frame();
        CHECK(sample(metalFramed, panePoint) == polishedColor); // Frame materials leave glass alone.
        CHECK(sample(metalFramed, framePoint)[0] > sample(polished, framePoint)[0] + 20);

        scene.objects = {facade};
        std::get<Quad>(scene.objects.front()).normal = {0, 0, 4};
        CHECK(frame() == polished); // CPU light and shader agree on normalized normals.
        scene.objects = {facade};
        std::get<Quad>(scene.objects.front()).windowMaterial.roughness = .12f;
        const auto smooth = frame();
        std::get<Quad>(scene.objects.front()).windowMaterial.roughness = .48f;
        const auto rough = frame();
        CHECK(sample(smooth, panePoint)[0] > sample(rough, panePoint)[0] + 40);
        auto reflectionArea = [&](const auto& pixels) {
            std::size_t area = 0;
            for (std::size_t i = 0; i < pixels.size(); i += 3) {
                area += pigment[i + 2] > pigment[i] + 40 && pixels[i] > pigment[i] + 20;
            }
            return area;
        };
        CHECK(reflectionArea(rough) > reflectionArea(smooth) + 100); // Roughness broadens and softens the lobe.

        scene.objects = {facade};
        scene.light.direction = {-alignedSun.x, alignedSun.y, alignedSun.z};
        CHECK(sample(frame(), panePoint)[0] < polishedColor[0] - 40); // The highlight follows the sun angle.
        scene.light.direction = alignedSun;
        scene.camera.position = {1.5f, 0, 4};
        CHECK(sample(frame(), panePoint)[0] < polishedColor[0] - 40); // And the viewer, rather than a UV phase.
        std::get<Quad>(scene.objects.front()).windowMaterial.reflectivity = 0;
        CHECK(std::abs(sample(frame(), panePoint)[0] - pigmentColor[0]) <= 8);
        scene.camera.position = {0, 0, 4};

        // A caster beyond the camera shadows the facade without obscuring it.
        // Shadowing removes the sun lobe but leaves the indirect environment.
        Quad blocker;
        blocker.model = multiply(translation({0, 6, 6}), scale({30, 30, 1}));
        scene.objects = {facade, blocker};
        const auto shadowed = frame();
        const auto shadowColor = sample(shadowed, panePoint);
        CHECK(shadowColor[0] < polishedColor[0] - 80 && shadowColor[2] > 30);
        CHECK(frame(19) == shadowed);
        std::get<Quad>(scene.objects.front()).receivesShadow = false;
        CHECK(frame() == polished); // Opting out of shadows retains the light's actual angle.
        scene.light.direction = {0, 1, -1};
        scene.objects = {facade};
        const auto away = frame();
        CHECK(sample(away, panePoint)[0] < polishedColor[0] - 80);
        std::get<Quad>(scene.objects.front()).receivesShadow = false;
        CHECK(frame() == away); // No unshadowed-face fullbright shortcut.

        // An ambient-only white environment isolates Fresnel from sunlight.
        scene.objects = {facade};
        scene.light.ambient = 1.1f;
        scene.sky.top = scene.sky.bottom = {1, 1, 1};
        const auto front = frame();
        const auto frontColor = sample(front, panePoint);
        scene.light.direction = alignedSun;
        CHECK(frame() == front); // Ambient-only materials cannot emit a direct sun glint.
        scene.sky.nightBlend = 1;
        CHECK(sample(frame(), panePoint)[2] < frontColor[2] - 40); // The environment follows the night sky.
        scene.sky.nightBlend = 0;
        scene.camera.position = {std::sin(radians(75)) * 4, 0, std::cos(radians(75)) * 4};
        const auto grazing = frame();
        CHECK(sample(grazing, panePoint)[0] > frontColor[0] + 40);
        std::get<Quad>(scene.objects.front()).windowMaterial.reflectivity = 0;
        const auto unreflectiveGrazing = sample(frame(), panePoint);
        scene.camera.position = {0, 0, 4};
        CHECK(std::abs(sample(frame(), panePoint)[0] - unreflectiveGrazing[0]) <= 8);

        scene.light = DirectionalLight{};
        scene.light.direction = alignedSun;
        scene.sky = Sky{};
        scene.sky.sunSize = scene.sky.cloudCoverage = 0;
        scene.objects = {facade};
        std::get<Quad>(scene.objects.front()).haze = .6f;
        const auto fogged = frame();
        Quad behind;
        behind.model = multiply(translation({0, 0, -1}), scale({12, 12, 1}));
        behind.color = {1, 0, 0};
        behind.receivesShadow = behind.castsShadow = false;
        scene.objects.emplace_back(behind);
        CHECK(frame() == fogged); // Atmospheric fade preserves opaque depth occlusion.

        // A red material makes dielectric white specular and colored metal
        // specular distinguishable without relying on facade mask boundaries.
        constexpr float red[] = {.5f};
        Texture surface = Texture::fromRedChannel({1, 1}, red);
        Quad wall = facade;
        wall.texture = &surface;
        wall.skyscraperWindows = false;
        wall.buildingSurface = true;
        wall.surfaceMaterial.roughness = .24f;
        scene.objects = {wall};
        const auto dielectric = frame();
        const auto dielectricColor = sample(dielectric, panePoint);
        std::get<Quad>(scene.objects.front()).surfaceMaterial.metallic = 1;
        const auto metalColor = sample(frame(), panePoint);
        CHECK(dielectricColor[1] > metalColor[1] + 80 && dielectricColor[2] > metalColor[2] + 80);
        scene.objects = {wall};
        scene.sky.sunColor = {1, .1f, .1f};
        const auto warm = sample(frame(), panePoint);
        CHECK(warm[0] > warm[1] + 80 && warm[1] < dielectricColor[1] - 80); // Light colors the specular lobe.
        scene.sky.sunColor = {1, 1, 1};
        scene.objects = {wall};
        auto& repeated = std::get<Quad>(scene.objects.front());
        repeated.textureScale = {3, 6};
        repeated.textureOffset = {2, .7f};
        CHECK(frame() == dielectric); // Neither material nor foundation shade resets with texture tiles.

        scene.objects = {facade};
        auto& material = std::get<Quad>(scene.objects.front()).windowMaterial;
        material.roughness = 0;
        const auto minimumRoughness = frame();
        material.roughness = -.5f;
        CHECK(frame() == minimumRoughness);
        material.roughness = 1;
        const auto maximumRoughness = frame();
        material.roughness = 2;
        CHECK(frame() == maximumRoughness);
        material.roughness = WindowMaterial{}.roughness;
        material.reflectivity = 2;
        CHECK(frame() == polished);
        material.reflectivity = -.5f;
        CHECK(frame() == pigment);
    }
}

void streetMaterialChecks(const game::GameAssets& assets, game::WorldScene& world, Size size) {
    using namespace engine::math;
    const std::vector<game::Entity> obstacles = {{game::EntityType::Obstacle, 0, -4, 0, 0},
                                               {game::EntityType::Obstacle, 2, -8, 0, 1}};
    for (float scroll : {0.0f, 37.0f, 99.0f}) {
        const auto& scenery = world.build(0, scroll, 2, obstacles, {});
        int roads = 0, sidewalks = 0, props = 0;
        for (const auto& object : scenery.objects) {
            if (const auto* quad = std::get_if<Quad>(&object)) {
                if (quad->texture == &assets.texture(game::TextureId::Road)) {
                    CHECK(quad->pbrSurface && quad->surfaceMaterial.roughness >= .85f);
                    CHECK(quad->surfaceMaterial.metallic == 0 && quad->receivesShadow);
                    CHECK(!quad->psxMinificationLodBias);
                    ++roads;
                } else if (quad->texture == &assets.texture(game::TextureId::Sidewalk)) {
                    CHECK(quad->pbrSurface && quad->surfaceMaterial.roughness >= .9f);
                    CHECK(quad->surfaceMaterial.metallic == 0 && quad->receivesShadow);
                    CHECK(quad->psxMinificationLodBias == GameConfig::kPsxSidewalkLodBias);
                    ++sidewalks;
                }
            } else if (const auto* cube = std::get_if<Cube>(&object)) {
                CHECK(cube->pbrSurface); // Curbs and the player use the shared finish, too.
            } else if (const auto* model = std::get_if<ModelInstance>(&object)) {
                CHECK(model->pbrSurface && !model->materialOverride);
                ++props;
            }
        }
        CHECK(roads > 0 && sidewalks > 0 && props >= 2);
    }
    const auto& bench = assets.model(game::ModelId::Bench).primitives();
    CHECK(bench[0].material.surfaceMaterial.metallic == 0);
    CHECK(bench[1].material.surfaceMaterial.metallic == 1);
    CHECK(near(bench[1].material.surfaceMaterial.roughness, .2f));

    RenderSettings settings;
    settings.retroSize = size;
    settings.retroPixelStep = 0;
    SceneRenderer renderer(settings);
    Scene scene;
    scene.camera.position = {0, 3, 4};
    scene.camera.target = {0, 0, 0};
    scene.light.direction = {0, 3, -4}; // Mirror alignment for the horizontal receiver.
    scene.sky.sunSize = scene.sky.cloudCoverage = 0;
    constexpr float pigment[] = {.5f};
    Texture asphalt = Texture::fromRedChannel({1, 1}, pigment);
    Quad road;
    road.model = multiply(rotateX(-kPi * .5f), scale({4, 5, 1}));
    road.normal = {0, 1, 0};
    road.worldSize = {4, 5};
    road.texture = &asphalt;
    road.castsShadow = false;
    road.affineStrength = 0;
    road.pbrSurface = true;
    road.surfaceMaterial.roughness = .24f;
    auto frame = [&](float time = 0) {
        scene.time = time;
        renderer.render(scene, size);
        checkState();
        return capture(size);
    };
    auto center = [&](const auto& pixels) {
        std::array<int, 3> color{};
        for (int y = size.height / 2 - 1; y <= size.height / 2 + 1; ++y) {
            for (int x = size.width / 2 - 1; x <= size.width / 2 + 1; ++x) {
                for (int channel = 0; channel < 3; ++channel) {
                    color[channel] += pixels[(y * size.width + x) * 3 + channel];
                }
            }
        }
        for (int& channel : color) {
            channel /= 9;
        }
        return color;
    };

    // A model quad isolates linear glTF factors and per-instance finish overrides.
    engine::assets::ModelData data;
    engine::assets::PrimitiveData primitive;
    primitive.vertices = {-2, -2.5f, 0, 0, 0, 1, 0, 0, 2, -2.5f, 0, 0, 0, 1, 1, 0,
                           2, 2.5f, 0, 0, 0, 1, 1, 1, -2, 2.5f, 0, 0, 0, 1, 0, 1};
    primitive.indices = {0, 1, 2, 0, 2, 3};
    primitive.material.baseColor = {std::pow((.5f + .055f) / 1.055f, 2.4f), 0, 0, 1};
    primitive.material.metallic = 0;
    primitive.material.roughness = .24f;
    data.primitives.push_back(primitive);
    Model model(data);
    ModelInstance prop{identity(), &model};
    prop.pbrSurface = true;

    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        renderer.setMode(mode);
        scene.camera.position = {0, 3, 4};
        scene.light.direction = {0, 3, -4};
        scene.objects = {road};
        const auto lit = frame();
        const auto litColor = center(lit);
        CHECK(frame(19) == lit); // Stable material response without time-dependent sheen.
        std::get<Quad>(scene.objects.front()).surfaceMaterial.roughness = .94f;
        CHECK(center(frame())[0] < litColor[0] - 60); // Concrete/asphalt spread and soften direct specular.
        scene.objects = {road};
        std::get<Quad>(scene.objects.front()).surfaceMaterial.reflectivity = 0;
        const auto matteColor = center(frame());
        CHECK(matteColor[1] == 0 && matteColor[2] == 0); // No reflected white/sky light over red pigment.
        CHECK(litColor[1] > matteColor[1] + 80);
        scene.objects = {road};
        scene.light.direction = {0, 3, 4};
        CHECK(center(frame())[0] < litColor[0] - 60); // A fixed surface does not retain a painted glint.
        scene.light.direction = {0, 3, -4};
        Quad caster;
        caster.model = multiply(translation({0, 3, -4}), multiply(rotateX(-kPi * .5f), scale({30, 30, 1})));
        scene.objects = {road, caster};
        const auto shadow = center(frame());
        CHECK(shadow[0] < litColor[0] - 60 && shadow[2] > 5); // Sun reflection is shadowed; sky remains.
        std::get<Quad>(scene.objects.front()).receivesShadow = false;
        CHECK(center(frame()) == litColor);

        scene.camera.position = {0, 0, 4};
        scene.light.direction = {0, 0, 1};
        Quad plaque;
        plaque.model = scale({4, 5, 1});
        plaque.normal = {0, 0, 1};
        plaque.color = {.5f, 0, 0};
        plaque.castsShadow = false;
        plaque.pbrSurface = true;
        plaque.surfaceMaterial.reflectivity = 0;
        scene.objects = {plaque};
        const auto redColor = center(frame());
        prop.materialOverride = plaque.surfaceMaterial;
        scene.objects = {prop};
        const auto modelColor = center(frame());
        CHECK(modelColor[0] > 60 && std::abs(modelColor[0] - redColor[0]) <= 8);
        CHECK(modelColor[1] == 0 && modelColor[2] == 0); // Linear factors must not be gamma-decoded twice.
        prop.materialOverride.reset();
        scene.objects = {prop};
        CHECK(center(frame())[1] > 120); // Imported dielectric finish restores the sun lobe.
        prop.materialOverride = SurfaceMaterial{.24f, 1, 1};
        scene.objects = {prop};
        CHECK(center(frame())[1] < 60); // Colored metallic reflection uses the same shader as buildings.
        CHECK(model.primitives()[0].material.surfaceMaterial.metallic == 0); // Overrides do not mutate the asset.
        prop.materialOverride.reset();
        Cube cube{scale({4, 5, .5f}), plaque.color};
        cube.pbrSurface = true;
        cube.surfaceMaterial = plaque.surfaceMaterial;
        scene.objects = {cube};
        CHECK(std::abs(center(frame())[0] - redColor[0]) <= 8);

        // A textured, faded surface must not leave UV/alpha/heat/sampler state on props.
        Quad textured = plaque;
        textured.texture = &assets.texture(game::TextureId::Sidewalk);
        textured.textureScale = {3, 6};
        textured.textureOffset = {.2f, .7f};
        textured.psxMinificationLodBias = GameConfig::kPsxSidewalkLodBias;
        textured.opacity = .3f;
        textured.haze = .7f;
        textured.heat = 1;
        scene.objects = {prop};
        const auto cleanProp = frame();
        scene.objects = {textured};
        frame(3);
        scene.objects = {prop};
        CHECK(frame() == cleanProp);
        scene.camera.position = {0, 3, 4};
        scene.light.direction = {0, 3, -4};
        scene.objects = {road};
        CHECK(frame() == lit);
    }
}

void surfaceDetailChecks(const game::GameAssets& assets, Size size) {
    using namespace engine::math;
    const auto texels = [](const Texture& texture) {
        std::vector<unsigned char> bytes(texture.size().width * texture.size().height * 4);
        glBindTexture(GL_TEXTURE_2D, texture.id());
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, bytes.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        return bytes;
    };
    const auto roadMaps = assets.materialTextures(game::TextureId::Road);
    const auto sidewalkMaps = assets.materialTextures(game::TextureId::Sidewalk);
    CHECK(roadMaps.properties && roadMaps.normal && roadMaps.occlusion == roadMaps.properties);
    CHECK(sidewalkMaps.properties && sidewalkMaps.normal && sidewalkMaps.packedReflectivity);
    CHECK(roadMaps.properties->maxMipLevel() > 0 && sidewalkMaps.normal->maxMipLevel() > 0);
    const auto roadColors = texels(assets.texture(game::TextureId::Road));
    const auto roadData = texels(*roadMaps.properties);
    CHECK(roadColors.size() == roadData.size());
    int paint = 0, tar = 0, aggregate = 0;
    for (std::size_t i = 0; i < roadColors.size(); i += 4) {
        const int r = roadColors[i], g = roadColors[i + 1], b = roadColors[i + 2];
        if ((r == 255 && g == 255 && b == 255) || (r >= 245 && g >= 140 && g <= 200 && b < 40)) {
            CHECK(roadData[i + 1] < 145 && roadData[i + 3] >= 230); // Smooth, reflective paint.
            ++paint;
        } else if (r <= 25 && g <= 25 && b <= 25) {
            CHECK(roadData[i + 1] > 220 && roadData[i + 3] < 180); // Rough, dull asphalt/tar.
            ++tar;
        } else if (r == 50 && g == 50 && b == 50) {
            CHECK(roadData[i + 1] < 215 && roadData[i + 3] > 170); // Exposed aggregate catches light.
            ++aggregate;
        }
    }
    CHECK(paint > 500 && tar > 10000 && aggregate > 100);
    const auto sidewalkColors = texels(assets.texture(game::TextureId::Sidewalk));
    const auto sidewalkData = texels(*sidewalkMaps.properties);
    CHECK(sidewalkColors.size() == sidewalkData.size());
    int stones = 0, dirt = 0;
    for (std::size_t i = 0; i < sidewalkColors.size(); i += 4) {
        if (sidewalkColors[i] == 190) {
            CHECK(sidewalkData[i + 1] < 190 && sidewalkData[i + 3] > 210);
            ++stones;
        } else if (sidewalkColors[i] == 55) {
            CHECK(sidewalkData[i + 1] > 240 && sidewalkData[i + 3] < 100);
            ++dirt;
        }
    }
    CHECK(stones > 100 && dirt > 100);
    const auto facadeMaps = assets.materialTextures(game::TextureId::Skyscraper);
    CHECK(facadeMaps.properties && facadeMaps.normal && facadeMaps.glass);
    CHECK(!facadeMaps.properties->repeats() && !facadeMaps.normal->repeats() && !facadeMaps.glass->repeats());
    CHECK(facadeMaps.glass->maxMipLevel() > 0 && facadeMaps.occlusion == facadeMaps.properties);
    const auto facadeColors = texels(assets.texture(game::TextureId::Skyscraper));
    const auto facadeData = texels(*facadeMaps.properties);
    const auto facadeMask = texels(*facadeMaps.glass);
    CHECK(facadeData.size() == facadeColors.size() && facadeMask.size() == facadeColors.size());
    std::array<bool, 256> paneRoughness{};
    int glassTexels = 0, frameTexels = 0;
    for (std::size_t i = 0; i < facadeColors.size(); i += 4) {
        CHECK(facadeData[i + 2] == 0); // Glass and masonry are dielectric; B remains editable for metal trim.
        if (facadeColors[i + 2] > 200) {
            CHECK(facadeMask[i] == 255 && facadeData[i + 1] < 100 && facadeData[i + 3] > 190);
            paneRoughness[facadeData[i + 1]] = true;
            ++glassTexels;
        } else {
            CHECK(facadeMask[i] == 0 && facadeData[i + 1] > 200 && facadeData[i + 3] < 165);
            ++frameTexels;
        }
    }
    CHECK(glassTexels > 6000 && frameTexels > 1800);
    CHECK(std::count(paneRoughness.begin(), paneRoughness.end(), true) > 8); // Constant blue texels have distinct finishes.
    CHECK(!roadMaps.glass && !sidewalkMaps.glass);
    const auto& bench = assets.model(game::ModelId::Bench).primitives();
    CHECK(bench[0].material.textures().normal && bench[1].material.textures().normal);
    CHECK(bench[0].material.textures().properties && bench[1].material.packedOcclusion);

    RenderSettings settings;
    settings.retroSize = size;
    settings.shadowMapSize = 64;
    settings.retroPixelStep = 0;
    SceneRenderer renderer(settings);
    Scene scene;
    scene.camera.position = {0, 0, 4};
    scene.camera.target = {0, 0, 0};
    scene.light.direction = {0, 0, 1};
    scene.sky.top = scene.sky.bottom = {0, 0, 0};
    scene.sky.sunSize = scene.sky.cloudCoverage = 0;
    constexpr float pigment[] = {.5f};
    Texture albedo = Texture::fromRedChannel({1, 1}, pigment);
    constexpr float coat[] = {1};
    Texture glassMask = Texture::fromRedChannel({1, 1}, coat);
    const auto dataTexture = [](const std::array<unsigned char, 8>& bytes) {
        constexpr float placeholder[] = {0, 0};
        auto result = Texture::fromRedChannel({2, 1}, placeholder);
        glBindTexture(GL_TEXTURE_2D, result.id());
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, bytes.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        return result;
    };
    auto properties = dataTexture({255, 60, 255, 255, 255, 240, 255, 90});
    auto normals = dataTexture({191, 128, 238, 255, 64, 128, 238, 255});
    const auto update = [&](const std::array<unsigned char, 8>& bytes) {
        glBindTexture(GL_TEXTURE_2D, properties.id());
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, bytes.data());
    };
    Quad panel;
    panel.model = scale({4, 5, 1});
    panel.normal = {0, 0, 1};
    panel.texture = &albedo;
    panel.worldSize = {4, 5};
    panel.castsShadow = panel.receivesShadow = false;
    panel.affineStrength = 0;
    panel.pbrSurface = true;
    panel.surfaceMaterial = {1, 0, 1};
    const auto frame = [&](const Quad& surface, float time = 0) {
        scene.objects = {surface};
        scene.time = time;
        renderer.render(scene, size);
        checkState();
        return capture(size);
    };
    const auto sample = [&](const auto& pixels, bool right) {
        std::array<int, 3> value{};
        const int cx = size.width * (right ? 3 : 1) / 4, cy = size.height / 2;
        for (int y = cy - 1; y <= cy + 1; ++y) {
            for (int x = cx - 1; x <= cx + 1; ++x) {
                for (int channel = 0; channel < 3; ++channel) {
                    value[channel] += pixels[(y * size.width + x) * 3 + channel];
                }
            }
        }
        for (int& channel : value) {
            channel /= 9;
        }
        return value;
    };
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        renderer.setMode(mode);
        panel.buildingSurface = panel.skyscraperWindows = false;
        panel.surfaceMaterial = {1, 0, 1};
        panel.materialTextures = {.properties = &properties, .packedReflectivity = true};
        scene.light.direction = {0, 0, 1};
        update({255, 60, 255, 255, 255, 240, 255, 90});
        const auto varied = frame(panel);
        CHECK(sample(varied, false)[1] > sample(varied, true)[1] + 25); // Same red albedo; smoother texels reflect more white sun.
        CHECK(frame(panel, 19) == varied);
        scene.light.direction = {0, 0, -1};
        const auto backlit = frame(panel);
        CHECK(sample(backlit, false)[1] == 0 && sample(backlit, true)[1] == 0); // No painted-in sheen.
        scene.light.direction = {0, 0, 1};
        update({255, 100, 255, 255, 255, 100, 255, 0});
        const auto reflective = frame(panel);
        CHECK(sample(reflective, false)[1] > sample(reflective, true)[1] + 25);
        panel.materialTextures.packedReflectivity = false;
        const auto standard = frame(panel);
        CHECK(std::abs(sample(standard, false)[1] - sample(standard, true)[1]) <= 2); // glTF ignores map alpha.
        update({255, 60, 0, 255, 255, 60, 255, 255});
        panel.surfaceMaterial.metallic = 1;
        const auto metal = frame(panel);
        CHECK(sample(metal, false)[1] > 40 && sample(metal, true)[1] == 0); // B selects dielectric vs red metal.
        panel.surfaceMaterial = {.7f, 0, 0};
        panel.materialTextures = {.normal = &normals};
        scene.light.direction = {1, 0, 1};
        const auto litGrain = frame(panel);
        CHECK(sample(litGrain, false)[0] > sample(litGrain, true)[0] + 20);
        scene.light.direction = {-1, 0, 1};
        const auto opposite = frame(panel);
        CHECK(sample(opposite, true)[0] > sample(opposite, false)[0] + 20);
        panel.materialTextures.normalScale = 0;
        const auto flat = frame(panel);
        CHECK(std::abs(sample(flat, false)[0] - sample(flat, true)[0]) <= 2);
        Quad unmapped = panel;
        unmapped.materialTextures = {};
        const auto clean = frame(unmapped);
        panel.materialTextures = {.properties = &properties, .normal = &normals, .occlusion = &properties,
                                  .packedReflectivity = true};
        frame(panel);
        CHECK(frame(unmapped) == clean); // Map/normal/occlusion state does not leak into the next asset.

        // Exercise the facade path with red glass: the mask, rather than albedo
        // color or the old global window finish, selects its coat and properties.
        panel.buildingSurface = panel.skyscraperWindows = true;
        panel.surfaceMaterial = {1, 1, 1};
        panel.materialTextures = {.properties = &properties, .packedReflectivity = true, .glass = &glassMask};
        scene.light.direction = {0, 0, 1};
        update({255, 60, 0, 255, 255, 240, 0, 90});
        const auto mappedGlass = frame(panel);
        CHECK(sample(mappedGlass, false)[1] > sample(mappedGlass, true)[1] + 25);
        CHECK(frame(panel, 19) == mappedGlass);
        panel.windowMaterial = {1, 0};
        CHECK(frame(panel) == mappedGlass); // Generated roughness/reflection values are authoritative for panes too.
        update({255, 100, 0, 255, 255, 100, 0, 0});
        const auto paneReflection = frame(panel);
        CHECK(sample(paneReflection, false)[1] > sample(paneReflection, true)[1] + 25);
        update({255, 60, 0, 255, 255, 60, 255, 255});
        const auto mappedMetal = frame(panel);
        CHECK(sample(mappedMetal, false)[1] > 40 && sample(mappedMetal, true)[1] == 0);
        panel.surfaceMaterial = {1, 0, 0};
        update({255, 255, 0, 0, 255, 255, 0, 0});
        panel.materialTextures.normal = &normals;
        scene.light.direction = {1, 0, 1};
        const auto paneNormal = frame(panel);
        CHECK(sample(paneNormal, false)[0] > sample(paneNormal, true)[0] + 10);
        scene.light.direction = {-1, 0, 1};
        const auto reverseNormal = frame(panel);
        CHECK(sample(reverseNormal, true)[0] > sample(reverseNormal, false)[0] + 10);
        panel.materialTextures.normal = nullptr;
        scene.light.direction = {0, 0, 1};
        const auto redCoat = frame(panel);
        panel.skyscraperWindows = false;
        CHECK(frame(panel) == redCoat); // An authored glass mask works independently of the legacy blue-glass flag.
        Quad bare = panel;
        bare.materialTextures = {};
        const auto bareColor = frame(bare);
        CHECK(sample(bareColor, false)[0] > sample(redCoat, false)[0] + 10);
        frame(panel);
        CHECK(frame(bare) == bareColor); // The glass-mask binding and finish do not leak to another facade.
    }
}

void sidewalkSamplingChecks(const game::GameAssets& assets, game::WorldScene& world, Size size) {
    RenderSettings settings;
    settings.shadowMapSize = 64; // Isolate sampling rather than shadow changes.
    SceneRenderer renderer(settings);
    auto sidewalks = [&](float scroll) {
        Scene scene = world.build(0, scroll, 2, {}, {});
        std::erase_if(scene.objects, [&](const auto& object) {
            const auto* quad = std::get_if<Quad>(&object);
            return !quad || quad->texture != &assets.texture(game::TextureId::Sidewalk);
        });
        for (auto& object : scene.objects) {
            auto& quad = std::get<Quad>(object);
            quad.receivesShadow = quad.castsShadow = false;
        }
        scene.sky.top = scene.sky.bottom = {0, 0, 0};
        scene.sky.sunSize = scene.sky.cloudCoverage = 0;
        return scene;
    };
    auto frame = [&](const Scene& scene) {
        renderer.render(scene, size);
        checkState();
        return capture(size);
    };
    renderer.setMode(RenderMode::Psx);
    std::vector<unsigned char> previousRaw, previousFiltered, previousMask;
    std::uint64_t rawChange = 0, filteredChange = 0, samples = 0;
    for (int index = 0; index < 32; ++index) {
        Scene scene = sidewalks(5 + index * GameConfig::kInitialSpeed / 60);
        const auto filtered = frame(scene);
        if (index == 0) {
            CHECK(frame(scene) == filtered); // No time-varying samples or pattern phase.
        }
        for (auto& object : scene.objects) {
            std::get<Quad>(object).psxMinificationLodBias.reset();
        }
        const auto raw = frame(scene);
        Scene maskScene = scene;
        for (auto& object : maskScene.objects) {
            auto& quad = std::get<Quad>(object);
            quad.texture = nullptr;
            quad.pbrSurface = false;
            quad.color = {1, 1, 1};
        }
        const auto mask = frame(maskScene);
        if (index > 0) {
            for (int y = 2; y < size.height - 2; ++y) {
                for (int x = 2; x < size.width - 2; ++x) {
                    bool interior = true;
                    for (int dy : {-2, 0, 2}) {
                        for (int dx : {-2, 0, 2}) {
                            const auto neighbor = static_cast<std::size_t>((y + dy) * size.width + x + dx) * 3;
                            interior = interior && mask[neighbor] > 250 && previousMask[neighbor] > 250;
                        }
                    }
                    if (!interior) {
                        continue; // Silhouette motion is not texture shimmer.
                    }
                    const auto pixel = static_cast<std::size_t>(y * size.width + x) * 3;
                    for (int channel = 0; channel < 3; ++channel) {
                        rawChange += std::abs(int(raw[pixel + channel]) - previousRaw[pixel + channel]);
                        filteredChange +=
                            std::abs(int(filtered[pixel + channel]) - previousFiltered[pixel + channel]);
                        ++samples;
                    }
                }
            }
        }
        previousRaw = raw;
        previousFiltered = filtered;
        previousMask = mask;
    }
    CHECK(samples > 10000 && rawChange > samples * 6);
    CHECK(filteredChange < rawChange * .65); // Materially lower motion shimmer on the actual repeating asset.
    for (auto mode : {RenderMode::Normal}) {
        renderer.setMode(mode);
        Scene scene = sidewalks(5);
        const auto original = frame(scene);
        for (auto& object : scene.objects) {
            std::get<Quad>(object).psxMinificationLodBias.reset();
        }
        CHECK(frame(scene) == original); // The opt-in must not alter other modes.
    }
}

void shadowEdgeChecks(Size size) {
    constexpr int nativeSize = 256;
    RenderSettings settings;
    settings.retroSize = {nativeSize, nativeSize};
    settings.shadowMapSize = 256;
    settings.retroPixelStep = 0; // Keep this check about shadow shading.
    settings.retroDitherStrength = 0;
    SceneRenderer filtered(settings);
    settings.retroDitherStrength = 1;
    SceneRenderer dithered(settings);
    Scene scene;
    scene.camera.position = {0, 0, 4};
    scene.camera.target = {0, 0, 0};
    scene.light.direction = {1.6f, .6f, 1};
    scene.light.ambient = .25f;
    scene.light.shadow.halfWidth = scene.light.shadow.halfHeight = 6;
    scene.light.shadow.lightDistance = 24;
    scene.light.shadow.farPlane = 64;
    scene.sky.sunSize = scene.sky.cloudCoverage = 0;
    Quad receiver;
    receiver.model = engine::math::scale({12, 12, 1});
    receiver.worldSize = {12, 12};
    receiver.normal = {0, 0, 1};
    receiver.color = {.74f, .67f, .55f};
    receiver.castsShadow = false;
    Quad caster;
    // The offscreen caster puts a broad shadow down the center of the receiver.
    caster.model = engine::math::multiply(engine::math::translation({3.2f, .8f, 2}),
                                          engine::math::scale({2.8f, 12, 1}));
    auto frame = [&](SceneRenderer& renderer) {
        renderer.render(scene, size);
        checkState();
        return capture(size);
    };
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        filtered.setMode(mode);
        dithered.setMode(mode);
        scene.objects = {receiver};
        const auto fullyLit = frame(dithered);
        CHECK(frame(filtered) == fullyLit); // Lit flat colors remain clean.
        if (mode == RenderMode::Psx) {
            std::get<Quad>(scene.objects.front()).castsShadow = true;
            CHECK(frame(dithered) == fullyLit); // The PCF footprint must not stipple self-shadow acne.
            scene.objects = {receiver};
        }
        scene.objects.emplace_back(caster);
        const auto smooth = frame(filtered);
        const auto stippled = frame(dithered);
        if (mode != RenderMode::Psx) {
            CHECK(smooth == stippled);
            continue;
        }
        std::size_t changed = 0;
        for (int y = 0; y < size.height; ++y) {
            for (int x = 0; x < size.width; ++x) {
                const int nativeX = x * nativeSize / size.width;
                const auto index = (y * size.width + x) * 3;
                const bool same = smooth[index] == stippled[index] && smooth[index + 1] == stippled[index + 1] &&
                                  smooth[index + 2] == stippled[index + 2];
                if (nativeX < nativeSize / 8 || nativeX > nativeSize * 7 / 8 ||
                    (nativeX > nativeSize * .42f && nativeX < nativeSize * .58f)) {
                    CHECK(same); // Fully lit areas and the deep shadow stay solid.
                }
                changed += !same;
            }
        }
        CHECK(changed > 30); // The actual cast-shadow edges get the ordered blend.
        CHECK(frame(dithered) == stippled); // No random/time-dependent shadow noise.
        // Follow the same material points during an integer-pixel camera pan.
        // A screen-anchored shadow pattern would flip between the two lighting colors.
        const float worldPixel = 8 * std::tan(engine::math::radians(30)) / nativeSize;
        const float fit = float(std::min(size.width, size.height)) / nativeSize;
        auto sample = [&](const auto& pixels, int nativeX, int nativeY, int channel) {
            const int x = int((size.width - nativeSize * fit) * .5f + (nativeX + .5f) * fit);
            const int y = int((size.height - nativeSize * fit) * .5f + (nativeY + .5f) * fit);
            return int(pixels[(y * size.width + x) * 3 + channel]);
        };
        for (int pan : {1, 2, 3}) {
            scene.camera.position.x = scene.camera.target.x = pan * worldPixel;
            const auto moving = frame(dithered);
            for (int y = nativeSize / 4; y < nativeSize * 3 / 4; ++y) {
                for (int x = nativeSize / 8; x < nativeSize * 7 / 8 - pan; ++x) {
                    for (int channel = 0; channel < 3; ++channel) {
                        // Presentation can differ by one RGB555 step; a shadow flip is much larger.
                        CHECK(std::abs(sample(moving, x, y, channel) - sample(stippled, x + pan, y, channel)) <= 10);
                    }
                }
            }
        }
        scene.camera.position.x = scene.camera.target.x = 0;
    }
}

void ditheredShadowChecks(const game::GameAssets& assets, Size size) {
    using namespace engine::math;
    constexpr int nativeSize = 256;
    RenderSettings settings;
    settings.retroSize = {nativeSize, nativeSize};
    settings.shadowMapSize = 256;
    settings.retroPixelStep = 0;
    settings.psxDitheredShadows = true;
    SceneRenderer renderer(settings);
    CHECK(renderer.psxDitheredShadows());
    settings.psxDitheredShadowDarkness = 0;
    SceneRenderer previous(settings);
    Scene scene;
    scene.camera.position = {0, 0, 4};
    scene.camera.target = {0, 0, 0};
    scene.light.direction = {1.6f, .6f, 1};
    scene.light.ambient = .25f;
    scene.light.shadow.halfWidth = scene.light.shadow.halfHeight = 6;
    scene.light.shadow.lightDistance = 24;
    scene.light.shadow.farPlane = 64;
    scene.sky.sunSize = scene.sky.cloudCoverage = 0;
    Quad receiver;
    receiver.model = scale({12, 12, 1});
    receiver.worldSize = {12, 12};
    receiver.normal = {0, 0, 1};
    receiver.color = {.74f, .67f, .55f};
    receiver.castsShadow = false;
    Quad textured = receiver;
    textured.texture = &assets.texture(game::TextureId::Sidewalk);
    Quad building = receiver;
    building.texture = &assets.texture(game::TextureId::Skyscraper);
    building.skyscraperWindows = true;
    Quad material = textured;
    material.pbrSurface = true;
    material.surfaceMaterial = {.94f, 0, .7f};
    Cube cube{scale({12, 12, .02f}), receiver.color};
    Cube materialCube = cube;
    materialCube.pbrSurface = true;
    materialCube.surfaceMaterial = {.45f, 0, 1};
    engine::assets::ModelData data;
    engine::assets::PrimitiveData primitive;
    primitive.vertices = {-6, -6, 0, 0, 0, 1, 0, 0, 6, -6, 0, 0, 0, 1, 1, 0,
                           6, 6, 0, 0, 0, 1, 1, 1, -6, 6, 0, 0, 0, 1, 0, 1};
    primitive.indices = {0, 1, 2, 0, 2, 3};
    primitive.material.baseColor = {.74f, .67f, .55f, 1};
    data.primitives.push_back(primitive);
    Model model(data);
    const std::vector<DrawCommand> receivers = {receiver, textured, building, material, cube, materialCube,
                                               ModelInstance{identity(), &model},
                                               ModelInstance{identity(), &model, true}};
    Quad caster;
    caster.model = multiply(translation({3.2f, .8f, 2}), scale({2.8f, 12, 1}));
    auto frame = [&](SceneRenderer& target) {
        target.render(scene, size);
        checkState();
        return capture(size);
    };
    const float fit = float(std::min(size.width, size.height)) / nativeSize;
    auto sample = [&](const auto& pixels, int x, int y, int channel) {
        const int outputX = int((size.width - nativeSize * fit) * .5f + (x + .5f) * fit);
        const int outputY = int((size.height - nativeSize * fit) * .5f + (y + .5f) * fit);
        return int(pixels[(outputY * size.width + outputX) * 3 + channel]);
    };
    for (const auto& surface : receivers) {
        for (float ambient : {.25f, .8f}) {
            scene.light.ambient = ambient;
            for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
                renderer.setMode(mode);
                previous.setMode(mode);
                renderer.setPsxDitheredShadows(false);
                CHECK(!renderer.psxDitheredShadows());
                scene.objects = {surface};
                const auto lit = frame(renderer);
                renderer.setPsxDitheredShadows(true);
                CHECK(renderer.psxDitheredShadows());
                CHECK(frame(renderer) == lit); // An unoccluded material must not acquire a checkerboard.
                CHECK(frame(previous) == lit);
                scene.objects.emplace_back(caster);
                const auto mesh = frame(renderer);
                const auto faint = frame(previous);
                renderer.setPsxDitheredShadows(false);
                const auto solid = frame(renderer);
                if (mode == RenderMode::Psx) {
                    std::size_t contrast = 0, strengthened = 0;
                    for (int y = nativeSize / 4; y < nativeSize * 3 / 4; ++y) {
                        for (int x = 110; x < 146; ++x) {
                            for (int channel = 0; channel < 3; ++channel) {
                                const int bright = sample(lit, x, y, channel);
                                const int dark = sample(solid, x, y, channel);
                                const int old = sample(faint, x, y, channel);
                                const int actual = sample(mesh, x, y, channel);
                                if ((x + y) % 2 != 0) {
                                    CHECK(std::abs(old - bright) <= 10);
                                    CHECK(std::abs(actual - bright) <= 10); // Holes still expose the lit texture.
                                } else {
                                    CHECK(std::abs(old - dark) <= 10); // Zero darkness preserves the former mesh look.
                                    CHECK(actual <= old + 10);
                                    strengthened += old - actual > 8;
                                    contrast += bright - actual > 20;
                                }
                            }
                        }
                    }
                    // More contrast even with strong ambient fill and metallic sky reflections.
                    CHECK(contrast > 1000 && strengthened > 1000 && mesh != solid);
                } else {
                    CHECK(mesh == solid && faint == solid); // Normal mode ignores both mesh settings.
                }
                renderer.setPsxDitheredShadows(true);
                CHECK(frame(renderer) == mesh);
                renderer.setPsxDitheredShadows(false);
                CHECK(frame(renderer) == solid); // Toggling back restores shadows without lingering state.
            }
            // Self-occlusion on a face turned away from the sun must not dither ambient fill.
            scene.light.direction.z = -1;
            scene.objects = {surface};
            renderer.setMode(RenderMode::Psx);
            const auto sunAway = frame(renderer);
            renderer.setPsxDitheredShadows(true);
            CHECK(frame(renderer) == sunAway);
            renderer.setPsxDitheredShadows(false);
            scene.light.direction.z = 1;
        }
    }
}

void subdivisionChecks(const game::GameAssets& assets, Size size) {
    using namespace engine::math;
    RenderSettings settings;
    settings.retroSize = {160, 160};
    settings.shadowMapSize = 64;
    settings.retroPixelStep = 0; // Isolate UV distortion from vertex coordinate rounding.
    settings.psxSubdivideQuads = false;
    SceneRenderer coarse(settings);
    settings.psxSubdivideQuads = true;
    SceneRenderer subdivided(settings);
    coarse.setMode(RenderMode::Psx);
    subdivided.setMode(RenderMode::Psx);
    Scene scene;
    scene.camera.position = {0, 0, 4};
    scene.camera.target = {0, 0, 0};
    scene.sky.cloudCoverage = 0;
    scene.sky.sunSize = 0;
    Quad road;
    road.texture = &assets.texture(game::TextureId::Road);
    road.model = multiply(rotateX(radians(55)), scale({3, 4, 1}));
    road.worldSize = {3, 4};
    road.receivesShadow = road.castsShadow = false;
    for (bool pbr : {false, true}) {
        road.pbrSurface = pbr;
        road.model.values[14] = 0;
        road.affineStrength = 0;
        scene.objects = {road};
        coarse.render(scene, size);
        const auto reference = capture(size);
        road.affineStrength = 1;
        scene.objects = {road};
        coarse.render(scene, size);
        const auto warped = capture(size);
        subdivided.render(scene, size);
        const auto improved = capture(size);
        auto error = [&](const auto& frame) {
            std::uint64_t difference = 0;
            for (std::size_t i = 0; i < frame.size(); ++i) {
                difference += std::abs(int(frame[i]) - reference[i]);
            }
            return difference;
        };
        CHECK(error(improved) < error(warped) * .75);
        CHECK(improved != reference); // Every subdivided triangle still uses affine interpolation.
        GLuint query = 0;
        glGenQueries(1, &query);
        GLuint topology = 0;
        for (float distance : {0.0f, 3.0f, 10.0f, 60.0f}) {
            road.model.values[14] = -distance;
            scene.objects = {road};
            glBeginQuery(GL_PRIMITIVES_GENERATED, query);
            subdivided.render(scene, size);
            glEndQuery(GL_PRIMITIVES_GENERATED);
            GLuint primitives = 0;
            glGetQueryObjectuiv(query, GL_QUERY_RESULT, &primitives);
            if (distance == 0) {
                topology = primitives;
            }
            CHECK(primitives == topology); // Camera motion cannot switch the affine triangle grid.
        }
        glDeleteQueries(1, &query);
        checkState();
    }
}

void lensChecks(SceneRenderer& renderer, Size size) {
    using namespace engine::math;
    Scene scene;
    scene.camera.position = {0, 0, 4};
    scene.camera.target = {0, 0, 0};
    scene.sky.top = scene.sky.bottom = {.15f, .25f, .4f};
    scene.sky.sunSize = 0;
    scene.sky.cloudCoverage = 0;
    Quad pole;
    pole.model = multiply(translation({.9f, 0, 0}), scale({.12f, 6, 1}));
    pole.color = {1, .1f, .1f};
    pole.normal = {0, 0, 1};
    pole.receivesShadow = pole.castsShadow = false;
    scene.objects = {pole};
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        renderer.setMode(mode);
        auto frame = [&](float strength) {
            scene.camera.fisheyeStrength = strength;
            renderer.render(scene, size);
            checkState();
            return capture(size);
        };
        const auto linear = frame(0);
        const auto subtle = frame(GameConfig::kCameraFisheyeStrength);
        CHECK(subtle != linear);
        const auto curved = frame(.25f);
        CHECK(curved != subtle && frame(.25f) == curved);
        const float fit = std::min(size.width / 240.0f, size.height / 320.0f);
        const float contentHeight = mode == RenderMode::Normal ? size.height : 320 * fit;
        const int centerY = size.height / 2;
        const int upperY = int(centerY + contentHeight * .3f);
        auto poleCenter = [&](const auto& pixels, int y) {
            float sum = 0;
            int count = 0;
            for (int x = 0; x < size.width; ++x) {
                const auto index = (y * size.width + x) * 3;
                if (pixels[index] > 160 && pixels[index] > pixels[index + 2] + 80) {
                    sum += x;
                    ++count;
                }
            }
            CHECK(count > 0);
            return sum / count;
        };
        CHECK(std::abs(poleCenter(linear, centerY) - poleCenter(linear, upperY)) < 1);
        CHECK(poleCenter(curved, centerY) > poleCenter(curved, upperY) + 1); // Visible barrel curvature.
        // Check the four camera-image corners, excluding legitimate letterboxing.
        const float contentWidth = mode == RenderMode::Normal ? size.width : 240 * fit;
        for (int y : {int((size.height - contentHeight) / 2) + 2,
                      int((size.height + contentHeight) / 2) - 3}) {
            for (int x : {int((size.width - contentWidth) / 2) + 2,
                          int((size.width + contentWidth) / 2) - 3}) {
                const auto index = (y * size.width + x) * 3;
                CHECK(curved[index] + curved[index + 1] + curved[index + 2] > 40);
            }
        }
        CHECK(frame(0) == linear);
        CHECK(frame(-1) == linear);
        CHECK(frame(std::numeric_limits<float>::quiet_NaN()) == linear);
        CHECK(frame(1) == curved); // Clamp excessive strength.
        scene.menu.visible = true;
        const auto menu = frame(0);
        CHECK(frame(.25f) != menu); // The game view keeps its lens underneath the menu.
        scene.menu.visible = false;
    }
    renderer.setMode(RenderMode::Normal);
}

void menuOverlayChecks(game::WorldScene& world, SceneRenderer& renderer, Size size) {
    // Match the facade's frozen opening scene, with UI animation on a separate clock.
    Scene scene = world.build(0, 0, 0, {}, {});
    auto frame = [&](float lens, bool menu, float uiTime = 0, int selection = 0) {
        scene.camera.fisheyeStrength = lens;
        scene.menu = {.visible = menu, .selection = selection, .time = uiTime};
        renderer.render(scene, size);
        checkState();
        return capture(size);
    };
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        renderer.setMode(mode);
        const auto background = frame(.25f, false);
        const auto menu = frame(.25f, true);
        const auto pulsing = frame(.25f, true, .4f);
        const auto navigating = frame(.25f, true, .4f, 1);
        const auto linearMenu = frame(0, true);
        CHECK(menu != background && pulsing != menu && navigating != pulsing);
        CHECK(frame(.25f, true, .4f, 1) == navigating);
        CHECK(frame(.25f, false) == background); // Starting play removes every part of the overlay.
        const float fit = std::min(size.width / 240.0f, size.height / 320.0f);
        const float width = mode == RenderMode::Normal ? size.width : 240 * fit;
        const float height = mode == RenderMode::Normal ? size.height : 320 * fit;
        const float left = (size.width - width) * .5f;
        const float top = (size.height + height) * .5f;
        std::size_t preserved = 0, titlePixels = 0;
        for (int y = 0; y < size.height; ++y) {
            for (int x = 0; x < size.width; ++x) {
                const int uiX = int(std::floor((x + .5f - left) / width * 240));
                const int uiY = int(std::floor((top - y - .5f) / height * 320));
                const bool title = uiX >= 12 && uiX <= 228 && uiY >= 54 && uiY <= 72;
                const bool options = uiX >= 51 && uiX <= 189 && uiY >= 125 && uiY <= 243;
                const auto index = (y * size.width + x) * 3;
                for (int channel = 0; channel < 3; ++channel) {
                    if (!title && !options) {
                        CHECK(menu[index + channel] == background[index + channel]);
                        CHECK(pulsing[index + channel] == background[index + channel]);
                        CHECK(navigating[index + channel] == background[index + channel]);
                        ++preserved; // Transparent pixels retain the frozen world, sky, lens and letterboxing.
                    }
                }
                if (title && menu[index] > 230 && menu[index + 1] > 185 && menu[index + 2] < 80) {
                    for (int channel = 0; channel < 3; ++channel) {
                        CHECK(menu[index + channel] == linearMenu[index + channel]);
                    }
                    ++titlePixels; // Opaque text stays straight and in front of foreground buildings.
                }
            }
        }
        CHECK(preserved > static_cast<std::size_t>(size.width) * size.height && titlePixels > 100);
    }
    renderer.setMode(RenderMode::Normal);
}

void sceneChecks() {
    engine::platform::Window window({"Renderer regression tests", {360, 640}, true, false});
    game::GameAssets assets(TEST_ASSET_DIR);
    game::WorldScene world(assets);
    SceneRenderer renderer;
    CHECK(!renderer.psxDitheredShadows());
    for (float scroll : {0.0f, 5.0f, 37.0f, 99.0f, 1217.0f}) {
        buildingShaderChecks(assets, world, scroll);
    }
    skyCloudChecks(window.drawableSize());
    buildingMaterialChecks(assets, window.drawableSize());
    streetMaterialChecks(assets, world, window.drawableSize());
    surfaceDetailChecks(assets, window.drawableSize());
    sidewalkSamplingChecks(assets, world, window.drawableSize());
    shadowEdgeChecks(window.drawableSize());
    ditheredShadowChecks(assets, window.drawableSize());
    collectibleChecks(assets, world, renderer, window.drawableSize());
    collectionBurstChecks(world, renderer, window.drawableSize());
    subdivisionChecks(assets, window.drawableSize());
    lensChecks(renderer, window.drawableSize());
    const std::vector<game::Entity> entities = {
        {game::EntityType::Obstacle, 0, -2, 0, 0},          {game::EntityType::Obstacle, 2, -8, 0, 1},
        {game::EntityType::ScoreCollectible, 1, -13, 0, 0}, {game::EntityType::SlowCollectible, 0, -19, 0, 1},
        {game::EntityType::Obstacle, 1, -35, 0, 0},         {game::EntityType::Obstacle, 0, -60, 0, 1}};
    const std::vector<game::CollectEffect> effects = {{game::EntityType::ScoreCollectible, 2, -4, 1.8f}};
    for (Size requested : {Size{360, 640}, Size{640, 360}, Size{240, 384}}) {
        window.setSize(requested);
        SDL_PumpEvents();
        renderer.render(world.build(0, 5, 2, entities, effects), window.drawableSize());
        window.swap();
        SDL_PumpEvents();
        const Size size = window.drawableSize();
        menuOverlayChecks(world, renderer, size);
        if (size.width > size.height) {
            skyCloudChecks(size); // Square cloud pixels and angular discs in landscape too.
        }
        renderer.setRetroMode(false);
        renderer.render(world.build(0, 5, 2, entities, effects), size);
        const auto normal = capture(size);
        checkState();
        CHECK(std::count_if(normal.begin(), normal.end(), [](auto value) { return value > 30; }) > 1000);
        renderer.setRetroMode(true);
        renderer.render(world.build(0, 5, 2, entities, effects), size);
        const auto retro = capture(size);
        CHECK(retro != normal);
        checkState();
        renderer.setRetroMode(true);
        renderer.render(world.build(0, 5, 2, entities, effects), size);
        CHECK(!glIsEnabled(GL_DITHER)); // Explicit PS1 dithering replaces driver dithering.
        CHECK(capture(size) == retro);
        renderer.setRetroMode(false);
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, 0, 1, 1);
        glDepthFunc(GL_ALWAYS);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glBlendEquation(GL_FUNC_SUBTRACT);
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        glEnable(GL_SAMPLE_COVERAGE);
        glDisable(GL_MULTISAMPLE);
        renderer.render(world.build(0, 5, 2, entities, effects), size);
        CHECK(capture(size) == normal);
        for (int frame = 0; frame < 12; ++frame) {
            constexpr RenderMode modes[] = {RenderMode::Normal, RenderMode::Psx};
            renderer.setMode(modes[frame % 2]);
            renderer.render(world.build((frame % 3 - 1) * 2.2f, frame * 3.0f, frame / 60.0f, entities, {}), size);
            checkState();
        }
    }
    // Reusable API: draw a scene with no Subway assets or world builder.
    Scene scene;
    scene.camera.position = {0, 4, 7};
    scene.objects.emplace_back(Cube{engine::math::identity(), {0.2f, 0.6f, 1}});
    renderer.setRetroMode(false);
    renderer.render(scene, window.drawableSize());
    checkState();
    renderer.render(scene, {0, 0});
    CHECK(glGetError() == GL_NO_ERROR);
}
void ownershipChecks() {
    // A second independent context verifies teardown/recreation and loading failure recovery.
    engine::platform::Window window({"Resource lifetime tests", {160, 160}, false, false});
    checkThrows([] { Texture missing("/nonexistent/subway-texture.png"); });
    Texture original(std::filesystem::path(TEST_ASSET_DIR) / "water.png", {TextureFilter::Nearest, false, true, true});
    const auto id = original.id();
    const std::vector<unsigned char> alpha(original.alphaMask().begin(), original.alphaMask().end());
    {
        Texture moved(std::move(original));
        CHECK(original.id() == 0 && moved.id() == id);
        CHECK(std::equal(alpha.begin(), alpha.end(), moved.alphaMask().begin(), moved.alphaMask().end()));
        Texture assigned;
        assigned = std::move(moved);
        CHECK(moved.id() == 0 && assigned.id() == id);
        CHECK(std::equal(alpha.begin(), alpha.end(), assigned.alphaMask().begin(), assigned.alphaMask().end()));
        CHECK(glIsTexture(id));
    }
    CHECK(!glIsTexture(id));
    checkThrows([] { Mesh invalid({}, 0, {}); });
    CHECK(glGetError() == GL_NO_ERROR);
}
} // namespace
int main() {
    try {
        sceneChecks();
        ownershipChecks();
        std::cout << "Graphics tests passed: all three modes, resizing, stable output, state and resource lifetimes\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
