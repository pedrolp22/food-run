#include "ShaderSources.h"
#include "TestSupport.h"
#include "engine/graphics/Mesh.h"
#include "engine/graphics/RenderTarget.h"
#include "engine/graphics/ShaderProgram.h"
#include "engine/graphics/Texture.h"
#include "engine/math/Math.h"
#include "engine/platform/Window.h"
#include <array>
#include <glad/glad.h>
#include <iostream>

using namespace engine::graphics;
int main() {
    try {
        engine::platform::Window window({"Ripple regression tests", {64, 16}, false, false});
        RenderTarget target({64, 16}, RenderTarget::Kind::ColorDepth);
        std::array<float, 64> distances{};
        for (int x = 0; x < 64; ++x) {
            distances[x] = (x + .5f) * .5f - 1;
        }
        Texture field = Texture::fromRedChannel({64, 1}, distances);
        ShaderProgram shader(shaders::ripple_vert, shaders::ripple_frag, "ripple regression", shaders::fog_glsl,
                             shaders::vertex_snap_glsl);
        Mesh quad = makeQuadMesh();
        shader.bind();
        const auto mvp = engine::math::scale({2, 2, 1});
        glUniformMatrix4fv(shader.uniform("uMvp"), 1, GL_FALSE, mvp.values.data());
        glUniform3f(shader.uniform("uSnap"), 64, 16, 0);
        glUniform1i(shader.uniform("uDistanceField"), 0);
        glUniform1f(shader.uniform("uRadius"), 24);
        glUniform1f(shader.uniform("uWidth"), 1);
        glUniform1f(shader.uniform("uOpacity"), 1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, field.id());
        target.bind();
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_DITHER);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glClearColor(0, 0, 0, 0);
        const auto render = [&](float phase, Color color) {
            glUniform1f(shader.uniform("uPhase"), phase);
            glUniform3f(shader.uniform("uColor"), color.r, color.g, color.b);
            glClear(GL_COLOR_BUFFER_BIT);
            quad.draw();
            std::array<float, 64 * 3> pixels{};
            glReadPixels(0, 8, 64, 1, GL_RGB, GL_FLOAT, pixels.data());
            CHECK(glGetError() == GL_NO_ERROR);
            return pixels;
        };
        const auto early = render(.25f, {.04f, .85f, .3f});
        const auto later = render(.5f, {.04f, .85f, .3f});
        CHECK(early[8 * 3 + 1] > .5f);                      // Near crest at distance 3.25.
        CHECK(early[32 * 3 + 1] < early[8 * 3 + 1] * .5f);  // Far crest at 15.25 fades down.
        CHECK(later[14 * 3 + 1] > early[14 * 3 + 1] + .3f); // Crest moves outward to 6.25.
        CHECK(later[8 * 3 + 1] < early[8 * 3 + 1] * .2f);
        CHECK(early[8 * 3 + 1] > early[8 * 3] * 4 && early[8 * 3 + 1] > early[8 * 3 + 2] * 2);
        const auto red = render(.25f, {.9f, .15f, .15f});
        CHECK(red[8 * 3] > red[8 * 3 + 1] * 4 && red[8 * 3] > red[8 * 3 + 2] * 4);
        const auto wrapped = render(1.25f, {.04f, .85f, .3f});
        for (int i = 0; i < 64 * 3; ++i) {
            CHECK(near(early[i], wrapped[i], .005f)); // Seamless repeating waves.
        }
        for (int x : {0, 1, 50, 63}) { // Inside the artwork or beyond the outer fade limit.
            CHECK(early[x * 3] == 0 && early[x * 3 + 1] == 0 && early[x * 3 + 2] == 0);
        }
        glUniform1i(shader.uniform("uBurst"), 1);
        const auto burst = [&](float progress) {
            glUniform1f(shader.uniform("uBurstProgress"), progress);
            return render(.25f, {.04f, .85f, .3f});
        };
        const auto impact = burst(0);
        const auto expanding = burst(.1f);
        const auto fading = burst(.7f);
        const auto finished = burst(1);
        CHECK(impact[1] > .7f); // Bright silhouette on the very first frame, with no fade-in delay.
        CHECK(expanding[17 * 3 + 1] > impact[17 * 3 + 1] + .3f); // Outgoing crest accelerates immediately.
        CHECK(fading[1] < impact[1] * .05f);                     // The core flash clears quickly.
        for (float value : finished) {
            CHECK(value == 0); // No repeated wave or residual flash after the one-shot lifetime.
        }
        const auto beforeEnd = burst(.999f);
        for (float value : beforeEnd) {
            CHECK(value < .01f); // Smooth fade into removal.
        }
        glUniform1i(shader.uniform("uBurst"), 0);
        const auto idleAgain = render(.25f, {.04f, .85f, .3f});
        CHECK(idleAgain == early);
        glUniform2f(shader.uniform("uFogRange"), 0, 2);
        const auto misted = render(.25f, {.04f, .85f, .3f});
        CHECK(near(misted[8 * 3 + 1], early[8 * 3 + 1] * .5f, .006f)); // Half-depth cue keeps half the ripple brightness.
        glUniform2f(shader.uniform("uFogRange"), .25f, .5f);
        const auto fogged = render(.25f, {.04f, .85f, .3f});
        for (float value : fogged) {
            CHECK(value == 0); // Distant additive waves fade out instead of adding fog-colored rectangles.
        }
        glUniform2f(shader.uniform("uFogRange"), 0, 0);
        CHECK(render(.25f, {.04f, .85f, .3f}) == early);
        std::cout << "Ripple tests passed: outward motion, fading, color, clipping, immediate collection pop and "
                     "continuity\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
