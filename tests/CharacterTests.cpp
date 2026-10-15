#include "GameConfig.h"
#include "TestSupport.h"
#include "engine/graphics/SceneRenderer.h"
#include "engine/platform/Window.h"
#include "game/WorldScene.h"
#include <algorithm>
#include <glad/glad.h>
#include <iostream>

using namespace engine::graphics;
using namespace engine::math;
namespace {
using engine::assets::ModelPose;
std::vector<unsigned char> capture(Size size) {
    std::vector<unsigned char> pixels(size.width * size.height * 3);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, size.width, size.height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    CHECK(glGetError() == GL_NO_ERROR);
    GLint binding = -1;
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 0, &binding);
    CHECK(binding == 0);
    return pixels;
}
double difference(const auto& a, const auto& b) {
    double sum = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        sum += std::abs(int(a[i]) - int(b[i]));
    return sum / a.size();
}
int redPixels(const auto& pixels) {
    int count = 0;
    for (std::size_t i = 0; i < pixels.size(); i += 3) {
        if (pixels[i] > 70 && pixels[i] > pixels[i + 1] + 40 && pixels[i] > pixels[i + 2] + 40)
            ++count;
    }
    return count;
}
std::vector<Mat4> palette(const ModelPose& pose) {
    const auto span = pose.jointMatrices(0);
    return {span.begin(), span.end()};
}
bool same(const auto& a, const auto& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](const Mat4& left, const Mat4& right) {
               return left.values == right.values;
           });
}
const ModelInstance& cat(const Scene& scene, const game::GameAssets& assets) {
    const ModelInstance* result = nullptr;
    for (const auto& object : scene.objects) {
        if (const auto* command = std::get_if<ModelInstance>(&object);
            command && command->model == &assets.model(game::ModelId::PlayerCat)) {
            CHECK(result == nullptr);
            result = command;
        }
    }
    CHECK(result && result->pose && result->pbrSurface);
    return *result;
}
void gaitChecks(const game::GameAssets& assets) {
    game::WorldScene world(assets);
    const auto& start = cat(world.build(0, 0, 0, {}, {}), assets);
    const auto idle = palette(*start.pose);
    const auto& run = cat(world.build(0, 37, 4, {}, {}, .01f, 0, 10), assets);
    const auto first = palette(*run.pose);
    CHECK(!same(first, idle));
    const auto& step = cat(world.build(.8f, 38.2f, 4.12f, {}, {}, .01f, 0, 10), assets);
    const auto second = palette(*step.pose);
    CHECK(!same(first, second));
    CHECK(std::abs(step.transform.values[0]) > .01f); // Lane changes turn the body toward lateral movement.
    const auto& data = step.pose->data();
    const auto node = [&](const char* name) {
        const auto found =
            std::find_if(data.nodes.begin(), data.nodes.end(), [&](const auto& n) { return n.name == name; });
        CHECK(found != data.nodes.end());
        return int(found - data.nodes.begin());
    };
    const int ears = node("Plane"), head = node("Bone.25");
    const auto relative = multiply(inverseAffine(step.pose->world(head)), step.pose->world(ears));
    for (int i = 0; i < 16; ++i)
        CHECK(near(relative.values[i], data.nodes[ears].localBind.values[i]));
    const auto revision = step.pose->revision();
    const auto& frozen = cat(world.build(.8f, 38.2f, 4.116f, {}, {}, 0, 0, 0), assets);
    CHECK(frozen.pose->revision() == revision && same(palette(*frozen.pose), second));
    const auto& repeated = cat(world.build(.8f, 38.2f, 4.116f, {}, {}, 0, 0, 0), assets);
    CHECK(repeated.pose->revision() == revision);
    const auto& restart = cat(world.build(0, 0, 0, {}, {}), assets);
    CHECK(same(palette(*restart.pose), idle));
    const auto& nextRun = cat(world.build(0, 37, 4, {}, {}, .01f, 0, 10), assets);
    CHECK(same(palette(*nextRun.pose), first));
    CHECK(nextRun.pose->boundsHalfExtent().y > .4f);
}
engine::assets::ModelData triangle() {
    engine::assets::ModelData data;
    data.nodes = {{"Joint"}, {"Mesh"}};
    data.nodeOrder = {0, 1};
    data.skins = {{{0}, {identity()}}};
    engine::assets::PrimitiveData primitive;
    primitive.node = 1;
    primitive.skin = 0;
    primitive.vertices = {48, 0, 0, 0, 0, 1, 0, 0, 52, 0, 0, 0, 0, 1, 1, 0, 50, 3, 0, 0, 0, 1, .5f, 1};
    primitive.indices = {0, 1, 2};
    primitive.material.baseColor = {1, .01f, .01f, 1};
    primitive.material.metallic = 0;
    primitive.material.roughness = .7f;
    primitive.influences.resize(3);
    for (auto& influence : primitive.influences)
        influence.weights[0] = 1;
    data.primitives.push_back(std::move(primitive));
    return data;
}
void skinPassChecks(Size size) {
    const auto data = triangle();
    Model skinned(data);
    ModelPose pose(*skinned.animationData()), otherPose(*skinned.animationData());
    pose.setLocal(0, translation({-50, 0, 0}));
    pose.evaluate();
    otherPose.setLocal(0, translation({-47, 0, 0}));
    otherPose.evaluate();
    auto baked = data;
    baked.nodes.clear();
    baked.nodeOrder.clear();
    baked.skins.clear();
    baked.primitives[0].skin = baked.primitives[0].node = -1;
    baked.primitives[0].influences.clear();
    for (std::size_t i = 0; i < baked.primitives[0].vertices.size(); i += engine::assets::kModelVertexStride) {
        baked.primitives[0].vertices[i] -= 50;
    }
    Model reference(baked);
    RenderSettings settings;
    settings.retroSize = size;
    settings.shadowMapSize = 256;
    settings.worldReflectionMapSize = 32;
    settings.planarReflectionSize = 256;
    settings.planarReflectionBlendSeconds = 0;
    settings.retroDitherStrength = 0;
    SceneRenderer renderer(settings);
    const float zero[] = {0}, one[] = {1};
    Texture backing = Texture::fromRedChannel({1, 1}, zero);
    Texture glass = Texture::fromRedChannel({1, 1}, one);
    Scene scene;
    scene.sky.top = scene.sky.bottom = {0, 0, 0};
    scene.sky.texturedClouds[0].texture = &backing;
    scene.sky.texturedClouds[0].opacity = 0;
    scene.light.ambient = .3f;
    scene.light.direction = {1, 2, 1};
    scene.light.shadow.center = {0, 1, 0};
    scene.light.shadow.halfWidth = scene.light.shadow.halfHeight = 8;
    Quad ground;
    ground.model = multiply(rotateX(-kPi * .5f), scale({20, 20, 1}));
    ground.color = {.7f, .7f, .7f};
    const auto frame = [&] {
        renderer.render(scene, size);
        return capture(size);
    };
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        renderer.setMode(mode);
        for (bool pbr : {false, true}) {
            scene.camera.position = {0, 3, 8};
            scene.camera.target = {0, 1, 0};
            ModelInstance actor{identity(), &skinned, pbr};
            actor.pose = &pose;
            ModelInstance other{identity(), &skinned, pbr};
            other.pose = &otherPose;
            scene.objects = {ground, actor, other};
            const auto actual = frame();
            // Pose bounds, not the original mesh at X=50, must control camera and shadow culling.
            CHECK(redPixels(actual) > 100);
            scene.objects = {ground, ModelInstance{identity(), &reference, pbr},
                             ModelInstance{translation({3, 0, 0}), &reference, pbr}};
            const auto expected = frame();
            CHECK(difference(actual, expected) < .2);
            Quad mirror;
            mirror.texture = &backing;
            mirror.materialTextures.glass = &glass;
            mirror.skyscraperWindows = true;
            mirror.castsShadow = false;
            mirror.model = scale({8, 8, 1});
            mirror.normal = {0, 0, 1};
            mirror.windowMaterial.roughness = 0;
            mirror.windowMaterial.reflectance =
                .8f; // Use a clear mirror to compare geometry without dark-glass attenuation.
            scene.camera.position = {0, 0, 4};
            scene.camera.target = {};
            actor.transform = translation({0, -1, 7});
            other.transform = actor.transform;
            scene.objects = {mirror, actor, other};
            const auto reflected = frame();
            CHECK(redPixels(reflected) > 30);
            scene.objects = {mirror, ModelInstance{actor.transform, &reference, pbr},
                             ModelInstance{multiply(actor.transform, translation({3, 0, 0})), &reference, pbr}};
            CHECK(difference(reflected, frame()) < .2);
        }
    }
}
} // namespace
int main() {
    try {
        engine::platform::Window window({"Player animation tests", {192, 192}, false, false});
        game::GameAssets assets(TEST_ASSET_DIR, false);
        gaitChecks(assets);
        skinPassChecks(window.drawableSize());
        std::cout
            << "Character tests passed: run, lane changes, ears, freeze/reset, GPU skinning, shadows and reflections\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
