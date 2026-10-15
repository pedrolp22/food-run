#include "TestSupport.h"
#include "engine/assets/ModelPose.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <limits>

using namespace engine::assets;
using namespace engine::math;
namespace {
Vec3 vertex(const ModelData& data, const ModelPose& pose, std::size_t index) {
    const auto& primitive = data.primitives[0];
    const auto offset = index * kModelVertexStride;
    const Vec3 point{primitive.vertices[offset], primitive.vertices[offset + 1], primitive.vertices[offset + 2]};
    Vec3 result{};
    for (int j = 0; j < 4; ++j) {
        const auto transformed = transformPoint(pose.jointMatrices(0)[primitive.influences[index].joints[j]], point);
        const float weight = primitive.influences[index].weights[j];
        result.x += transformed.x * weight;
        result.y += transformed.y * weight;
        result.z += transformed.z * weight;
    }
    return result;
}
struct Fixture {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("subway-skin-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixture() { std::filesystem::create_directory(path); }
    ~Fixture() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    void json(const std::string& text) { std::ofstream(path / "scene.gltf") << text; }
};
} // namespace
void skinningAssetChecks() {
    Fixture fixture;
    {
        std::ofstream binary(fixture.path / "mesh.bin", std::ios::binary);
        const float positions[] = {0, 0, 0, 0, 1, 0, 1, 1, 0};
        const std::uint16_t indices[] = {0, 1, 2};
        const std::uint8_t joints[] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0};
        const std::uint8_t weights[] = {255, 0, 0, 0, 255, 0, 0, 0, 128, 127, 0, 0};
        const std::uint16_t padding = 0;
        const Mat4 matrices[] = {translation({-1, -1, 0}), translation({-1, 0, 0})};
        binary.write(reinterpret_cast<const char*>(positions), sizeof(positions));
        binary.write(reinterpret_cast<const char*>(indices), sizeof(indices));
        binary.write(reinterpret_cast<const char*>(joints), sizeof(joints));
        binary.write(reinterpret_cast<const char*>(weights), sizeof(weights));
        binary.write(reinterpret_cast<const char*>(&padding), sizeof(padding));
        binary.write(reinterpret_cast<const char*>(matrices), sizeof(matrices));
    }
    const std::string json = R"({"asset":{"version":"2.0"},"scenes":[{"nodes":[0,2]}],
"nodes":[{"mesh":0,"skin":0,"translation":[50,50,50]}, {"translation":[0,1,0]}, {"translation":[1,0,0],"children":[1]}],
"buffers":[{"uri":"mesh.bin","byteLength":196}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36}, {"buffer":0,"byteOffset":36,"byteLength":6},
{"buffer":0,"byteOffset":42,"byteLength":12}, {"buffer":0,"byteOffset":54,"byteLength":12}, {"buffer":0,"byteOffset":68,"byteLength":128}],
"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"},
{"bufferView":2,"componentType":5121,"count":3,"type":"VEC4"},
{"bufferView":3,"componentType":5121,"count":3,"type":"VEC4","normalized":true},
{"bufferView":4,"componentType":5126,"count":2,"type":"MAT4"}],
"skins":[{"joints":[1,2],"inverseBindMatrices":4}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0,"JOINTS_0":2,"WEIGHTS_0":3},"indices":1}]}]})";
    fixture.json(json);
    const auto data = loadGltf(fixture.path / "scene.gltf", 2);
    CHECK(data.skins.size() == 1 && data.primitives[0].influences.size() == 3);
    // The child's index precedes its parent, skin indices differ from node indices, and the mesh-node transform is
    // ignored.
    ModelPose pose(data), independent(data);
    CHECK(near(vertex(data, pose, 0).x, -1) && near(vertex(data, pose, 0).y, 0));
    CHECK(near(vertex(data, pose, 1).x, -1) && near(vertex(data, pose, 1).y, 2));
    CHECK(near(pose.boundsCenter().x, 0) && near(pose.boundsHalfExtent().y, 1));
    pose.setLocal(1, multiply(data.nodes[1].localBind, translation({1, 0, 0})));
    pose.evaluate();
    CHECK(near(vertex(data, pose, 0).x, -1));
    CHECK(near(vertex(data, pose, 1).x, 1));
    CHECK(near(vertex(data, pose, 2).x, 1 + 2 * 128.f / 255));
    CHECK(near(vertex(data, independent, 1).x, -1));
    CHECK(pose.boundsCenter().x > .5f && pose.boundsHalfExtent().x > 1.5f);
    pose.reset();
    pose.evaluate();
    CHECK(near(pose.boundsCenter().x, 0));
    auto invalidTransform = identity();
    invalidTransform.values[0] = std::numeric_limits<float>::quiet_NaN();
    checkThrows([&] { pose.setLocal(1, invalidTransform); });
    for (const auto& [from, to] :
         {std::pair{"\"skin\":0", "\"skin\":99"}, std::pair{"\"joints\":[1,2]", "\"joints\":[1,1]"},
          std::pair{"\"joints\":[1,2]", "\"joints\":[1,99]"}, std::pair{"\"normalized\":true", "\"normalized\":false"},
          std::pair{"\"count\":2,\"type\":\"MAT4\"", "\"count\":1,\"type\":\"MAT4\""},
          std::pair{"\"JOINTS_0\":2", "\"JOINTS_0\":99"},
          std::pair{"\"WEIGHTS_0\":3", "\"WEIGHTS_0\":3,\"JOINTS_1\":2"}}) {
        auto bad = json;
        const auto position = bad.find(from);
        CHECK(position != std::string::npos);
        bad.replace(position, std::char_traits<char>::length(from), to);
        fixture.json(bad);
        checkThrows([&] { loadGltf(fixture.path / "scene.gltf"); });
    }
    const auto cat = loadGltf(std::filesystem::path(TEST_ASSET_DIR) / "models/gato/gato.gltf", 1.75f);
    CHECK(cat.skins[0].joints.size() == 38 && cat.primitives[0].influences.size() == 1140);
    const ModelPose catBind(cat);
    CHECK(near(catBind.boundsCenter().y - catBind.boundsHalfExtent().y, 0));
    CHECK(near(catBind.boundsHalfExtent().y * 2, 1.75f));
    const auto affine =
        multiply(translation({-3, 5, 7}), multiply(rotation({1, 2, 3}, .7f), scale({-.25f, .75f, 1.2f})));
    const auto product = multiply(affine, inverseAffine(affine));
    for (int i = 0; i < 16; ++i)
        CHECK(near(product.values[i], identity().values[i]));
    checkThrows([&] { inverseAffine(scale({1, 0, 1})); });
}
