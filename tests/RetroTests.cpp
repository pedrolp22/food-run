#include "ShaderSources.h"
#include "TestSupport.h"
#include "engine/graphics/Mesh.h"
#include "engine/graphics/RenderTarget.h"
#include "engine/graphics/SceneRenderer.h"
#include "engine/graphics/ShaderProgram.h"
#include "engine/graphics/TextureSampler.h"
#include "engine/platform/Window.h"
#include <array>
#include <algorithm>
#include <glad/glad.h>
#include <iostream>
#include <limits>
#include <vector>

using namespace engine::graphics;
using namespace engine::math;
namespace {
constexpr std::string_view unlit = R"(
float shadowedLight(float light, float ambient) { return light; }
float sunVisibility(float diffuse) { return 1.0; }
float ditheredShadowAmbient(float visibility, float diffuse) { return 1.0; }
float ambientOcclusionVisibility(float visibility, float diffuse) { return 1.0; }
vec3 distanceFog(vec3 color) { return color; }
)";
constexpr std::string_view whiteFragment = R"(#version 330 core
out vec4 FragColor;
void main() { FragColor = vec4(1.0); }
)";
void matrix(const ShaderProgram& program, const char* name, const Mat4& value) {
    // Some production uniforms are unused by the diagnostic fragment shader.
    glUniformMatrix4fv(glGetUniformLocation(program.id(), name), 1, GL_FALSE, value.values.data());
}
std::array<float, 3> pixel(int x, int y) {
    std::array<float, 3> result{};
    glReadPixels(x, y, 1, 1, GL_RGB, GL_FLOAT, result.data());
    return result;
}
void affineChecks() {
    RenderTarget output({128, 128}, RenderTarget::Kind::ColorDepth);
    RenderTarget gradient({256, 256}, RenderTarget::Kind::ColorDepth);
    std::vector<unsigned char> texels(256 * 256 * 3);
    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            texels[(y * 256 + x) * 3] = x;
            texels[(y * 256 + x) * 3 + 1] = y;
        }
    }
    glBindTexture(GL_TEXTURE_2D, gradient.texture());
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGB, GL_UNSIGNED_BYTE, texels.data());
    output.bind();
    // These project to the same right triangle, but have clip W values of 1, 4, 2.
    constexpr std::array<float, 24> vertices = {-0.75f, -0.75f, 1, 0, 1,     0,    0, 0, 3, -3, 4, 0,
                                                1,      0,      1, 0, -1.5f, 1.5f, 2, 0, 1, 0,  0, 1};
    Mat4 projection{};
    projection.values[0] = projection.values[5] = projection.values[11] = 1;
    const std::array sources = {std::pair{shaders::road_vert, shaders::road_frag},
                                std::pair{shaders::model_vert, shaders::model_frag},
                                std::pair{shaders::sprite_vert, shaders::sprite_frag}};
    for (int kind = 0; kind < 3; ++kind) {
        ShaderProgram program(sources[kind].first, sources[kind].second, "affine regression",
                              std::string(unlit) + std::string(shaders::texture_filter_glsl),
                              shaders::vertex_snap_glsl);
        Mesh mesh(vertices, 8,
                  kind == 1 ? std::initializer_list<VertexAttribute>{{0, 3, 0}, {1, 3, 3}, {2, 2, 6}}
                            : std::initializer_list<VertexAttribute>{{0, 3, 0}, {1, 2, 6}});
        program.bind();
        matrix(program, "uMvp", projection);
        matrix(program, "uModel", identity());
        glUniform3f(program.uniform("uSnap"), 128, 128, 0);
        glUniform1i(program.uniform("uTexture"), 0);
        if (kind == 0) {
            glUniform2f(program.uniform("uTexScale"), 1, 1);
            glUniform2f(program.uniform("uTexOffset"), 0, 0);
            glUniform3f(program.uniform("uColor"), 1, 1, 1);
            glUniform1f(program.uniform("uAlpha"), 1);
            glUniform1f(program.uniform("uLight"), 1);
        } else if (kind == 1) {
            glUniform3f(program.uniform("uBaseColor"), 1, 1, 1);
            glUniform3f(program.uniform("uSunDir"), 0, 1, 0);
            glUniform1f(program.uniform("uHasTexture"), 1);
        } else {
            glUniform1f(program.uniform("uAspect"), 1);
        }
        for (float strength : {0.0f, 0.5f, 1.0f}) {
            glUniform1f(program.uniform("uAffine"), strength);
            glClear(GL_COLOR_BUFFER_BIT);
            mesh.draw();
            for (int y : {28, 40, 52}) {
                for (int x : {28, 40, 52}) {
                    const float b = (x + 0.5f - 16) / 96;
                    const float c = (y + 0.5f - 16) / 96;
                    const float divisor = 1 - b - c + b / 4 + c / 2;
                    const float u = (1 - strength) * (b / 4 / divisor) + strength * b;
                    const float v = (1 - strength) * (c / 2 / divisor) + strength * c;
                    const float gain = kind == 2 ? 1.35f : 1;
                    const auto actual = pixel(x, y);
                    CHECK(near(actual[0], gain * u, 0.012f));
                    CHECK(near(actual[1], gain * v, 0.012f));
                }
            }
        }
    }
}
void snapChecks() {
    const std::array sources = {shaders::road_vert,  shaders::model_vert, shaders::sprite_vert,
                                shaders::plain_vert, shaders::cube_vert,  shaders::sun_vert};
    GLuint feedback = 0;
    glGenBuffers(1, &feedback);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, feedback);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 12 * sizeof(float), nullptr, GL_STREAM_READ);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, feedback);
    glEnable(GL_RASTERIZER_DISCARD);
    for (auto source : sources) {
        ShaderProgram program(source, whiteFragment, "snap regression", {}, shaders::vertex_snap_glsl);
        const char* varying = "gl_Position";
        glTransformFeedbackVaryings(program.id(), 1, &varying, GL_INTERLEAVED_ATTRIBS);
        glLinkProgram(program.id());
        GLint linked = 0;
        glGetProgramiv(program.id(), GL_LINK_STATUS, &linked);
        CHECK(linked);
        program.bind();
        auto project = [&](float pixelX, float step, float z = 0.2f, float w = 1.0f, float strength = 1.0f) {
            // An odd target size catches grids accidentally anchored at the NDC center.
            const float x = (pixelX / 241 * 2 - 1) * w;
            const float y = (45.3f / 319 * 2 - 1) * w;
            const std::array vertices = {x, y, z, x, y, z, x, y, z};
            Mesh mesh(vertices, 3, {{0, 3, 0}});
            Mat4 mvp = identity();
            mvp.values[15] = w;
            matrix(program, "uMvp", mvp);
            glUniform3f(program.uniform("uSnap"), 241, 319, step);
            glUniform1f(program.uniform("uSnapStrength"), strength);
            glBeginTransformFeedback(GL_TRIANGLES);
            mesh.draw();
            glEndTransformFeedback();
            std::array<float, 12> result{};
            glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(result), result.data());
            for (int i = 0; i < 12; i += 4) {
                CHECK(std::isfinite(result[i]) && std::isfinite(result[i + 1]));
                CHECK(result[i + 2] == z && result[i + 3] == w);
                if (step == 0 || strength == 0 || w <= 0.00001f || z < -w) {
                    CHECK(result[i] == x && result[i + 1] == y);
                } else {
                    const float pixelY = (result[i + 1] / w * 0.5f + 0.5f) * 319;
                    CHECK(std::abs(pixelY - 45.3f) <= step * .5f * strength + .0001f);
                    if (strength == 1) {
                        CHECK(near(pixelY / step, std::round(pixelY / step), 0.0001f));
                    }
                }
            }
            return w == 0 ? 0 : (result[0] / w * 0.5f + 0.5f) * 241;
        };
        CHECK(near(project(10.1f, 1), 10, 0.0001f));
        CHECK(near(project(10.4f, 1), 10, 0.0001f));
        CHECK(near(project(10.6f, 1), 11, 0.0001f));
        CHECK(near(project(11.3f, 2), 12, 0.0001f));
        CHECK(near(project(10.3f, 0), 10.3f, 0.0001f));
        project(10.3f, 1, -2);    // Across the near plane.
        project(10.3f, 1, 0, 0);  // At the eye.
        project(10.3f, 1, 0, -1); // Behind the eye.
        project(10.3f, 1, 0, 0.000001f);
        CHECK(near(project(10.3f, 1, .2f, 1, 0), 10.3f, .0001f));
        for (int frame = 0; frame < 20; ++frame) {
            const float original = 10 + frame * .053f;
            const float shifted = project(original, 1, .2f, 1, .45f);
            CHECK(std::abs(shifted - original) <= .2251f); // Tuned wobble stays below a quarter pixel.
        }
        CHECK(project(10.501f, 1, .2f, 1, .45f) - project(10.499f, 1, .2f, 1, .45f) < .46f);
    }
    glDisable(GL_RASTERIZER_DISCARD);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
    glDeleteBuffers(1, &feedback);
}
void samplerChecks() {
    RenderTarget output({8, 8}, RenderTarget::Kind::ColorDepth);
    RenderTarget source({2, 1}, RenderTarget::Kind::ColorDepth);
    glBindTexture(GL_TEXTURE_2D, source.texture());
    constexpr unsigned char texels[] = {255, 0, 0, 0, 255, 0};
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_RGB, GL_UNSIGNED_BYTE, texels);
    constexpr unsigned char mip[] = {0, 0, 255};
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, mip);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ShaderProgram program(shaders::retro_vert, R"(#version 330 core
        uniform sampler2D uTexture;
        uniform vec2 uUv;
        uniform float uLod;
        out vec4 FragColor;
        void main() { FragColor = textureLod(uTexture, uUv, uLod); }
    )",
                          "sampling regression");
    program.bind();
    glUniform2f(program.uniform("uScale"), 2, 2);
    glUniform1i(program.uniform("uTexture"), 0);
    Mesh quad = makeQuadMesh();
    output.bind();
    TextureSampler clamp;
    TextureSampler repeat(true);
    auto sample = [&](unsigned int sampler, float u, float lod) {
        glBindSampler(0, sampler);
        glUniform2f(program.uniform("uUv"), u, 0.5f);
        glUniform1f(program.uniform("uLod"), lod);
        quad.draw();
        return pixel(4, 4);
    };
    const auto linear = sample(0, 0.5f, 0);
    CHECK(near(linear[0], 0.5f, 0.01f) && near(linear[1], 0.5f, 0.01f));
    CHECK(sample(0, 0.5f, 4)[2] == 1);
    CHECK(sample(clamp.id(), 0.51f, 0)[1] == 1);
    CHECK(sample(clamp.id(), 0.51f, 4)[1] == 1); // Base texels even under minification.
    CHECK(sample(repeat.id(), 1.1f, 0)[0] == 1);
    CHECK(sample(repeat.id(), -0.9f, 0)[0] == 1);
    CHECK(sample(clamp.id(), 1.1f, 0)[1] == 1);
    CHECK(sample(0, 0.5f, 0) == linear); // Asset filtering survives retro sampling.
    const auto id = repeat.id();
    {
        TextureSampler moved(std::move(repeat));
        CHECK(repeat.id() == 0 && moved.id() == id);
        TextureSampler assigned;
        assigned = std::move(moved);
        CHECK(moved.id() == 0 && assigned.id() == id && glIsSampler(id));
    }
    CHECK(!glIsSampler(id));
}
void minificationChecks() {
    RenderTarget output({8, 8}, RenderTarget::Kind::ColorDepth);
    RenderTarget source({2, 1}, RenderTarget::Kind::ColorDepth);
    glBindTexture(GL_TEXTURE_2D, source.texture());
    constexpr unsigned char texels[] = {255, 0, 0, 0, 255, 0};
    constexpr unsigned char mip[] = {0, 0, 255};
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_RGB, GL_UNSIGNED_BYTE, texels);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, mip);
    TextureSampler nearest;
    TextureSampler repeating(true);
    glBindSampler(0, nearest.id());
    ShaderProgram program(shaders::retro_vert, R"(#version 330 core
        in vec2 vTexCoord;
        uniform sampler2D uTexture;
        uniform vec2 uUv;
        uniform float uFootprint;
        out vec4 FragColor;
        void main() {
            FragColor = sampleSurfaceLod(uTexture, uUv, 0.0, vTexCoord * uFootprint);
        }
    )", "PSX minification regression", shaders::texture_filter_glsl);
    program.bind();
    glUniform2f(program.uniform("uScale"), 2, 2);
    glUniform1i(program.uniform("uTexture"), 0);
    glUniform1i(program.uniform("uMaxTextureLod"), 1);
    glUniform1f(program.uniform("uPsxLodBias"), 0);
    Mesh quad = makeQuadMesh();
    output.bind();
    auto sample = [&](float u, float footprint, bool enabled = true, bool repeat = false) {
        glBindSampler(0, repeat ? repeating.id() : nearest.id());
        glUniform1i(program.uniform("uPsxMinification"), enabled);
        glUniform1i(program.uniform("uTextureRepeat"), repeat);
        glUniform2f(program.uniform("uUv"), u, .5f);
        // Two source texels across eight raster pixels.
        glUniform1f(program.uniform("uFootprint"), footprint * 4);
        quad.draw();
        return pixel(4, 4);
    };
    for (float u : {.49f, .51f, -.9f, 1.1f}) {
        for (bool repeat : {false, true}) {
            CHECK(sample(u, .5f, true, repeat) == sample(u, .5f, false, repeat));
        }
    }
    CHECK(sample(.49f, .5f)[0] == 1 && sample(.51f, .5f)[1] == 1); // Magnified edges stay hard.
    CHECK(sample(1.1f, .5f, true, true)[0] == 1); // Manual nearest fetches retain repeat wrapping.
    CHECK(sample(.51f, 8, false)[1] == 1); // Surfaces that opt out keep the original PSX sampler.
    for (float u : {.49f, .51f, -.9f, 1.1f}) {
        CHECK(sample(u, 8)[2] == 1); // Subpixel details resolve, including clamp edges and the last mip.
        CHECK(sample(u, 8, true, true)[2] == 1);
    }
    const auto transition = sample(.51f, std::sqrt(2.0f));
    CHECK(transition[0] == 0 && near(transition[1], .5f, .005f) && near(transition[2], .5f, .005f));
    for (float boundary : {1.0f, 2.0f}) {
        const auto before = sample(.51f, boundary * .999f);
        const auto after = sample(.51f, boundary * 1.001f);
        for (int channel = 0; channel < 3; ++channel) {
            CHECK(std::abs(after[channel] - before[channel]) <= 1.0f / 255 + .0001f); // No discrete LOD pop.
        }
    }
    glUniform1f(program.uniform("uPsxLodBias"), -1);
    CHECK(sample(.51f, 2)[1] == 1); // Negative bias preserves finer grain.
    glUniform1f(program.uniform("uPsxLodBias"), 1);
    CHECK(sample(.51f, .25f)[1] == 1); // Bias must not filter texels whose biased footprint is still magnified.
    glUniform1f(program.uniform("uPsxLodBias"), 0);
    constexpr float values[] = {.25f, .75f};
    Texture unmipped = Texture::fromRedChannel({2, 1}, values);
    glBindTexture(GL_TEXTURE_2D, unmipped.id());
    glUniform1i(program.uniform("uMaxTextureLod"), 0);
    CHECK(near(sample(.51f, 32)[0], .75f, .005f)); // Single-level textures remain valid.
    glBindSampler(0, 0);
}
void colorChecks() {
    constexpr int width = 32, height = 17; // Odd height also checks the top-down dither phase.
    RenderTarget source({width, height}, RenderTarget::Kind::ColorDepth);
    std::array<unsigned char, width * height * 3> texels{};
    ShaderProgram program(shaders::retro_vert, shaders::retro_frag, "PS1 color regression",
                          std::string(shaders::lens_glsl) + std::string(shaders::ambient_occlusion_glsl));
    program.bind();
    glUniform2f(program.uniform("uScale"), 2, 2);
    glUniform1i(program.uniform("uTexture"), 0);
    const float aspect = static_cast<float>(width) / height;
    glUniform1f(program.uniform("uLensAspect"), aspect);
    Mesh quad = makeQuadMesh();
    // Hardware PS1 offsets in top-down raster order; native dither must survive enlargement.
    const int offsets[4][4] = {{-4, 0, -3, 1}, {2, -2, 3, -1}, {-3, 1, -4, 0}, {3, -1, 2, -2}};
    auto fill = [&](auto rgb) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const auto value = rgb(x, y);
                for (int channel = 0; channel < 3; ++channel) {
                    texels[(y * width + x) * 3 + channel] = value[channel];
                }
            }
        }
    };
    auto verify = [&](std::array<float, 3> ditherWeights, bool interiorOnly = false, float lensStrength = 0) {
        glBindTexture(GL_TEXTURE_2D, source.texture());
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, texels.data());
        glUniform1f(program.uniform("uLensStrength"), lensStrength);
        for (int scale : {1, 3}) {
            const Size enlarged = {width * scale, height * scale};
            RenderTarget output(enlarged, RenderTarget::Kind::ColorDepth);
            output.bind();
            glBindTexture(GL_TEXTURE_2D, source.texture());
            quad.draw();
            std::vector<float> actual(enlarged.width * enlarged.height * 3);
            glReadPixels(0, 0, enlarged.width, enlarged.height, GL_RGB, GL_FLOAT, actual.data());
            for (int y = 0; y < enlarged.height; ++y) {
                for (int x = 0; x < enlarged.width; ++x) {
                    const float px = (x + .5f) / enlarged.width * 2 - 1;
                    const float py = (y + .5f) / enlarged.height * 2 - 1;
                    const float radius = (px * px * aspect * aspect + py * py) / (1 + aspect * aspect);
                    const float lens = (1 + lensStrength * radius) / (1 + lensStrength);
                    const int nativeX = std::clamp(int((px * lens * .5f + .5f) * width), 0, width - 1);
                    const int nativeY = std::clamp(int((py * lens * .5f + .5f) * height), 0, height - 1);
                    if (interiorOnly && (nativeX < 2 || nativeY < 2 || nativeX >= width - 2 ||
                                         nativeY >= height - 2)) {
                        continue;
                    }
                    const int dither = offsets[(height - 1 - nativeY) % 4][nativeX % 4];
                    for (int channel = 0; channel < 3; ++channel) {
                        const int rgb8 = texels[(nativeY * width + nativeX) * 3 + channel];
                        const float offset = dither * ditherWeights[channel];
                        const float rgb5 = std::floor(std::clamp(rgb8 + offset, 0.0f, 255.0f) / 8);
                        // RGB8 storage/readback may round by one byte; RGB555 steps are much larger.
                        CHECK(near(actual[(y * enlarged.width + x) * 3 + channel], rgb5 / 31, 1.0f / 255 + .0001f));
                    }
                }
            }
        }
    };
    constexpr std::array<float, 3> noDither = {0, 0, 0};
    glUniform1f(program.uniform("uDitherStrength"), 1);
    // Even colors between palette entries stay solid instead of acquiring a checker pattern.
    for (const auto rgb : {std::array{119, 128, 139}, std::array{8, 248, 0}, std::array{255, 255, 255}}) {
        fill([&](int, int) { return rgb; });
        verify(noDither);
    }
    // Reject both gentle hard steps and high-contrast surface/texture boundaries.
    for (int jump : {6, 96}) {
        fill([&](int x, int) {
            const int offset = x >= width / 2 ? jump : 0;
            return std::array{119 + offset, 128 + offset, 139 + offset};
        });
        verify(noDither);
    }
    fill([](int x, int y) { return std::array{119 + ((x + y) % 2) * 6, 128, 139}; });
    verify(noDither); // Fine texture patterns do not attract extra dither.
    // Slow and faster color ramps retain the signed PS1 pattern, including after the lens warp.
    for (bool slow : {false, true}) {
        fill([&](int x, int y) {
            return std::array{32 + (slow ? x / 2 : x * 4), 80 + (slow ? y / 2 : y * 4),
                              160 - (slow ? x / 2 : x * 4)};
        });
        for (float strength : {0.0f, .5f, 1.0f}) {
            glUniform1f(program.uniform("uDitherStrength"), strength);
            const std::array expected = {strength, strength, strength};
            verify(expected, true);
            verify(expected, true, .095f);
        }
    }
    glUniform1f(program.uniform("uDitherStrength"), 1);
    fill([](int x, int) { return std::array{32 + x / 2, 119, 139}; });
    verify({1, 0, 0}, true);
}
void shadowBlendChecks() {
    constexpr int width = 16, height = 17;
    RenderTarget source({width, height}, RenderTarget::Kind::ColorDepth);
    constexpr std::string_view vertex = R"(#version 330 core
        layout(location = 0) in vec3 aPosition;
        uniform vec2 uScale;
        uniform vec2 uExtent;
        uniform float uOffset;
        uniform float uTilt;
        out vec3 vDitherPosition;
        void main() {
            vDitherPosition = vec3((aPosition.xy + 0.5) * uExtent, 0.0);
            vDitherPosition.z = uTilt * (vDitherPosition.x + vDitherPosition.y);
            gl_Position = vec4(aPosition.xy * uScale + vec2(uOffset, 0.0), 0.0, 1.0);
        }
    )";
    ShaderProgram program(vertex, R"(#version 330 core
        uniform float uCoverage;
        uniform bool uAmbientOnly;
        uniform float uDiffuse;
        out vec4 FragColor;
        void main() {
            float visibility = shadowBlend(uCoverage, vDitherPosition,
                                           dFdx(vDitherPosition), dFdy(vDitherPosition));
            FragColor = vec4(vec3(uAmbientOnly ? ditheredShadowAmbient(visibility, uDiffuse) : visibility), 1.0);
        }
    )", "shadow coverage regression", shaders::lighting_glsl);
    program.bind();
    glUniform2f(program.uniform("uScale"), 2, 2);
    // A fixed mesh-space grid with two native pixels per cell.
    glUniform2f(program.uniform("uExtent"), width / 96.0f, height / 96.0f);
    Mesh quad = makeQuadMesh();
    std::vector<float> pixels(width * height);
    auto draw = [&](float coverage, float strength) {
        program.bind();
        glUniform1f(program.uniform("uCoverage"), coverage);
        glUniform1f(program.uniform("uShadowDitherStrength"), strength);
        source.bind();
        quad.draw();
        glReadPixels(0, 0, width, height, GL_RED, GL_FLOAT, pixels.data());
    };
    for (float coverage : {0.0f, .125f, .25f, .5f, .75f, .875f, 1.0f}) {
        for (float strength : {0.0f, .5f, 1.0f}) {
            draw(coverage, strength);
            float mean = 0;
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < width; ++x) {
                    const float actual = pixels[y * width + x];
                    CHECK(actual >= 0 && actual <= 1);
                    if (strength == 0 || coverage == 0 || coverage == 1) {
                        CHECK(near(actual, coverage, 1.0f / 255 + .0001f));
                    } else if (strength == 1) {
                        CHECK(actual == 0 || actual == 1); // Mix lit/shaded pixels, not gray noise.
                    }
                    mean += actual;
                    if (x < 8 && y < 8) {
                        CHECK(actual == pixels[(y + 8) * width + x + 8]);
                    }
                }
            }
            CHECK(near(mean / 256, coverage, 1.0f / 255 + .0001f)); // Preserve the shadow's coverage.
            if (coverage > 0 && coverage < 1) {
                const auto [low, high] = std::minmax_element(pixels.begin(), pixels.end());
                CHECK(near(*high - *low, strength, 2.0f / 255 + .0001f));
            }
        }
    }
    draw(.125f, 1);
    CHECK(pixels[0] == 1 && pixels[4] == 0 && pixels[4 * width + 4] == 1);
    for (float tilt : {0.0f, 1.0f}) {
        glUniform1f(program.uniform("uTilt"), tilt);
        draw(.125f, 1);
        const auto stationary = pixels;
        for (int shift : {1, 2, 3}) {
            glUniform1f(program.uniform("uOffset"), 2.0f * shift / width);
            draw(.125f, 1);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width - shift; ++x) {
                    CHECK(pixels[y * width + x + shift] == stationary[y * width + x]);
                }
            }
        }
        glUniform1f(program.uniform("uOffset"), 0);
    }
    glUniform1f(program.uniform("uTilt"), 0);
    // Tiny coverage changes must not suddenly flip a threshold cell black/white.
    draw(.53125f - .001f, 1);
    const auto belowThreshold = pixels;
    draw(.53125f + .001f, 1);
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        CHECK(std::abs(pixels[i] - belowThreshold[i]) < .06f);
    }
    // Subpixel cells fade into filtered coverage rather than aliasing in the distance.
    glUniform2f(program.uniform("uExtent"), width / 24.0f, height / 24.0f);
    draw(.375f, 1);
    for (float actual : pixels) {
        CHECK(near(actual, .375f, 1.0f / 255 + .0001f));
    }
    glUniform2f(program.uniform("uExtent"), width / 96.0f, height / 96.0f);

    draw(.25f, 1);
    RenderTarget enlarged({width * 3, height * 3}, RenderTarget::Kind::ColorDepth);
    ShaderProgram presentation(shaders::retro_vert, shaders::retro_frag, "shadow enlargement",
                               std::string(shaders::lens_glsl) + std::string(shaders::ambient_occlusion_glsl));
    presentation.bind();
    glUniform2f(presentation.uniform("uScale"), 2, 2);
    glUniform1i(presentation.uniform("uTexture"), 0);
    glUniform1f(presentation.uniform("uDitherStrength"), 0);
    glUniform1f(presentation.uniform("uLensStrength"), 0);
    glUniform1f(presentation.uniform("uLensAspect"), float(width) / height);
    enlarged.bind();
    glBindTexture(GL_TEXTURE_2D, source.texture());
    quad.draw();
    std::vector<float> magnified(width * height * 9);
    glReadPixels(0, 0, width * 3, height * 3, GL_RED, GL_FLOAT, magnified.data());
    for (int y = 0; y < height * 3; ++y) {
        for (int x = 0; x < width * 3; ++x) {
            CHECK(magnified[y * width * 3 + x] == pixels[(y / 3) * width + x / 3]);
        }
    }

    program.bind();
    glUniform1i(program.uniform("uDitheredShadows"), 1);
    for (float visibility : {0.0f, .25f, .5f, 1.0f}) {
        draw(visibility, 0);
        const auto mesh = pixels;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const float expected = (x + y) % 2 == 0 ? visibility : 1.0f;
                CHECK(near(pixels[y * width + x], expected, 1.0f / 255 + .0001f));
            }
        }
        draw(visibility, 1);
        CHECK(pixels == mesh); // Mesh transparency is independent of color/edge-dither strength.
    }
    draw(0, 0);
    const auto mesh = pixels;
    // Mesh cells are framebuffer pixels, even when the surface grid or pose changes.
    glUniform2f(program.uniform("uExtent"), width / 12.0f, height / 12.0f);
    glUniform1f(program.uniform("uTilt"), 1);
    glUniform1f(program.uniform("uOffset"), 2.0f / width);
    draw(0, 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 1; x < width; ++x) {
            CHECK(pixels[y * width + x] == mesh[y * width + x]);
        }
    }
    glUniform1f(program.uniform("uOffset"), 0);
    draw(0, 0);
    for (float lens : {0.0f, .095f}) {
        presentation.bind();
        glUniform1f(presentation.uniform("uLensStrength"), lens);
        enlarged.bind();
        glBindTexture(GL_TEXTURE_2D, source.texture());
        quad.draw();
        glReadPixels(0, 0, width * 3, height * 3, GL_RED, GL_FLOAT, magnified.data());
        const float aspect = float(width) / height;
        for (int y = 0; y < height * 3; ++y) {
            for (int x = 0; x < width * 3; ++x) {
                const float px = (x + .5f) / (width * 3) * 2 - 1;
                const float py = (y + .5f) / (height * 3) * 2 - 1;
                const float radius = (px * px * aspect * aspect + py * py) / (1 + aspect * aspect);
                const float warp = (1 + lens * radius) / (1 + lens);
                const int nativeX = std::clamp(int((px * warp * .5f + .5f) * width), 0, width - 1);
                const int nativeY = std::clamp(int((py * warp * .5f + .5f) * height), 0, height - 1);
                CHECK(magnified[y * width * 3 + x] == mesh[nativeY * width + nativeX]);
            }
        }
    }
    // Stronger ink only dims covered pixels, including their ambient/sky light.
    // The ordinary path and sun-away faces must retain their ambient lighting.
    program.bind();
    glUniform1i(program.uniform("uAmbientOnly"), 1);
    for (bool enabled : {false, true}) {
        glUniform1i(program.uniform("uDitheredShadows"), enabled);
        for (float darkness : {0.0f, .8f, 1.0f}) {
            glUniform1f(program.uniform("uDitheredShadowDarkness"), darkness);
            for (float diffuse : {0.0f, .05f, 1.0f}) {
                glUniform1f(program.uniform("uDiffuse"), diffuse);
                for (float visibility : {0.0f, .5f, 1.0f}) {
                    draw(visibility, 0);
                    for (int y = 0; y < height; ++y) {
                        for (int x = 0; x < width; ++x) {
                            const float actual = pixels[y * width + x];
                            const float shaded = 1 - darkness * (1 - visibility);
                            if (!enabled || (x + y) % 2 != 0 || diffuse == 0) {
                                CHECK(actual == 1);
                            } else if (diffuse == 1) {
                                CHECK(near(actual, shaded, 1.0f / 255 + .0001f));
                            } else if (darkness > 0 && visibility < 1) {
                                CHECK(actual > shaded && actual < 1); // Gradual fade at grazing angles.
                            } else {
                                CHECK(actual == 1);
                            }
                        }
                    }
                }
            }
        }
    }
}
void gouraudChecks() {
    RenderTarget output({128, 128}, RenderTarget::Kind::ColorDepth);
    // The screen triangle has unequal W and three orthogonal normals. Only
    // vertex A faces the sun, so console Gouraud light equals its screen weight.
    constexpr std::array<float, 24> vertices = {-0.75f, -0.75f, 1, 0, 1, 0, 0, 0,
                                                3, -3, 4, 1, 0, 0, 1, 0,
                                                -1.5f, 1.5f, 2, 0, 0, 1, 0, 1};
    Mesh triangle(vertices, 8, {{0, 3, 0}, {1, 3, 3}, {2, 2, 6}});
    Mat4 projection{};
    projection.values[0] = projection.values[5] = projection.values[11] = 1;
    const std::array sources = {std::pair{shaders::model_vert, shaders::model_frag},
                                std::pair{shaders::cube_vert, shaders::cube_frag}};
    for (int kind = 0; kind < 2; ++kind) {
        ShaderProgram program(sources[kind].first, sources[kind].second, "Gouraud regression",
                              std::string(unlit) + std::string(shaders::texture_filter_glsl),
                              shaders::vertex_snap_glsl);
        program.bind();
        matrix(program, "uMvp", projection);
        matrix(program, "uModel", identity());
        glUniform3f(program.uniform("uSnap"), 128, 128, 0);
        glUniform3f(program.uniform("uSunDir"), 0, 1, 0);
        glUniform1f(program.uniform("uAmbient"), 0);
        glUniform3f(program.uniform(kind == 0 ? "uBaseColor" : "uColor"), 1, 1, 1);
        output.bind();
        glUniform1i(program.uniform("uVertexLighting"), 1);
        triangle.draw();
        const float vertexShade = pixel(40, 40)[0];
        CHECK(near(vertexShade, 1 - 2 * (40.5f - 16) / 96, 0.005f));
        glUniform1i(program.uniform("uVertexLighting"), 0);
        triangle.draw();
        CHECK(pixel(40, 40)[0] > vertexShade + 0.3f); // Normal mode keeps per-pixel normals.
    }
}
} // namespace
int main() {
    try {
        engine::platform::Window window({"Retro rendering tests", {128, 128}, false, false});
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_DITHER);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        affineChecks();
        snapChecks();
        samplerChecks();
        minificationChecks();
        colorChecks();
        shadowBlendChecks();
        gouraudChecks();
        for (float invalid : {-1.0f, std::numeric_limits<float>::infinity()}) {
            RenderSettings settings;
            settings.retroPixelStep = invalid;
            checkThrows([&] { SceneRenderer renderer(settings); });
        }
        for (float invalid : {-1.0f, 1.1f, std::numeric_limits<float>::quiet_NaN()}) {
            RenderSettings settings;
            settings.modelAffineStrength = invalid;
            checkThrows([&] { SceneRenderer renderer(settings); });
            settings = {};
            settings.retroSnapStrength = invalid;
            checkThrows([&] { SceneRenderer renderer(settings); });
            settings = {};
            settings.retroAffineStrength = invalid;
            checkThrows([&] { SceneRenderer renderer(settings); });
            settings = {};
            settings.retroDitherStrength = invalid;
            checkThrows([&] { SceneRenderer renderer(settings); });
            settings = {};
            settings.psxDitheredShadowDarkness = invalid;
            checkThrows([&] { SceneRenderer renderer(settings); });
        }
        CHECK(glGetError() == GL_NO_ERROR);
        std::cout << "Retro tests passed: affine UVs, snapping, clipping, nearest sampling/minification, "
                     "selective RGB555 dither, shadow-edge blending and Gouraud light\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
