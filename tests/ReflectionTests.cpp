#include "TestSupport.h"
#include "engine/graphics/Model.h"
#include "engine/graphics/ReflectionMap.h"
#include "engine/graphics/RenderTarget.h"
#include "engine/graphics/SceneRenderer.h"
#include "engine/platform/Window.h"
#include <algorithm>
#include <array>
#include <glad/glad.h>
#include <iostream>
#include <vector>

using namespace engine::graphics;
using namespace engine::math;
namespace {
std::vector<unsigned char> capture(Size size) {
    std::vector<unsigned char> pixels(size.width * size.height * 3);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, size.width, size.height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    CHECK(glGetError() == GL_NO_ERROR);
    return pixels;
}
void stateChecks() {
    GLint value = -1;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &value);
    CHECK(value == 0);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &value);
    CHECK(value == GL_TEXTURE0);
    for (int unit = 0; unit <= 13; ++unit) {
        glGetIntegeri_v(GL_SAMPLER_BINDING, unit, &value);
        CHECK(value == 0);
    }
    glActiveTexture(GL_TEXTURE6);
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &value);
    CHECK(value == 0);
    glActiveTexture(GL_TEXTURE0);
    GLboolean writes = GL_FALSE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &writes);
    CHECK(writes && glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_BLEND));
    CHECK(!glIsEnabled(GL_CLIP_DISTANCE0));
    glGetIntegerv(GL_DEPTH_FUNC, &value);
    CHECK(value == GL_LESS);
    std::array<float, 2> depthRange{};
    glGetFloatv(GL_DEPTH_RANGE, depthRange.data());
    CHECK(depthRange[0] == 0 && depthRange[1] == 1);
    std::array<GLboolean, 4> colorWrites{};
    glGetBooleanv(GL_COLOR_WRITEMASK, colorWrites.data());
    CHECK(std::all_of(colorWrites.begin(), colorWrites.end(), [](auto enabled) { return enabled == GL_TRUE; }));
    CHECK(glGetError() == GL_NO_ERROR);
}
void resourceChecks() {
    checkThrows([] { ReflectionMap invalid(0); });
    unsigned int id = 0;
    {
        ReflectionMap map(16);
        id = map.texture();
        CHECK(glIsTexture(id) && map.maxMipLevel() == 4);
        checkThrows([&] { map.bindFace(6); });
        // Every attachment and every generated mip must refer to the right face.
        for (int face = 0; face < 6; ++face) {
            map.bindFace(face);
            CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
            glClearColor((face + 1) / 8.0f, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        map.generateMipmaps();
        glBindTexture(GL_TEXTURE_CUBE_MAP, id);
        for (int face = 0; face < 6; ++face) {
            std::array<unsigned char, 3> pixel{};
            glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 4, GL_RGB, GL_UNSIGNED_BYTE, pixel.data());
            CHECK(std::abs(int(pixel[0]) - int((face + 1) * 255 / 8.0f)) <= 1);
            CHECK(pixel[1] == 0 && pixel[2] == 0);
        }
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    }
    CHECK(!glIsTexture(id));
    CHECK(glGetError() == GL_NO_ERROR);
}
void worldChecks(Size size) {
    RenderSettings settings;
    settings.retroSize = size;
    settings.retroPixelStep = 0;
    settings.shadowMapSize = 64;
    settings.worldReflectionMapSize = 64;
    settings.planarReflectionSize = 0; // Exercise the distant cubemap fallback independently.
    SceneRenderer renderer(settings);
    Scene scene;
    scene.camera.position = {0, 0, 4};
    scene.camera.target = {0, 0, 0};
    scene.light.direction = {0, 1, 0};
    scene.light.ambient = 1;
    scene.sky.top = scene.sky.bottom = {0, 0, 0};
    scene.sky.sunSize = scene.sky.cloudCoverage = 0;
    constexpr float pigment[] = {.02f}, mask[] = {1};
    Texture backing = Texture::fromRedChannel({1, 1}, pigment);
    Texture glass = Texture::fromRedChannel({1, 1}, mask);
    Quad window;
    window.model = scale({4, 4, 1});
    window.worldSize = {4, 4};
    window.normal = {0, 0, 1};
    window.texture = &backing;
    window.materialTextures.glass = &glass;
    window.skyscraperWindows = true;
    window.castsShadow = false;
    window.affineStrength = 0;
    window.windowMaterial.roughness = .08f;
    window.windowMaterial.reflectance = .04f;
    Quad sign;
    // Behind the camera: its only contribution to the main image is a reflection.
    sign.model = multiply(translation({0, 0, 8}), scale({2, 2, 1}));
    sign.worldSize = {2, 2};
    sign.normal = {0, 0, -1};
    sign.color = {1, 0, 0};
    sign.castsShadow = sign.receivesShadow = false;
    scene.objects = {window, sign};
    const auto frame = [&] {
        renderer.render(scene, size);
        stateChecks();
        return capture(size);
    };
    const auto peak = [size](const auto& pixels, int channel) {
        int result = 0;
        // Compare the central glass region, excluding the procedural sky border.
        for (int y = size.height / 4; y < size.height * 3 / 4; ++y) {
            for (int x = size.width / 4; x < size.width * 3 / 4; ++x) {
                result = std::max(result, int(pixels[(y * size.width + x) * 3 + channel]));
            }
        }
        return result;
    };
    const auto center = [size](const auto& pixels, int channel) {
        return int(pixels[(size.height / 2 * size.width + size.width / 2) * 3 + channel]);
    };
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        renderer.setMode(mode);
        scene.objects = {window, sign};
        const auto red = frame();
        CHECK(center(red, 0) > 40 && center(red, 1) < 10 && center(red, 2) < 10);
        CHECK(frame() == red); // No recursive sampling or stale face history.
        scene.reflectionProbePosition = Vec3{1, 0, 4};
        CHECK(frame() != red); // A caller can place the probe independently of the view camera.
        scene.reflectionProbePosition.reset();
        std::get<Quad>(scene.objects[1]).color = {0, 1, 0};
        const auto green = frame();
        CHECK(center(green, 1) > 40 && center(green, 0) < 10);
        std::get<Quad>(scene.objects[1]).model = multiply(translation({2, 0, 8}), scale({2, 2, 1}));
        CHECK(frame() != green); // Moving geometry updates even with identical scene time.
        scene.objects = {window, sign};
        std::get<Quad>(scene.objects[0]).windowMaterial.roughness = .85f;
        CHECK(peak(frame(), 0) < peak(red, 0) - 15); // Rough glass blurs the captured environment.
        std::get<Quad>(scene.objects[0]).windowMaterial.worldReflections = false;
        const auto disabled = frame();
        CHECK(peak(disabled, 0) < 10);
        scene.objects = {window};
        std::get<Quad>(scene.objects[0]).windowMaterial.worldReflections = false;
        CHECK(frame() == disabled); // The off-screen sign has no diffuse/main-view effect.
        scene.objects = {window, sign};
        std::get<Quad>(scene.objects[0]).windowMaterial.reflectivity = 0;
        CHECK(peak(frame(), 0) < 10);
        scene.objects = {window, sign};
        scene.menu.visible = true;
        frame();
        scene.menu.visible = false;
        CHECK(frame() == red); // UI is never captured into the world environment.
        scene.objects.clear();
        frame(); // A following scene cannot retain a stale reflection binding.
    }
    RenderSettings off = settings;
    off.worldReflectionMapSize = 0;
    SceneRenderer analytic(off);
    scene.objects = {window, sign};
    analytic.render(scene, size);
    CHECK(peak(capture(size), 0) < 10);
    stateChecks();
    off.worldReflectionMapSize = -1;
    checkThrows([&] { SceneRenderer invalid(off); });
}
void planarChecks(Size size) {
    RenderSettings settings;
    settings.retroSize = size;
    settings.retroPixelStep = 0;
    settings.worldReflectionMapSize = 32;
    settings.planarReflectionSize = 128;
    settings.shadowMapSize = 64;
    SceneRenderer renderer(settings);
    Scene scene;
    scene.sky.sunSize = scene.sky.cloudCoverage = 0;
    scene.sky.top = scene.sky.bottom = {0, 0, 0};
    scene.light.ambient = 1;
    scene.light.direction = {0, 1, 0};
    constexpr float zero[] = {0}, one[] = {1};
    Texture backing = Texture::fromRedChannel({1, 1}, zero);
    Texture mask = Texture::fromRedChannel({1, 1}, one);
    Quad window;
    window.texture = &backing;
    window.materialTextures.glass = &mask;
    window.skyscraperWindows = true;
    window.castsShadow = false;
    window.affineStrength = 0;
    window.worldSize = {6, 6};
    window.windowMaterial.roughness = 0;
    Cube character;
    character.color = {0, 0, 1};
    const auto frame = [&] {
        renderer.render(scene, size);
        stateChecks();
        return capture(size);
    };
    const auto checkPosition = [&](const auto& pixels, Vec3 virtualPosition) {
        double xSum = 0, ySum = 0;
        int count = 0;
        for (int y = 0; y < size.height; ++y) {
            for (int x = 0; x < size.width; ++x) {
                const auto index = (y * size.width + x) * 3;
                if (pixels[index + 2] > 100 && pixels[index + 2] > pixels[index] + 60 &&
                    pixels[index + 2] > pixels[index + 1] + 60) {
                    xSum += x + .5;
                    ySum += y + .5;
                    ++count;
                }
            }
        }
        CHECK(count > 20); // The reflected moving cube has a readable silhouette.
        const auto projection = scene.camera.viewProjection(float(size.width) / size.height);
        const auto clip = transformPoint(projection, virtualPosition);
        const auto& m = projection.values;
        const float w = m[3] * virtualPosition.x + m[7] * virtualPosition.y + m[11] * virtualPosition.z + m[15];
        CHECK(std::abs(xSum / count - (clip.x / w * .5 + .5) * size.width) < 2);
        CHECK(std::abs(ySum / count - (clip.y / w * .5 + .5) * size.height) < 2);
    };
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        renderer.setMode(mode);
        for (bool side : {false, true}) {
            scene.camera.position = side ? Vec3{4, 0, 0} : Vec3{0, 0, 4};
            scene.camera.target = {0, 0, 0};
            window.normal = side ? Vec3{1, 0, 0} : Vec3{0, 0, 1};
            window.model = multiply(side ? rotateY(kPi * .5f) : identity(), scale({6, 6, 1}));
            for (float motion : {-.6f, .6f}) {
                const Vec3 position = side ? Vec3{7, -.2f, motion} : Vec3{motion, -.2f, 7};
                const Vec3 virtualPosition = side ? Vec3{-7, -.2f, motion} : Vec3{motion, -.2f, -7};
                character.model = multiply(translation(position), scale({.6f, .6f, .6f}));
                scene.objects = {window, character};
                const auto reflected = frame();
                checkPosition(reflected, virtualPosition);
                CHECK(frame() == reflected);
                // Geometry behind the mirror plane must not obscure the capture.
                Cube wrongSide;
                wrongSide.color = {1, 0, 0};
                // Keep the entire blocker behind the clipping plane.
                wrongSide.model = multiply(translation(side ? Vec3{-2, 0, 0} : Vec3{0, 0, -2}),
                                           scale(side ? Vec3{.5f, 30, 30} : Vec3{30, 30, .5f}));
                scene.objects.push_back(wrongSide);
                CHECK(frame() == reflected);
                scene.objects = {window, character};
                scene.reflectionProbePosition = Vec3{9, 9, 9};
                CHECK(frame() == reflected); // Local geometry is independent of the distant cubemap probe.
                scene.reflectionProbePosition.reset();
                std::get<Quad>(scene.objects[0]).windowMaterial.reflectivity = 0;
                const auto disabled = frame();
                CHECK(disabled != reflected);
                std::get<Quad>(scene.objects[0]).windowMaterial = window.windowMaterial;
                std::get<Quad>(scene.objects[0]).windowMaterial.roughness = 1;
                CHECK(frame() != reflected);
            }
        }
    }
    engine::assets::ModelData modelData;
    engine::assets::PrimitiveData primitive;
    for (const auto position : {Vec3{57, 0, 0}, Vec3{63, 0, 0}, Vec3{63, 6, 0}, Vec3{57, 6, 0}}) {
        primitive.vertices.insert(primitive.vertices.end(), {position.x, position.y, position.z, 0, 0, -1, 0, 0});
    }
    primitive.indices = {0, 1, 2, 0, 2, 3};
    primitive.material.baseColor = {0, 0, 1, 1};
    modelData.primitives.push_back(std::move(primitive));
    Model model(modelData);
    CHECK(model.boundsCenter().x == 60 && model.boundsCenter().y == 3);
    CHECK(model.boundsHalfExtent().x == 3 && model.boundsHalfExtent().y == 3 && model.boundsHalfExtent().z == 0);
    ModelInstance prop;
    prop.model = &model;
    prop.transform = translation({-60, -3, 7}); // Origin is outside the view; the imported vertices are inside it.
    window.normal = {0, 0, 1};
    window.model = scale({6, 6, 1});
    scene.camera.position = {0, 0, 4};
    scene.camera.target = {};
    scene.sky.texturedClouds[0].texture = &backing;
    scene.sky.texturedClouds[0].opacity = 0;
    scene.objects = {window, prop};
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        renderer.setMode(mode);
        checkPosition(frame(), {0, 0, -7});
    }
    RenderSettings invalid = settings;
    invalid.planarReflectionSize = -1;
    checkThrows([&] { SceneRenderer bad(invalid); });
    invalid = settings;
    invalid.maxPlanarReflections = -1;
    checkThrows([&] { SceneRenderer bad(invalid); });
}
void distanceFadeChecks(Size size) {
    RenderSettings settings;
    settings.retroSize = size;
    settings.retroPixelStep = 0;
    settings.shadowMapSize = 64;
    settings.worldReflectionMapSize = 32;
    settings.planarReflectionSize = 128;
    settings.planarReflectionDistance = 6;
    settings.planarReflectionFadeDistance = 2;
    SceneRenderer local(settings);
    auto fallbackSettings = settings;
    fallbackSettings.planarReflectionSize = 0;
    SceneRenderer fallback(fallbackSettings);
    constexpr float zero[] = {0}, one[] = {1};
    Texture backing = Texture::fromRedChannel({1, 1}, zero);
    Texture mask = Texture::fromRedChannel({1, 1}, one);
    Quad window;
    window.model = scale({6, 6, 1});
    window.normal = {0, 0, 1};
    window.texture = &backing;
    window.skyscraperWindows = true;
    window.materialTextures.glass = &mask;
    window.windowMaterial.roughness = 0;
    window.castsShadow = false;
    Quad sign;
    sign.model = multiply(translation({0, 0, 9}), scale({2, 2, 1}));
    sign.normal = {0, 0, -1};
    sign.color = {1, 0, 0};
    sign.castsShadow = sign.receivesShadow = false;
    Scene scene;
    scene.camera.target = {0, 0, 0};
    scene.sky.top = scene.sky.bottom = {0, 0, 0};
    scene.sky.sunSize = scene.sky.cloudCoverage = 0;
    scene.light.ambient = 1;
    scene.reflectionProbePosition = Vec3{6, 0, 4}; // Distinguish local geometry from the distant approximation.
    scene.objects = {window, sign};
    const auto difference = [&](float distance) {
        scene.camera.position = {0, 0, distance};
        local.render(scene, size);
        stateChecks();
        const auto planar = capture(size);
        fallback.render(scene, size);
        stateChecks();
        const auto cubemap = capture(size);
        double total = 0;
        for (std::size_t index = 0; index < planar.size(); ++index) {
            total += std::abs(int(planar[index]) - int(cubemap[index]));
        }
        return total / planar.size();
    };
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        local.setMode(mode);
        fallback.setMode(mode);
        const double near = difference(4);
        const double middle = difference(5);
        const double edge = difference(5.95f);
        CHECK(near > .5);
        CHECK(middle > 0 && middle < near);
        CHECK(edge < near * .1);
        CHECK(difference(6) == 0 && difference(6.1f) == 0); // No image jump at the local-capture boundary.
    }
    settings.planarReflectionFadeDistance = -1;
    checkThrows([&] { SceneRenderer invalid(settings); });
}
void coverageChecks(Size size) {
    RenderSettings settings;
    settings.retroSize = size;
    settings.retroPixelStep = settings.retroAffineStrength = settings.retroDitherStrength = 0;
    settings.shadowMapSize = 32;
    settings.worldReflectionMapSize = 32;
    settings.planarReflectionSize = 128;
    settings.distantPlanarReflectionSize = 64;
    settings.maxPlanarReflections = 1;
    constexpr float zero[] = {0}, one[] = {1};
    Texture backing = Texture::fromRedChannel({1, 1}, zero);
    Texture mask = Texture::fromRedChannel({1, 1}, one);
    Quad left;
    left.normal = {0, 0, 1};
    left.texture = &backing;
    left.skyscraperWindows = true;
    left.materialTextures.glass = &mask;
    left.windowMaterial.roughness = 0;
    left.windowMaterial.reflectance = .35f; // Avoid saturation so intermediate handover brightness is measurable.
    left.castsShadow = left.receivesShadow = false;
    left.reflectionId = 1;
    Quad right = left;
    right.reflectionId = 2;
    Quad sign;
    sign.model = multiply(translation({0, 0, 9}), scale({100, 100, 1}));
    sign.normal = {0, 0, -1};
    sign.castsShadow = sign.receivesShadow = false;
    sign.color = {1, 0, 0};
    Scene scene;
    scene.camera.position = {0, 0, 5};
    scene.camera.target = {};
    scene.sky.top = scene.sky.bottom = {0, 0, 0};
    scene.sky.texturedClouds[0].texture = &backing;
    scene.sky.texturedClouds[0].opacity = 0; // Direct, black authored sky; skip the distant cubemap.
    scene.light.ambient = 1;
    const auto sample = [&](const auto& pixels, Vec3 point, int channel) {
        const auto projection = scene.camera.viewProjection(static_cast<float>(size.width) / size.height);
        const auto clip = transformPoint(projection, point);
        const auto& m = projection.values;
        const float w = m[3] * point.x + m[7] * point.y + m[11] * point.z + m[15];
        const int x = (clip.x / w * .5f + .5f) * size.width;
        const int y = (clip.y / w * .5f + .5f) * size.height;
        return int(pixels[(y * size.width + x) * 3 + channel]);
    };
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        SceneRenderer renderer(settings);
        renderer.setMode(mode);
        scene.time = 1;
        left.model = multiply(translation({-1.3f, 0, 0}), scale({2.4f, 4, 1}));
        right.model = multiply(translation({1.3f, 0, -.05f}), scale({2.2f, 4, 1}));
        sign.color = {1, 0, 0};
        const auto frame = [&] {
            scene.objects = {left, right, sign};
            renderer.render(scene, size);
            stateChecks();
            return capture(size);
        };
        const auto initial = frame();
        const int litLeft = sample(initial, {-1.3f, 0, 0}, 0);
        const int litRight = sample(initial, {1.3f, 0, -.05f}, 0);
        CHECK(litLeft > 150 && litRight > 150); // The budget limits detail, not visible reflection coverage.
        for (int tick = 0; tick < 8; ++tick) {
            scene.time += 1.f / 60;
            right.model.values[0] = tick % 2 ? 2.42f : 2.38f;
            const auto tied = frame();
            CHECK(sample(tied, {-1.3f, 0, 0}, 0) >= litLeft - 2);
            CHECK(sample(tied, {1.3f, 0, -.05f}, 0) >= litRight - 2);
        }
        left.model.values[0] = .8f; // Swapping detail priority must not fade either face out.
        scene.time += 1.f / 60;
        const auto incoming = frame();
        CHECK(sample(incoming, {-1.3f, 0, 0}, 0) >= litLeft - 2);
        CHECK(sample(incoming, {1.3f, 0, -.05f}, 0) >= litRight - 2);
        CHECK(frame() == incoming); // Frozen time remains deterministic during priority changes.
        for (int tick = 0; tick < 20; ++tick) {
            scene.time += 1.f / 60;
            frame();
        }
        const auto settled = frame();
        CHECK(sample(settled, {-1.3f, 0, 0}, 0) > 150);
        CHECK(sample(settled, {1.3f, 0, -.05f}, 0) > 150);
        sign.color = {0, 0, 1};
        right.model.values[14] += .3f; // Keep identity while the facade moves, as the game's scrolling scenery does.
        scene.time += 1.f / 60;
        const auto moving = frame();
        CHECK(sample(moving, {1.3f, 0, .25f}, 2) > 150);
        CHECK(sample(moving, {1.3f, 0, .25f}, 0) < 40);
        CHECK(sample(moving, {-1.3f, 0, 0}, 2) >
              150); // Nearby faces outside the priority budget update every frame too.
        right.model.values[0] = 1.1f;
        right.windowMaterial.roughness = 1;
        scene.time += 1.f / 60;
        const auto thin = frame();
        CHECK(sample(thin, {1.3f, 0, .25f}, 2) >
              80); // A smaller crop in a retained buffer keeps its blurred reflection.
        CHECK(sample(thin, {1.3f, 0, .25f}, 0) < 40);
        right.windowMaterial.roughness = 0;
    }
    settings.planarReflectionBlendSeconds = -1;
    checkThrows([&] { SceneRenderer invalid(settings); });
}
void distantRefreshChecks(Size size) {
    RenderSettings settings;
    settings.retroSize = size;
    settings.retroPixelStep = settings.retroAffineStrength = settings.retroDitherStrength = 0;
    settings.shadowMapSize = settings.worldReflectionMapSize = 32;
    settings.planarReflectionSize = 128;
    settings.distantPlanarReflectionSize = 64;
    settings.maxPlanarReflections = settings.maxDistantPlanarUpdatesPerFrame = 1;
    settings.planarReflectionBlendSeconds = 0;
    constexpr float zero[] = {0}, one[] = {1};
    Texture backing = Texture::fromRedChannel({1, 1}, zero);
    Texture glass = Texture::fromRedChannel({1, 1}, one);
    Scene scene;
    scene.camera.position = {0, 0, 8};
    scene.camera.target = {0, 0, -50};
    scene.sky.top = scene.sky.bottom = {0, 0, 0};
    scene.sky.cloudCoverage = scene.sky.sunSize = 0;
    scene.sky.texturedClouds[0].texture = &backing;
    scene.sky.texturedClouds[0].opacity = 0;
    scene.light.ambient = 1;
    std::array<Vec3, 3> centers{};
    for (int index = 0; index < 3; ++index) {
        centers[index] = {float(index - 1) * 16, 0, -48.f - index * 4};
        Quad window;
        window.model = multiply(translation(centers[index]), scale({10, 12, 1}));
        window.worldSize = {10, 12};
        window.normal = {0, 0, 1};
        window.texture = &backing;
        window.materialTextures.glass = &glass;
        window.skyscraperWindows = true;
        window.windowMaterial.roughness = 0;
        window.windowMaterial.reflectance = .8f;
        window.castsShadow = window.receivesShadow = false;
        window.reflectionGroupId = index + 1;
        window.reflectionId = index + 101;
        scene.objects.emplace_back(window);
    }
    Quad backdrop;
    backdrop.model = multiply(translation({0, 0, 12}), scale({300, 200, 1}));
    backdrop.normal = {0, 0, -1};
    backdrop.castsShadow = backdrop.receivesShadow = false;
    scene.objects.emplace_back(backdrop);
    const auto colors = [&](const auto& pixels) {
        std::array<std::array<int, 3>, 3> samples{};
        const auto projection = scene.camera.viewProjection(float(size.width) / size.height);
        const auto& m = projection.values;
        for (std::size_t index = 0; index < centers.size(); ++index) {
            const auto point = centers[index];
            const auto clip = transformPoint(projection, point);
            const float w = m[3] * point.x + m[7] * point.y + m[11] * point.z + m[15];
            const int x = (clip.x / w * .5f + .5f) * size.width;
            const int y = (clip.y / w * .5f + .5f) * size.height;
            CHECK(x >= 0 && x < size.width && y >= 0 && y < size.height);
            const auto offset = (y * size.width + x) * 3;
            samples[index] = {pixels[offset], pixels[offset + 1], pixels[offset + 2]};
        }
        return samples;
    };
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        SceneRenderer renderer(settings);
        renderer.setMode(mode);
        const auto frame = [&] {
            renderer.render(scene, size);
            stateChecks();
            return colors(capture(size));
        };
        scene.time = 1;
        std::get<Quad>(scene.objects.back()).color = {1, 0, 0};
        for (auto color : frame()) {
            CHECK(color[0] > 150 && color[1] < 20); // Every new face captures immediately, beyond the refresh budget.
        }
        scene.time += 1;
        std::get<Quad>(scene.objects.back()).color = {0, 1, 0};
        const auto staggered = frame();
        const auto green = [](auto color) { return color[1] > 150 && color[0] < 20; };
        CHECK(std::count_if(staggered.begin(), staggered.end(), green) == 2);
        CHECK(std::count_if(staggered.begin(), staggered.end(), [](auto color) { return color[0] > 150; }) == 1);
        scene.time += 1.f / 60;
        const auto completed = frame();
        CHECK(std::all_of(completed.begin(), completed.end(), green)); // Deferred views stay visible and catch up.
        scene.time += 1.f / 60;
        scene.camera.position.x = 1;
        std::get<Quad>(scene.objects.back()).color = {0, 0, 1};
        for (auto color : frame()) {
            CHECK(color[2] > 150 && color[1] < 20); // Camera changes refresh all mappings immediately.
        }
        scene.camera.position.x = 0;
        for (int refreshPolicy : {0, 1, 2}) {
            auto immediateSettings = settings;
            if (refreshPolicy == 0) {
                immediateSettings.distantPlanarReflectionUpdateSeconds = 0;
            } else if (refreshPolicy == 1) {
                immediateSettings.maxDistantPlanarUpdatesPerFrame = 0;
            } else {
                immediateSettings.nearPlanarReflectionDistance = 75; // All three panes now belong to the near region.
            }
            SceneRenderer immediate(immediateSettings);
            immediate.setMode(mode);
            scene.time = 3;
            std::get<Quad>(scene.objects.back()).color = {1, 0, 0};
            immediate.render(scene, size);
            scene.time = 4;
            std::get<Quad>(scene.objects.back()).color = {0, 1, 0};
            immediate.render(scene, size);
            stateChecks();
            const auto updated = colors(capture(size));
            CHECK(std::all_of(updated.begin(), updated.end(), green));
        }
    }
    settings.maxDistantPlanarUpdatesPerFrame = -1;
    checkThrows([&] { SceneRenderer invalid(settings); });
    settings.maxDistantPlanarUpdatesPerFrame = 1;
    settings.nearPlanarReflectionDistance = -1;
    checkThrows([&] { settings.validate(); });
    settings.nearPlanarReflectionDistance = settings.distantPlanarReflectionDetailDistance;
    checkThrows([&] { settings.validate(); });
    settings.nearPlanarReflectionDistance = 20;
    settings.distantPlanarReflectionDetailDistance = 75;
    settings.validate();
}
void buildingCoverageChecks(Size size) {
    RenderSettings settings;
    settings.retroSize = size;
    settings.retroPixelStep = settings.retroAffineStrength = settings.retroDitherStrength = 0;
    settings.shadowMapSize = settings.worldReflectionMapSize = 32;
    settings.planarReflectionSize = 128;
    settings.distantPlanarReflectionSize = 64;
    settings.maxPlanarReflections = 1;
    constexpr float zero[] = {0}, one[] = {1};
    Texture backing = Texture::fromRedChannel({1, 1}, zero);
    Texture mask = Texture::fromRedChannel({1, 1}, one);
    Quad front;
    front.texture = &backing;
    front.materialTextures.glass = &mask;
    front.skyscraperWindows = true;
    front.normal = {0, 0, 1};
    front.windowMaterial.roughness = 0;
    front.windowMaterial.reflectance = .35f;
    front.castsShadow = front.receivesShadow = false;
    front.reflectionId = 101;
    front.reflectionGroupId = 10;
    Quad side = front;
    side.normal = {1, 0, 0};
    side.reflectionId = 102;
    Quad backdrop;
    backdrop.normal = {0, 0, -1};
    backdrop.color = {1, 0, 0};
    backdrop.castsShadow = backdrop.receivesShadow = false;
    Quad sideBackdrop = backdrop;
    sideBackdrop.normal = {-1, 0, 0};
    Scene scene;
    scene.light.ambient = 1;
    scene.sky.top = scene.sky.bottom = {0, 0, 0};
    scene.sky.cloudCoverage = scene.sky.sunSize = 0;
    scene.sky.texturedClouds[0].texture = &backing;
    scene.sky.texturedClouds[0].opacity = 0;
    const auto sample = [&](const auto& pixels, Vec3 point) {
        const auto projection = scene.camera.viewProjection(float(size.width) / size.height);
        const auto clip = transformPoint(projection, point);
        const auto& m = projection.values;
        const float w = m[3] * point.x + m[7] * point.y + m[11] * point.z + m[15];
        const int x = (clip.x / w * .5f + .5f) * size.width;
        const int y = (clip.y / w * .5f + .5f) * size.height;
        CHECK(x >= 0 && x < size.width && y >= 0 && y < size.height);
        return int(pixels[(y * size.width + x) * 3]);
    };
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        SceneRenderer renderer(settings);
        renderer.setMode(mode);
        scene.time = 1;
        scene.camera.position = {6, 0, 7};
        scene.camera.target = {0, 0, -2};
        front.model = scale({4, 6, 1});
        side.model = multiply(translation({2, 0, -2}), multiply(rotateY(kPi * .5f), scale({4, 6, 1})));
        backdrop.model = multiply(translation({0, 0, 12}), scale({100, 100, 1}));
        sideBackdrop.model = multiply(translation({12, 0, 0}), multiply(rotateY(kPi * .5f), scale({100, 100, 1})));
        const auto frame = [&](bool both = true) {
            scene.objects = {front, backdrop, sideBackdrop};
            if (both)
                scene.objects.push_back(side);
            renderer.render(scene, size);
            stateChecks();
            return capture(size);
        };
        const int frontFull = sample(frame(false), {});
        scene.time += 1.f / 60;
        const int sideFull = sample(frame(), {2, 0, -2});
        CHECK(frontFull > 150 &&
              sideFull > 150); // A newly visible perpendicular face inherits its building's activation.
        scene.objects.clear();
        scene.time += 1;
        renderer.render(scene, size);
        scene.time += 1.f / 60;
        const auto arriving = frame();
        const int frontFade = sample(arriving, {}), sideFade = sample(arriving, {2, 0, -2});
        CHECK(frontFade > 30 && frontFade < frontFull - 20);
        CHECK(sideFade > 30 && sideFade < sideFull - 20);
        CHECK(std::abs(float(frontFade) / frontFull - float(sideFade) / sideFull) < .04f);
        CHECK(frame() == arriving);
        // Reflect both sides well before the previous 90-unit cutoff, even
        // though only one plane can receive the high-detail budget.
        scene.time = 0;
        scene.camera.position = {70, 0, 95};
        scene.camera.target = {0, 0, -20};
        scene.camera.farPlane = 1000;
        front.model = scale({40, 40, 1});
        side.model = multiply(translation({20, 0, -20}), multiply(rotateY(kPi * .5f), scale({40, 40, 1})));
        backdrop.model = multiply(translation({0, 0, 200}), scale({1000, 1000, 1}));
        sideBackdrop.model = multiply(translation({200, 0, 0}), multiply(rotateY(kPi * .5f), scale({1000, 1000, 1})));
        const auto distant = frame();
        CHECK(sample(distant, {}) > 80 && sample(distant, {20, 0, -20}) > 80);

        // A building-shaped group with a passage through it must not hide
        // glass visible through the gap, even if some panels overlap.
        scene.camera.position = {0, 0, 5};
        scene.camera.target = {0, 0, -12};
        front.model = multiply(translation({.8f, 0, -12}), scale({1.4f, 4, 1}));
        backdrop.model = multiply(translation({0, 0, 12}), scale({100, 100, 1}));
        scene.objects = {front, backdrop};
        Quad wall = front;
        wall.skyscraperWindows = false;
        wall.buildingSurface = true;
        wall.materialTextures = {};
        wall.surfaceMaterial.reflectivity = 0;
        wall.reflectionId = 0;
        wall.reflectionGroupId = 20;
        for (float z : {-4.f, -6.f}) {
            wall.normal = {0, 0, z == -4 ? 1.f : -1.f};
            wall.model = multiply(translation({-1, 0, z}), scale({2, 4, 1}));
            scene.objects.push_back(wall);
            scene.objects.push_back(wall);
        }
        for (float x : {-2.f, 2.f}) {
            wall.normal = {x < 0 ? -1.f : 1.f, 0, 0};
            wall.model = multiply(translation({x, 0, -5}), multiply(rotateY(kPi * .5f), scale({2, 4, 1})));
            scene.objects.push_back(wall);
        }
        wall.normal = {0, 1, 0};
        wall.model = multiply(translation({0, 2, -5}), multiply(rotateX(kPi * .5f), scale({4, 2, 1})));
        scene.objects.push_back(wall);
        renderer.render(scene, size);
        stateChecks();
        CHECK(sample(capture(size), {.8f, 0, -12}) > 150);
    }
    settings.distantPlanarReflectionSize = 0;
    checkThrows([&] { SceneRenderer invalid(settings); });
    settings.distantPlanarReflectionSize = 64;
    settings.distantPlanarReflectionUpdateSeconds = -1;
    checkThrows([&] { SceneRenderer invalid(settings); });
}
} // namespace
int main() {
    try {
        engine::platform::Window window({"World reflection tests", {160, 160}, false, false});
        resourceChecks();
        worldChecks(window.drawableSize());
        planarChecks(window.drawableSize());
        distanceFadeChecks(window.drawableSize());
        coverageChecks(window.drawableSize());
        distantRefreshChecks(window.drawableSize());
        buildingCoverageChecks(window.drawableSize());
        std::cout << "Reflection tests passed: moving geometry, planar projection, glass blur, state and lifetime\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
