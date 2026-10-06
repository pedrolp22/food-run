#include "GameConfig.h"
#include "TestSupport.h"
#include "engine/graphics/SceneRenderer.h"
#include "game/WorldScene.h"
#include <algorithm>
#include <glad/glad.h>
#include <set>

using namespace engine::graphics;
using namespace engine::math;
namespace {
std::vector<unsigned char> frame(SceneRenderer& renderer, const Scene& scene, Size size) {
    renderer.render(scene, size);
    std::vector<unsigned char> pixels(size.width * size.height * 3);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, size.width, size.height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    CHECK(glGetError() == GL_NO_ERROR);
    return pixels;
}
double difference(const auto& a, const auto& b) {
    double total = 0;
    for (std::size_t index = 0; index < a.size(); ++index) {
        total += std::abs(int(a[index]) - int(b[index]));
    }
    return total / a.size();
}
struct DiscBounds {
    int left, right = 0, bottom, top = 0, pixels = 0;
};
DiscBounds moonBounds(const auto& pixels, const auto& sky, Size size) {
    DiscBounds bounds{size.width, 0, size.height};
    for (int y = 0; y < size.height; ++y) {
        for (int x = 0; x < size.width; ++x) {
            const int index = (y * size.width + x) * 3;
            if (int(pixels[index]) - sky[index] > 55) {
                bounds.left = std::min(bounds.left, x);
                bounds.right = std::max(bounds.right, x);
                bounds.bottom = std::min(bounds.bottom, y);
                bounds.top = std::max(bounds.top, y);
                ++bounds.pixels;
            }
        }
    }
    return bounds;
}
} // namespace

void skyCycleChecks(const game::GameAssets& assets, game::WorldScene& world, Size size) {
    const Scene day = world.build(0, 37, 24, {}, {}, 0, 0);
    const Scene dusk = world.build(0, 37, 24, {}, {}, 0, .5f);
    Scene night = world.build(0, 37, 24, {}, {}, 0, 1);
    CHECK(dot(day.light.direction, night.light.direction) < .8f);
    CHECK(dot(day.light.direction, dusk.light.direction) > .7f);
    CHECK(dot(normalize(night.sky.moon.direction), night.light.direction) > .9999f);
    CHECK(day.light.viewProjection().values != night.light.viewProjection().values);
    const Scene adjacent = world.build(0, 37, 24, {}, {}, 0, .501f);
    CHECK(dot(adjacent.light.direction, dusk.light.direction) > .9999f); // Shadows move continuously through dusk.
    const auto& moon = assets.texture(game::TextureId::Moon);
    CHECK(night.sky.moon.texture == &moon && !moon.repeats());
    std::vector<unsigned char> texels(moon.size().width * moon.size().height * 4);
    glBindTexture(GL_TEXTURE_2D, moon.id());
    GLint filter = 0;
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &filter);
    CHECK(filter == GL_NEAREST);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &filter);
    CHECK(filter == GL_NEAREST);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels.data());
    CHECK(texels[3] == 0); // The transparent corner must not become an opaque square in the sky.
    CHECK(texels[(moon.size().height / 2 * moon.size().width + moon.size().width / 2) * 4 + 3] == 255);
    glBindTexture(GL_TEXTURE_2D, 0);
    night.objects.clear();
    night.particles.clear();
    night.camera.position = {};
    night.camera.target = night.sky.moon.direction;
    night.camera.fisheyeStrength = 0;
    night.sky.stars.density = 0;
    const auto clouds = night.sky.texturedClouds;
    for (auto& layer : night.sky.texturedClouds) {
        layer.opacity = 0;
    }
    // Exercise both portrait and landscape projection without needing to resize the OS window.
    for (Size viewport : {size, Size{size.width, size.width / 2}}) {
        RenderSettings settings;
        settings.retroSize = viewport;
        settings.shadowMapSize = 32;
        settings.worldReflectionMapSize = 0;
        settings.retroDitherStrength = 0;
        SceneRenderer renderer(settings);
        for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
            renderer.setMode(mode);
            night.sky.nightBlend = 1;
            night.sky.moon.texture = nullptr;
            const auto plain = frame(renderer, night, viewport);
            night.sky.moon.texture = &moon;
            night.sky.moon.glowStrength = 0;
            const auto disc = frame(renderer, night, viewport);
            const auto bounds = moonBounds(disc, plain, viewport);
            CHECK(bounds.pixels > 30);
            CHECK(std::abs((bounds.right - bounds.left) - (bounds.top - bounds.bottom)) <= 2);
            CHECK(std::abs((bounds.left + bounds.right + 1) * .5f - viewport.width * .5f) < 2);
            CHECK(std::abs((bounds.bottom + bounds.top + 1) * .5f - viewport.height * .5f) < 2);
            std::set<int> craterShades;
            for (std::size_t index = 0; index < disc.size(); index += 3) {
                if (int(disc[index]) - plain[index] > 55) {
                    craterShades.insert(disc[index]);
                }
            }
            CHECK(craterShades.size() > 5); // Retain crater detail after the PSX palette reduces the source colors.
            night.sky.moon.glowStrength = GameConfig::kMoonGlowStrength;
            const auto glow = frame(renderer, night, viewport);
            CHECK(difference(glow, disc) > .02);
            CHECK(frame(renderer, night, viewport) == glow);
            night.camera.position = {8, 4, 3};
            night.camera.target = {night.camera.position.x + night.sky.moon.direction.x,
                                   night.camera.position.y + night.sky.moon.direction.y,
                                   night.camera.position.z + night.sky.moon.direction.z};
            CHECK(difference(frame(renderer, night, viewport), glow) < .01); // No sky parallax or distance scaling.
            night.camera.position = {};
            night.camera.target = night.sky.moon.direction;
            night.sky.nightBlend = 0;
            const auto daylight = frame(renderer, night, viewport);
            night.sky.moon.texture = nullptr;
            CHECK(frame(renderer, night, viewport) == daylight); // Moon and halo are absent during the day.
            night.sky.moon.texture = &moon;
            night.sky.nightBlend = 1;
            night.sky.moon.glowStrength = 0;
            night.sky.moon.sizeDegrees *= 2;
            const auto larger = moonBounds(frame(renderer, night, viewport), plain, viewport);
            CHECK(larger.pixels > bounds.pixels * 3 && larger.pixels < bounds.pixels * 5);
            night.sky.moon.sizeDegrees *= .5f;
            night.sky.texturedClouds = clouds;
            const auto covered = frame(renderer, night, viewport);
            night.sky.moon.texture = nullptr;
            const auto cloudSky = frame(renderer, night, viewport);
            CHECK(difference(covered, cloudSky) < difference(disc, plain)); // Clouds pass in front of the moon.
            for (auto& layer : night.sky.texturedClouds) {
                layer.opacity = 0;
            }
        }
    }
}
