#include "ShaderSources.h"
#include "TestSupport.h"
#include "engine/graphics/Mesh.h"
#include "engine/graphics/Model.h"
#include "engine/graphics/RenderTarget.h"
#include "engine/graphics/SceneRenderer.h"
#include "engine/graphics/ShaderProgram.h"
#include "engine/platform/Window.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <glad/glad.h>
#include <iostream>
#include <limits>
#include <vector>

using namespace engine::graphics;
using namespace engine::math;
namespace {
constexpr auto vertex = R"(#version 330 core
layout(location = 0) in vec3 position;
void main() { gl_Position = vec4(position.xy * 2.0, 0.0, 1.0); }
)";
constexpr auto fragment = R"(#version 330 core
out vec4 FragColor;
uniform vec4 uFinish;
uniform float uNoV;
uniform int uCheck;
void main() {
    MaterialBrdf material = prepareBrdf(vec3(1.0), uFinish.x, uFinish.y, uFinish.z, uFinish.w, uNoV, 0.0);
    if (uCheck == 1) {
        FragColor = vec4(material.environmentWeight, 1.0);
    } else if (uCheck == 2) {
        FragColor = vec4(specularOcclusion(uNoV, 1.0, uFinish.x),
                          specularOcclusion(uNoV, 0.4, uFinish.x),
                          specularOcclusion(uNoV, 0.0, uFinish.x), 1.0);
    } else {
        // Cosine-weighted hemisphere quadrature. Averaging PI * BRDF yields
        // reflected energy under a unit-radiance environment, without tonemapping.
        float noL = sqrt(gl_FragCoord.y / 128.0);
        float phi = gl_FragCoord.x / 512.0 * 2.0 * PI;
        vec3 light = vec3(sqrt(1.0 - noL * noL) * vec2(cos(phi), sin(phi)), noL);
        vec3 view = vec3(sqrt(1.0 - uNoV * uNoV), 0.0, uNoV);
        vec3 halfway = normalize(light + view);
        vec3 response = evaluateBrdf(material, noL, halfway.z, dot(view, halfway), material.alphaSquared);
        FragColor = vec4(response * PI, 1.0);
    }
}
)";

void numericalChecks() {
    constexpr Size size{512, 128};
    RenderTarget output(size, RenderTarget::Kind::ColorDepth);
    // Floating-point storage exposes excessive energy instead of clipping it.
    glBindTexture(GL_TEXTURE_2D, output.texture());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, size.width, size.height, 0, GL_RGB, GL_FLOAT, nullptr);
    output.bind();
    CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    ShaderProgram program(vertex, fragment, "BRDF integration", shaders::brdf_glsl);
    const auto mesh = makeQuadMesh();
    std::vector<float> pixels(size.width * size.height * 3);
    const auto render = [&](float roughness, float metallic, float reflectivity, float noV, int checkKind) {
        program.bind();
        glUniform4f(program.uniform("uFinish"), roughness, metallic, reflectivity, .04f);
        glUniform1f(program.uniform("uNoV"), noV);
        glUniform1i(program.uniform("uCheck"), checkKind);
        mesh.draw();
        glReadPixels(0, 0, size.width, size.height, GL_RGB, GL_FLOAT, pixels.data());
        CHECK(glGetError() == GL_NO_ERROR);
        for (float value : pixels) {
            CHECK(std::isfinite(value) && value >= 0);
        }
        double sum = 0;
        for (std::size_t i = 0; i < pixels.size(); i += 3) {
            sum += pixels[i];
        }
        return sum / (size.width * size.height);
    };
    for (float roughness : {.35f, .5f, .75f, 1.0f}) {
        for (float noV : {.1f, .3f, .5f, 1.0f}) {
            const double metalEnergy = render(roughness, 1, 1, noV, 0);
            CHECK(metalEnergy > .8 && metalEnergy < 1.07); // Analytic compensation, including grazing rough metals.
            CHECK(near(render(roughness, 1, 1, noV, 1), 1, .0001)); // White-furnace environment test.
            CHECK(render(roughness, 1, 0, noV, 0) == 0); // Zero reflectivity disables direct and indirect specular.
            CHECK(render(roughness, 1, 0, noV, 1) == 0);
            const double diffuseEnergy = render(roughness, 0, 0, noV, 0);
            CHECK(diffuseEnergy > .45 && diffuseEnergy < 1.02); // Rough diffuse does not create light.
            render(roughness, 0, 1, noV, 2);
            CHECK(near(pixels[0], 1, .0001));
            CHECK(pixels[1] > 0 && pixels[1] < 1 && pixels[2] == 0);
        }
    }
    // Smooth reflections retain more light at normal incidence than grazing;
    // fully rough reflections converge to diffuse AO.
    render(.08f, 0, 1, 1, 2);
    const float smoothFront = pixels[1];
    render(.08f, 0, 1, .1f, 2);
    CHECK(smoothFront > pixels[1] + .1f);
    render(1, 0, 1, .1f, 2);
    CHECK(near(pixels[1], .4f, .001f));
    RenderTarget::bindDefault({160, 160});
}

