#include "ShaderSources.h"
#include "TestSupport.h"
#include "engine/graphics/Mesh.h"
#include "engine/graphics/RenderTarget.h"
#include "engine/graphics/SceneRenderer.h"
#include "engine/graphics/ShaderProgram.h"
#include "engine/platform/Window.h"
#include <algorithm>
#include <array>
#include <glad/glad.h>
#include <iostream>
#include <limits>
#include <vector>

using namespace engine::graphics;
using namespace engine::math;
namespace {
constexpr Size size = {128, 128};
std::array<float, 3> pixel(int x = 64, int y = 64) {
    std::array<float, 3> rgb{};
    glReadPixels(x, y, 1, 1, GL_RGB, GL_FLOAT, rgb.data());
    return rgb;
}
std::vector<unsigned char> frame() {
    std::vector<unsigned char> pixels(size.width * size.height * 3);
    glReadPixels(0, 0, size.width, size.height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    return pixels;
}
float rgb555(float channel) {
    return std::floor(std::floor(channel * 255 + .5f) / 8) / 31;
}
void checkColor(const std::array<float, 3>& actual, Color expected) {
    CHECK(near(actual[0], rgb555(expected.r), .005f));
    CHECK(near(actual[1], rgb555(expected.g), .005f));
    CHECK(near(actual[2], rgb555(expected.b), .005f));
}
void distanceChecks() {
    RenderSettings settings;
    settings.retroSize = size;
    settings.shadowMapSize = 64;
    settings.retroDitherStrength = 0;
    settings.psxDistanceFog = true;
    settings.psxFogStart = 4;
    settings.psxFogEnd = 16;
    settings.psxFogFollowsSky = false; // Explicit palette overrides still work for renderer clients.
    SceneRenderer renderer(settings);
    CHECK(renderer.psxDistanceFog());
    Scene scene;
    scene.camera.position = {0, 0, 0};
    scene.camera.target = {0, 0, -1};
    scene.sky.cloudCoverage = scene.sky.sunSize = 0;
    Quad surface;
    surface.color = {.8f, .2f, .1f};
    surface.receivesShadow = surface.castsShadow = false;
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        renderer.setMode(mode);
        for (float depth : {2.0f, 4.0f, 8.0f, 12.0f, 16.0f, 22.0f}) {
            surface.model = multiply(translation({0, 0, -depth}), scale({depth * 2, depth * 2, 1}));
            scene.objects = {surface};
            renderer.setPsxDistanceFog(false);
            renderer.render(scene, size);
            const auto original = pixel();
            renderer.setPsxDistanceFog(true);
            renderer.render(scene, size);
            const auto actual = pixel();
            if (mode == RenderMode::Normal || depth <= settings.psxFogStart) {
                CHECK(actual == original);
            } else {
                const float cue = std::min((depth - 4) / 12, 1.0f);
                const float fog = cue;
                const Color tint = settings.psxFogColor;
                checkColor(actual, {surface.color.r * (1 - fog) + tint.r * fog,
                                    surface.color.g * (1 - fog) + tint.g * fog,
                                    surface.color.b * (1 - fog) + tint.b * fog});
            }
        }
    }
    // A stored preference survives style changes; disabling restores the same raster.
    renderer.setMode(RenderMode::Psx);
    renderer.setPsxDistanceFog(false);
    renderer.render(scene, size);
    const auto clear = frame();
    renderer.setPsxDistanceFog(true);
    renderer.render(scene, size);
    const auto fogged = frame();
    CHECK(clear != fogged);
    renderer.setRetroMode(false);
    CHECK(renderer.psxDistanceFog());
    renderer.render(scene, size);
    renderer.setRetroMode(true);
    renderer.render(scene, size);
    CHECK(frame() == fogged);
    renderer.setPsxDistanceFog(false);
    renderer.render(scene, size);
    CHECK(frame() == clear);
}
void skyChecks() {
    RenderSettings settings;
    settings.retroSize = size;
    settings.shadowMapSize = 64;
    settings.retroDitherStrength = 0;
    settings.psxDistanceFog = true;
    // Sky matching overrides custom tints without recoloring the actual sky.
    settings.psxFogColor = {.9f, .1f, .2f};
    settings.psxNightFogColor = {.02f, .04f, .09f};
    SceneRenderer renderer(settings);
    renderer.setMode(RenderMode::Psx);
    Scene scene;
    scene.camera.position = {0, 0, 0};
    scene.sky.cloudCoverage = scene.sky.sunSize = 0;
    for (float night : {0.0f, .5f, 1.0f}) {
        scene.sky.nightBlend = night;
        scene.sky.bottom = {.64f, .82f, .96f}; // A nondefault scene palette must still match its own horizon.
        const Color day = scene.sky.bottom, dark = {.38f, .31f, .40f};
        const Color tint = {day.r * (1 - night) + dark.r * night,
                            day.g * (1 - night) + dark.g * night,
                            day.b * (1 - night) + dark.b * night};
        // Distance cueing meets the regular horizon in every viewing direction.
        for (Vec3 bearing : {Vec3{0, 0, -1}, Vec3{1, 0, 0}, Vec3{0, 0, 1}, Vec3{-1, 0, 0}}) {
            scene.camera.target = bearing;
            renderer.render(scene, size);
            const auto sky = pixel(64, 63); // Just below the horizon, outside its ordinary vertical gradient.
            checkColor(sky, tint);
            Quad distant;
            const float depth = settings.psxFogEnd + 10;
            const Vec3 position = {bearing.x * depth, bearing.y * depth, bearing.z * depth};
            distant.model = multiply(translation(position), scale({400, 400, 1}));
            if (bearing.x != 0) {
                distant.model = multiply(translation(position),
                                          multiply(rotateY(kPi * .5f), scale({400, 400, 1})));
            }
            distant.receivesShadow = distant.castsShadow = false;
            scene.objects = {distant};
            renderer.render(scene, size);
            CHECK(pixel(64, 63) == sky); // Full-fog surfaces and sky share exactly the same horizon color.
            scene.objects.clear();
        }
    }
    scene.sky.cloudCoverage = .4f;
    scene.sky.sunSize = 5.5f;
    scene.camera.target = normalize(scene.light.direction);
    for (float night : {0.0f, .5f, 1.0f}) {
        scene.sky.nightBlend = night;
        renderer.setPsxDistanceFog(false);
        renderer.render(scene, size);
        const auto clear = frame();
        renderer.setPsxDistanceFog(true);
        renderer.render(scene, size);
        CHECK(frame() == clear); // Clouds, blue sky, sun, moon and stars retain their regular colors and contrast.
    }
    settings.psxFogFollowsSky = false;
    SceneRenderer customFog(settings);
    customFog.setMode(RenderMode::Psx);
    customFog.setPsxDistanceFog(false);
    customFog.render(scene, size);
    const auto clear = frame();
    customFog.setPsxDistanceFog(true);
    customFog.render(scene, size);
    CHECK(frame() == clear); // Even an explicit custom fog tint affects only distant surfaces.
}
void materialEnvironmentChecks() {
    RenderSettings settings;
    settings.retroSize = size;
    settings.shadowMapSize = 64;
    settings.psxFogStart = 4;
    settings.psxFogEnd = 16;
    SceneRenderer renderer(settings);
    renderer.setMode(RenderMode::Psx);
    Scene scene;
    scene.camera.position = {0, 0, 0};
    scene.camera.target = {0, 0, -1};
    scene.sky.top = {.2f, .6f, 1};
    scene.sky.bottom = {.74f, .86f, .97f};
    scene.light.ambient = 1.1f; // Isolate indirect material reflections from the sun.
    Quad near;
    near.model = multiply(translation({0, 0, -2}), scale({4, 4, 1}));
    near.normal = {0, 0, 1};
    near.color = {.12f, .45f, .9f};
    near.pbrSurface = true;
    near.receivesShadow = near.castsShadow = false;
    for (float night : {0.0f, .5f, 1.0f}) {
        scene.sky.nightBlend = night;
        for (float metallic : {0.0f, 1.0f}) {
            near.surfaceMaterial = {.24f, metallic, 1};
            scene.objects = {near};
            renderer.setPsxDistanceFog(false);
            renderer.render(scene, size);
            const auto original = frame();
            renderer.setPsxDistanceFog(true);
            renderer.render(scene, size);
            CHECK(frame() == original); // Nearby glossy and metallic finishes retain their colorful sky environment.
        }
    }
}
void vertexChecks() {
    RenderTarget output(size, RenderTarget::Kind::ColorDepth);
    // Unequal W values detect accidental perspective interpolation of vertex color cues.
    constexpr std::array<float, 9> vertices = {-.75f, -.75f, 1, 3, -3, 4, -6, 6, 8};
    Mesh triangle(vertices, 3, {{0, 3, 0}});
    Mat4 projection{};
    projection.values[0] = projection.values[5] = projection.values[11] = 1;
    ShaderProgram program(shaders::plain_vert, R"(#version 330 core
out vec4 FragColor;
void main() { FragColor = vec4(vec3(distanceFogAmount()), 1.0); }
)", "PSX vertex fog regression", shaders::fog_glsl, shaders::vertex_snap_glsl);
    output.bind();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    program.bind();
    glUniformMatrix4fv(program.uniform("uMvp"), 1, GL_FALSE, projection.values.data());
    glUniform3f(program.uniform("uSnap"), size.width, size.height, 0);
    glUniform2f(program.uniform("uFogRange"), 2, 8);
    triangle.draw();
    std::array<float, 3> shade{};
    glReadPixels(40, 40, 1, 1, GL_RGB, GL_FLOAT, shade.data());
    const float cue = (40.5f - 16) / 96 * (85.0f / 255 + 1);
    CHECK(near(shade[0], cue, .005f));
    glUniform2f(program.uniform("uFogRange"), 0, 0);
    triangle.draw();
    glReadPixels(40, 40, 1, 1, GL_RGB, GL_FLOAT, shade.data());
    CHECK(shade[0] == 0);
}
void stabilityChecks() {
    RenderSettings settings;
    settings.retroSize = size;
    settings.shadowMapSize = 64;
    settings.psxDistanceFog = true;
    settings.psxFogStart = 4;
    settings.psxFogEnd = 16;
    SceneRenderer renderer(settings);
    renderer.setRetroMode(true);
    Scene scene;
    scene.camera.position = {0, 0, 0};
    scene.camera.target = {0, 0, -1};
    scene.sky.cloudCoverage = scene.sky.sunSize = 0;
    Quad ramp;
    ramp.model = multiply(translation({0, 0, -10}), multiply(rotateY(.7f), scale({16, 16, 1})));
    ramp.receivesShadow = ramp.castsShadow = false;
    ramp.color = {.2f, .1f, .05f};
    scene.objects = {ramp};
    renderer.render(scene, size);
    const auto first = frame();
    scene.time = 37;
    renderer.render(scene, size);
    CHECK(frame() == first); // Fog and its ordered RGB555 blending have no animated/random phase.
    renderer.setRetroMode(false);
    renderer.render(scene, size);
    renderer.setRetroMode(true);
    renderer.render(scene, size);
    CHECK(frame() == first);
    settings.retroDitherStrength = 0;
    SceneRenderer undithered(settings);
    undithered.setRetroMode(true);
    undithered.render(scene, size);
    CHECK(frame() != first); // Smooth fog gradients participate in the existing selective shade dithering.
}
void settingsChecks() {
    for (float invalid : {-1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        RenderSettings settings;
        settings.psxFogStart = invalid;
        checkThrows([&] { SceneRenderer renderer(settings); });
        settings = {};
        settings.psxFogEnd = invalid;
        checkThrows([&] { SceneRenderer renderer(settings); });
    }
    RenderSettings settings;
    settings.psxFogEnd = settings.psxFogStart;
    checkThrows([&] { SceneRenderer renderer(settings); });
    for (float invalid : {-.1f, 1.1f, std::numeric_limits<float>::quiet_NaN()}) {
        settings = {};
        settings.psxFogColor.r = invalid;
        checkThrows([&] { SceneRenderer renderer(settings); });
        settings = {};
        settings.psxNightFogColor.b = invalid;
        checkThrows([&] { SceneRenderer renderer(settings); });
    }
    settings = {};
    settings.psxFogStart = 0; // Optional immediate depth cueing is valid.
    SceneRenderer renderer(settings);
    CHECK(!renderer.psxDistanceFog());
}
} // namespace
int main() {
    try {
        engine::platform::Window window({"PSX fog tests", size, false, false});
        glDisable(GL_DITHER);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        distanceChecks();
        skyChecks();
        materialEnvironmentChecks();
        vertexChecks();
        stabilityChecks();
        settingsChecks();
        CHECK(glGetError() == GL_NO_ERROR);
        std::cout << "Fog tests passed: linear distance cueing, sky matching, sky/material isolation and stable dithering\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
