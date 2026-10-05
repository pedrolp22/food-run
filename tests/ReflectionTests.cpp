#include "TestSupport.h"
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
    for (int unit = 0; unit <= 8; ++unit) {
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
} // namespace
int main() {
    try {
        engine::platform::Window window({"World reflection tests", {160, 160}, false, false});
        resourceChecks();
        worldChecks(window.drawableSize());
        planarChecks(window.drawableSize());
        distanceFadeChecks(window.drawableSize());
        std::cout << "Reflection tests passed: moving geometry, planar projection, glass blur, state and lifetime\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