void reflectanceBindings() {
    RenderSettings settings;
    settings.worldReflectionMapSize = 0;
    settings.shadowMapSize = 32;
    SceneRenderer renderer(settings);
    Scene scene;
    scene.camera.position = {0, 0, 4};
    scene.camera.target = {};
    scene.light.ambient = 1.1f;
    scene.sky.top = scene.sky.bottom = {1, 1, 1};
    scene.sky.sunSize = scene.sky.cloudCoverage = 0;
    Quad surface;
    surface.model = scale({4, 4, 1});
    surface.normal = {0, 0, 1};
    surface.color = {.5f, 0, 0};
    surface.pbrSurface = true;
    surface.castsShadow = false;
    Cube cube{scale({4, 4, 1}), surface.color, {}, true};
    engine::assets::PrimitiveData primitive;
    primitive.vertices = {-2, -2, 0, 0, 0, 1, 0, 0, 2,  -2, 0, 0, 0, 1, 1, 0,
                          2,  2,  0, 0, 0, 1, 1, 1, -2, 2,  0, 0, 0, 1, 0, 1};
    primitive.indices = {0, 1, 2, 0, 2, 3};
    primitive.material.baseColor = {.214f, 0, 0, 1};
    engine::assets::ModelData data;
    data.primitives.push_back(primitive);
    Model model(data);
    ModelInstance prop{identity(), &model, true};
    const auto sample = [&] {
        renderer.render(scene, {160, 160});
        std::array<unsigned char, 3> pixel{};
        glReadPixels(80, 80, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel.data());
        CHECK(glGetError() == GL_NO_ERROR);
        return pixel[1]; // Reflected white light, independent of red diffuse pigment.
    };
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        renderer.setMode(mode);
        for (auto object : {DrawCommand(surface), DrawCommand(cube), DrawCommand(prop)}) {
            scene.objects = {object};
            const auto setFinish = [&](float reflectance, float reflectivity = 1) {
                SurfaceMaterial finish{.5f, 0, reflectivity, reflectance};
                std::visit(
                    [&](auto& item) {
                        if constexpr (requires { item.surfaceMaterial; }) {
                            item.surfaceMaterial = finish;
                        } else if constexpr (requires { item.materialOverride; }) {
                            item.materialOverride = finish;
                        }
                    },
                    scene.objects.front());
            };
            setFinish(.04f);
            const auto ordinary = sample();
            setFinish(.2f);
            CHECK(sample() > ordinary + 20);
            setFinish(std::numeric_limits<float>::quiet_NaN());
            CHECK(sample() == ordinary);
            setFinish(1, 0);
            CHECK(sample() == 0);
        }
    }
}
} // namespace

int main() {
    try {
        engine::platform::Window window({"BRDF tests", {160, 160}, false, false});
        numericalChecks();
        reflectanceBindings();
        std::cout << "BRDF tests passed: HDR energy, grazing stability, occlusion and material bindings\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
