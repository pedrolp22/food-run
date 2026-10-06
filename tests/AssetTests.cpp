#include "TestSupport.h"
#include "engine/assets/AlphaDistanceField.h"
#include "engine/assets/GltfLoader.h"
#include "engine/assets/SpriteGeometry.h"
#include "engine/math/Math.h"
#include <chrono>
#include <cstdint>
#include <fstream>
#include <limits>

namespace {
struct Fixture {
    std::filesystem::path directory =
        std::filesystem::temp_directory_path() /
        ("subway-assets-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixture() { std::filesystem::create_directory(directory); }
    ~Fixture() {
        std::error_code error;
        std::filesystem::remove_all(directory, error);
    }
    void json(const std::string& contents) const { std::ofstream(directory / "scene.gltf") << contents; }
};
void replace(std::string& value, const std::string& from, const std::string& to) {
    const auto index = value.find(from);
    CHECK(index != std::string::npos);
    value.replace(index, from.size(), to);
}
void distanceFieldChecks() {
    using namespace engine::assets;
    // Compare the linear-time transform against a brute-force reference, with holes,
    // disconnected islands, threshold alpha, nonsquare dimensions and border padding.
    for (int seed = 0; seed < 12; ++seed) {
        constexpr int width = 7, height = 5, padding = 3;
        std::array<std::uint8_t, width * height> alpha{};
        for (int i = 0; i < int(alpha.size()); ++i) {
            alpha[i] = (i * 17 + seed * 7) % 13 < 4 ? 26 : 25;
        }
        const auto field = makeAlphaDistanceField(alpha, width, height, padding);
        CHECK(field.width == width + 2 * padding && field.height == height + 2 * padding);
        for (int y = 0; y < field.height; ++y) {
            for (int x = 0; x < field.width; ++x) {
                float expected = 1e9f;
                for (int sy = 0; sy < height; ++sy) {
                    for (int sx = 0; sx < width; ++sx) {
                        if (alpha[sy * width + sx] >= 26) {
                            expected =
                                std::min(expected, std::hypot(float(x - sx - padding), float(y - sy - padding)) - .5f);
                        }
                    }
                }
                CHECK(near(field.pixels[y * field.width + x], expected));
            }
        }
    }
    std::array<std::uint8_t, 12> alpha{};
    alpha[7] = 255;
    const auto field = makeAlphaDistanceField(alpha, 4, 3, 4);
    CHECK(near(field.sourceCenter.x, .875f) && near(field.sourceCenter.y, .5f));
    const auto isolated = makeAlphaDistanceField(alpha, 4, 3, 4, true);
    for (float value : isolated.pixels) {
        CHECK(std::isfinite(value) && value > 4);
    }
    alpha[0] = alpha[1] = 255;
    const auto noisy = makeAlphaDistanceField(alpha, 4, 3, 4);
    const auto clean = makeAlphaDistanceField(alpha, 4, 3, 4, true);
    CHECK(clean.sourceCenter.x == noisy.sourceCenter.x && clean.sourceCenter.y == noisy.sourceCenter.y);
    CHECK(clean.pixels[5 * clean.width + 7] > 1 && noisy.pixels[5 * noisy.width + 7] < 0);
    alpha.fill(0);
    const auto empty = makeAlphaDistanceField(alpha, 4, 3, 4);
    for (float value : empty.pixels) {
        CHECK(std::isfinite(value) && value > 4);
    }
    checkThrows([&] { makeAlphaDistanceField(alpha, 4, 3, 0); });
    checkThrows([&] { makeAlphaDistanceField(alpha, 4, 3, -1); });
    checkThrows([&] { makeAlphaDistanceField(alpha, 4, 3, std::numeric_limits<int>::max()); });
    checkThrows([&] { makeAlphaDistanceField(alpha, 4, 4, 4); });
    checkThrows([&] { makeAlphaDistanceField(alpha, -4, 3, 4); });
}
void spriteGeometryChecks() {
    using namespace engine::assets;
    using namespace engine::math;
    const std::array<std::uint8_t, 2> adjacent = {255, 255};
    const auto pair = extrudeSprite(adjacent, 2, 1);
    // Two textured faces plus six perimeter walls; no wall between opaque neighbors.
    CHECK(pair.indices.size() == 8 * 6);
    const std::array<std::uint8_t, 9> ring = {255, 255, 255, 255, 0, 255, 255, 255, 255};
    const auto hollow = extrudeSprite(ring, 3, 3);
    CHECK(hollow.indices.size() == (2 + 12 + 4) * 6); // Includes the hole's four inner walls.
    for (std::size_t i = 0; i < hollow.indices.size(); i += 3) {
        const auto vertex = [&](unsigned int index, int offset) {
            const auto start = index * SpriteGeometry::stride + offset;
            CHECK(start + 2 < hollow.vertices.size());
            return Vec3{hollow.vertices[start], hollow.vertices[start + 1], hollow.vertices[start + 2]};
        };
        const auto a = vertex(hollow.indices[i], 0);
        const auto b = vertex(hollow.indices[i + 1], 0);
        const auto c = vertex(hollow.indices[i + 2], 0);
        const Vec3 ab = {b.x - a.x, b.y - a.y, b.z - a.z};
        const Vec3 ac = {c.x - a.x, c.y - a.y, c.z - a.z};
        CHECK(dot(cross(ab, ac), vertex(hollow.indices[i], 5)) > 0); // Outward winding, no degenerate walls.
    }
    std::array<std::uint8_t, 12> padded{};
    padded[7] = 26; // One visible pixel against the right border, just above the alpha cutoff.
    padded[0] = 25;
    const auto single = extrudeSprite(padded, 4, 3);
    CHECK(single.indices.size() == 36);
    for (std::size_t i = 0; i < single.vertices.size(); i += SpriteGeometry::stride) {
        CHECK(near(std::abs(single.vertices[i]), .125f));
        CHECK(near(std::abs(single.vertices[i + 1]), 1.0f / 6));
        CHECK(near(std::abs(single.vertices[i + 2]), .5f));
        // Correct UV placement after centering, including side-wall samples inside the visible pixel.
        CHECK(single.vertices[i + 3] >= .75f && single.vertices[i + 3] <= 1);
        CHECK(single.vertices[i + 4] >= 1.0f / 3 && single.vertices[i + 4] <= 2.0f / 3);
    }
    padded.fill(0);
    CHECK(extrudeSprite(padded, 4, 3).vertices.empty());
    checkThrows([&] { extrudeSprite(padded, 0, 3); });
    checkThrows([&] { extrudeSprite(padded, -4, 3); });
    checkThrows([&] { extrudeSprite(padded, 4, 4); });
    checkThrows([&] { extrudeSprite({}, 4, 3); });
}
} // namespace
void assetTests() {
    distanceFieldChecks();
    spriteGeometryChecks();
    using namespace engine::assets;
    for (const auto& [name, count] : {std::pair{"traffic_cone", 3}, {"barrier", 4}, {"trash_can", 1}, {"bench", 2},
                                    {"tree", 1}}) {
        const auto model = loadGltf(std::filesystem::path(TEST_ASSET_DIR) / "models" / name / "scene.gltf", 2);
        CHECK(model.primitives.size() == static_cast<std::size_t>(count));
        float low = 1e9f, high = -1e9f;
        for (const auto& primitive : model.primitives) {
            CHECK(!primitive.indices.empty() && primitive.vertices.size() % kModelVertexStride == 0);
            for (std::size_t vertex = 0; vertex < primitive.vertices.size(); vertex += kModelVertexStride) {
                low = std::min(low, primitive.vertices[vertex + 1]);
                high = std::max(high, primitive.vertices[vertex + 1]);
            }
        }
        CHECK(near(low, 0) && near(high, 2));
    }
    Fixture fixture;
    constexpr float vertices[] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    constexpr std::uint16_t indices[] = {0, 1, 2};
    {
        std::ofstream binary(fixture.directory / "mesh.bin", std::ios::binary);
        binary.write(reinterpret_cast<const char*>(vertices), sizeof(vertices));
        binary.write(reinterpret_cast<const char*>(indices), sizeof(indices));
    }
    const std::string json = R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],
"buffers":[{"uri":"mesh.bin","byteLength":42}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],
"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}]})";
    const auto path = fixture.directory / "scene.gltf";
    fixture.json(json);
    const auto triangle = loadGltf(path);
    CHECK(triangle.primitives.size() == 1);
    CHECK(near(triangle.primitives[0].vertices[5], 1));
    CHECK(triangle.primitives[0].material.roughness == 1 && triangle.primitives[0].material.metallic == 1);
    auto materialJson = json;
    replace(materialJson, "\"indices\":1", "\"indices\":1,\"material\":0");
    materialJson.insert(materialJson.size() - 1,
                        R"(,"materials":[{"pbrMetallicRoughness":{"roughnessFactor":0.28,"metallicFactor":0.6}}])");
    fixture.json(materialJson);
    const auto material = loadGltf(path).primitives[0].material;
    CHECK(near(material.roughness, .28f) && near(material.metallic, .6f));
    CHECK(material.alphaCutoff == 0 && !material.doubleSided && !material.baseTextureNearest);
    auto maskedJson = materialJson;
    replace(maskedJson,
            "\"pbrMetallicRoughness\":", "\"alphaMode\":\"MASK\",\"doubleSided\":true,\"pbrMetallicRoughness\":");
    fixture.json(maskedJson);
    CHECK(near(loadGltf(path).primitives[0].material.alphaCutoff, .5f));
    CHECK(loadGltf(path).primitives[0].material.doubleSided);
    replace(maskedJson, "\"alphaMode\":\"MASK\"", "\"alphaMode\":\"MASK\",\"alphaCutoff\":0.3");
    fixture.json(maskedJson);
    CHECK(near(loadGltf(path).primitives[0].material.alphaCutoff, .3f));
    for (const char* invalid : {"-0.1", "1.1", "false", "\"cutout\""}) {
        auto invalidMask = maskedJson;
        replace(invalidMask, "\"alphaCutoff\":0.3", std::string("\"alphaCutoff\":") + invalid);
        fixture.json(invalidMask);
        checkThrows([&] { loadGltf(path); });
    }
    const auto tree = loadGltf(std::filesystem::path(TEST_ASSET_DIR) / "models/tree/scene.gltf");
    CHECK(tree.primitives[0].material.doubleSided && tree.primitives[0].material.baseTextureNearest);
    CHECK(tree.primitives[0].material.baseTextureRepeat);
    CHECK(near(tree.primitives[0].material.alphaCutoff, .0045714425f));
    CHECK(tree.primitives[0].material.metallic == 0 && tree.primitives[0].material.roughness > .8f);
    for (const char* invalidFactor : {"-0.1", "1.1", "true", "\"smooth\""}) {
        auto invalidMaterial = materialJson;
        replace(invalidMaterial, "0.28", invalidFactor);
        fixture.json(invalidMaterial);
        checkThrows([&] { loadGltf(path); });
        invalidMaterial = materialJson;
        replace(invalidMaterial, "0.6", invalidFactor);
        fixture.json(invalidMaterial);
        checkThrows([&] { loadGltf(path); });
    }
    const auto bench = loadGltf(std::filesystem::path(TEST_ASSET_DIR) / "models/bench/scene.gltf");
    CHECK(bench.primitives[0].material.metallic == 0);
    CHECK(near(bench.primitives[0].material.roughness, .915385f));
    CHECK(bench.primitives[1].material.metallic == 1); // Authored metal supports omit the glTF default.
    CHECK(near(bench.primitives[1].material.roughness, .2f));
    CHECK(bench.primitives[0].material.normalTexture.filename() == "Material.001_normal.png");
    CHECK(bench.primitives[1].material.normalTexture.filename() == "Material.002_normal.png");
    // External material images share UV0 and retain linear data/scales separately from albedo.
    Fixture mappedFixture;
    constexpr float uv[] = {0, 0, 1, 0, 0, 1};
    {
        std::ofstream binary(mappedFixture.directory / "mesh.bin", std::ios::binary);
        binary.write(reinterpret_cast<const char*>(vertices), sizeof(vertices));
        binary.write(reinterpret_cast<const char*>(indices), sizeof(indices));
        constexpr std::uint16_t padding = 0; // Align FLOAT UVs to four bytes.
        binary.write(reinterpret_cast<const char*>(&padding), sizeof(padding));
        binary.write(reinterpret_cast<const char*>(uv), sizeof(uv));
    }
    auto mappedJson = json;
    replace(mappedJson, "\"byteLength\":42", "\"byteLength\":68");
    replace(mappedJson, "\"byteOffset\":36,\"byteLength\":6}",
            "\"byteOffset\":36,\"byteLength\":6},{\"buffer\":0,\"byteOffset\":44,\"byteLength\":24}");
    replace(mappedJson, "\"type\":\"SCALAR\"}]",
            "\"type\":\"SCALAR\"},{\"bufferView\":2,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"}]");
    replace(mappedJson, "\"attributes\":{\"POSITION\":0}", "\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":2}");
    replace(mappedJson, "\"indices\":1", "\"indices\":1,\"material\":0");
    mappedJson.insert(mappedJson.size() - 1, R"(,
"images":[{"uri":"color.png"},{"uri":"surface.png"},{"uri":"normal.png"}],
"textures":[{"source":0},{"source":1},{"source":2}],
"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0},"metallicRoughnessTexture":{"index":1},
"roughnessFactor":0.8,"metallicFactor":0.6},"normalTexture":{"index":2,"scale":0.35},
"occlusionTexture":{"index":1,"strength":0.5}}])");
    mappedFixture.json(mappedJson);
    const auto mappedPath = mappedFixture.directory / "scene.gltf";
    const auto mapped = loadGltf(mappedPath).primitives.front().material;
    CHECK(mapped.baseTexture == mappedFixture.directory / "color.png");
    CHECK(mapped.metallicRoughnessTexture == mappedFixture.directory / "surface.png");
    CHECK(mapped.occlusionTexture == mapped.metallicRoughnessTexture);
    CHECK(mapped.normalTexture == mappedFixture.directory / "normal.png");
    CHECK(near(mapped.normalScale, .35f) && near(mapped.occlusionStrength, .5f));
    CHECK(!mapped.packedReflectivity); // Standard glTF data must ignore alpha in a metallic-roughness map.
    for (const auto& [from, to] : {std::pair{"\"scale\":0.35", "\"scale\":true"},
                                  std::pair{"\"strength\":0.5", "\"strength\":1.5"},
                                  std::pair{"\"index\":2", "\"index\":9999"},
                                  std::pair{"\"index\":2", "\"index\":2,\"texCoord\":1"}}) {
        auto invalidMap = mappedJson;
        replace(invalidMap, from, to);
        mappedFixture.json(invalidMap);
        checkThrows([&] { loadGltf(mappedPath); });
    }
    const auto invalid = [&](const std::string& from, const std::string& to) {
        auto changed = json;
        replace(changed, from, to);
        fixture.json(changed);
        checkThrows([&] { loadGltf(path); });
    };
    invalid("\"byteOffset\":36", "\"byteOffset\":9999");
    invalid("\"count\":3", "\"count\":99999999");
    invalid("\"byteOffset\":0", "\"byteOffset\":-1");
    invalid("\"bufferView\":0", "\"bufferView\":99");
    invalid("\"mesh\":0", "\"mesh\":0,\"children\":[0]");
    invalid("\"mesh\":0", "\"mesh\":0,\"scale\":[1]");
    invalid("\"count\":3", "\"count\":xyz");
    fixture.json("{unfinished");
    checkThrows([&] { loadGltf(path); });
    checkThrows([&] { loadGltf(path, 0); });
    checkThrows([&] { loadGltf(path, std::numeric_limits<float>::infinity()); });
    checkThrows([&] { loadGltf(fixture.directory / "missing.gltf"); });
}
