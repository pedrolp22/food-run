#include "Renderer.h"
#include "GameConfig.h"
#include "engine/assets/GltfLoader.h"
#include "engine/graphics/SceneRenderer.h"
#include "engine/platform/Window.h"
#include "game/WorldScene.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <string_view>
#include <utility>

namespace {
using engine::assets::ModelData;
using engine::assets::PrimitiveData;
using engine::math::multiply;
using engine::math::scale;
using engine::math::translation;
using engine::math::Vec3;

constexpr std::array<unsigned int, 26> kMenuGlyphs = {
    0x118fe2eu, 0xf8be2fu, 0x1e0843eu, 0xf8c62fu,  0x1f0bc3fu, 0x0u,       0x0u,      0x0u, 0x1f2109fu,
    0x0u,       0x0u,      0x1f08421u, 0x118d771u, 0x11cd671u, 0xe8c62eu,  0x10be2fu, 0x0u, 0x92be2fu,
    0xf8383eu,  0x42109fu, 0xe8c631u,  0x0u,       0x11dd631u, 0x1151151u, 0x421151u, 0x0u};
constexpr std::array<std::array<int, 4>, 6> kCubeFaces = {
    std::array<int, 4>{4, 5, 6, 7}, {1, 0, 3, 2}, {5, 1, 2, 6}, {0, 4, 7, 3}, {7, 6, 2, 3}, {0, 1, 5, 4}};
constexpr std::array<Vec3, 6> kCubeNormals = {Vec3{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
constexpr std::array<Vec3, 4> kFaceUv = {Vec3{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};

void appendBlock(PrimitiveData& primitive, Vec3 center, Vec3 size) {
    const Vec3 half{size.x * .5f, size.y * .5f, size.z * .5f};
    const std::array<Vec3, 8> corners = {{{center.x - half.x, center.y - half.y, center.z - half.z},
                                          {center.x + half.x, center.y - half.y, center.z - half.z},
                                          {center.x + half.x, center.y + half.y, center.z - half.z},
                                          {center.x - half.x, center.y + half.y, center.z - half.z},
                                          {center.x - half.x, center.y - half.y, center.z + half.z},
                                          {center.x + half.x, center.y - half.y, center.z + half.z},
                                          {center.x + half.x, center.y + half.y, center.z + half.z},
                                          {center.x - half.x, center.y + half.y, center.z + half.z}}};
    constexpr std::array<unsigned int, 6> triangles = {0, 1, 2, 0, 2, 3};
    for (std::size_t face = 0; face < kCubeFaces.size(); ++face) {
        const unsigned int first =
            static_cast<unsigned int>(primitive.vertices.size() / engine::assets::kModelVertexStride);
        for (std::size_t corner = 0; corner < 4; ++corner) {
            const auto point = corners[kCubeFaces[face][corner]];
            const auto normal = kCubeNormals[face];
            const auto uv = kFaceUv[corner];
            primitive.vertices.insert(primitive.vertices.end(),
                                      {point.x, point.y, point.z, normal.x, normal.y, normal.z, uv.x, uv.y});
        }
        for (const auto index : triangles)
            primitive.indices.push_back(first + index);
    }
}

void appendMenuText(PrimitiveData& primitive, std::string_view text, float top, float pixelSize) {
    const float width = (static_cast<float>(text.size()) * 6 - 1) * pixelSize;
    const float left = -width * .5f;
    for (std::size_t character = 0; character < text.size(); ++character) {
        const int letter = text[character] - 'A';
        if (letter < 0 || letter >= static_cast<int>(kMenuGlyphs.size()))
            continue;
        const unsigned int glyph = kMenuGlyphs[letter];
        for (int row = 0; row < 5; ++row) {
            for (int column = 0; column < 5; ++column) {
                if (((glyph >> (row * 5 + column)) & 1u) == 0)
                    continue;
                appendBlock(primitive,
                            {left + (static_cast<float>(character) * 6 + column + .5f) * pixelSize,
                             top - (row + .5f) * pixelSize, .12f},
                            {pixelSize * .78f, pixelSize * .78f, pixelSize * .92f});
            }
        }
    }
}

ModelData makeMenuLettering() {
    ModelData data;
    PrimitiveData lettering;
    lettering.material.baseColor = {1.0f, .84f, .52f, 1};
    lettering.material.emissiveFactor = {.10f, .055f, .018f};
    lettering.material.roughness = .36f;
    lettering.material.metallic = .18f;
    appendMenuText(lettering, "SUBWAY MONEY", 1.06f, .03f);
    appendMenuText(lettering, "PLAY", .45f, .038f);
    appendMenuText(lettering, "OPTIONS", .05f, .038f);
    appendMenuText(lettering, "CREDITS", -.35f, .038f);
    appendMenuText(lettering, "EXIT", -.75f, .038f);
    data.primitives.push_back(std::move(lettering));
    return data;
}

ModelData makeMenuCursor() {
    ModelData data;
    PrimitiveData cursor;
    cursor.material.baseColor = {1.0f, .74f, .30f, 1};
    cursor.material.emissiveFactor = {.14f, .065f, .012f};
    cursor.material.roughness = .38f;
    appendBlock(cursor, {-.025f, .055f, .12f}, {.045f, .035f, .065f});
    appendBlock(cursor, {.025f, 0, .12f}, {.045f, .035f, .065f});
    appendBlock(cursor, {-.025f, -.055f, .12f}, {.045f, .035f, .065f});
    data.primitives.push_back(std::move(cursor));
    return data;
}
} // namespace

struct Renderer::Impl {
    // Destruction runs in reverse order: all GPU assets die before the window's GL context.
    engine::platform::Window window;
    engine::graphics::SceneRenderer graphics;
    game::GameAssets assets;
    game::WorldScene world;
    game::PlayerCharacter menuPlayer;
    engine::graphics::Model menuLettering;
    engine::graphics::Model menuCursor;
    engine::graphics::Scene menuScene;
    engine::graphics::Size drawable;
    std::size_t selectionCursorIndex = 0;
    explicit Impl(const std::filesystem::path& root)
        : window({.title = GameConfig::kWindowTitle, .size = {.width = 720, .height = 1280}}),
          graphics(
              {.retroSize = {.width = GameConfig::kPsxInternalWidth, .height = GameConfig::kPsxInternalHeight},
               .retroDitherStrength = GameConfig::kPsxDitherStrength,
               .psxDitheredShadows = GameConfig::kPsxDitheredShadows,
               .psxDitheredShadowDarkness = GameConfig::kPsxDitheredShadowDarkness,
               .psxDistanceFog = GameConfig::kPsxDistanceFog,
               .psxFogStart = GameConfig::kPsxFogStart,
               .psxFogEnd = GameConfig::kPsxFogEnd,
               .psxFogColor = {GameConfig::kPsxFogColor[0], GameConfig::kPsxFogColor[1], GameConfig::kPsxFogColor[2]},
               .psxNightFogColor = {GameConfig::kPsxNightFogColor[0], GameConfig::kPsxNightFogColor[1],
                                    GameConfig::kPsxNightFogColor[2]},
               .psxFogFollowsSky = GameConfig::kPsxFogFollowsSky}),
          assets(root, GameConfig::kMipmappedTextures), world(assets),
          menuPlayer(*assets.model(game::ModelId::PlayerCat).animationData()), menuLettering(makeMenuLettering()),
          menuCursor(makeMenuCursor()),
          menuScene(world.build(0, 0, 0, {}, {})), drawable(window.drawableSize()) {
        menuScene.camera.position = {0, GameConfig::kMenuCameraHeight, GameConfig::kMenuCameraZ};
        menuScene.camera.target = {0, GameConfig::kMenuCameraLookHeight, GameConfig::kMenuCameraLookZ};
        menuScene.camera.verticalFov = GameConfig::kMenuCameraFov;
        menuScene.camera.fisheyeStrength = 0;
        using namespace engine::math;
        const Mat4 panel = menuPanelTransform();
        engine::graphics::ModelInstance text;
        text.model = &menuLettering;
        text.pbrSurface = true;
        text.transform = panel;
        menuScene.objects.emplace_back(text);
        engine::graphics::ModelInstance cursor;
        cursor.model = &menuCursor;
        cursor.pbrSurface = true;
        cursor.transform = panel;
        selectionCursorIndex = menuScene.objects.size();
        menuScene.objects.emplace_back(cursor);
        updateMenuSelection(0);
    }
    engine::math::Mat4 menuPanelTransform() const {
        return engine::math::multiply(engine::math::translation({1.3f, 1.65f, GameConfig::kPlayerZ - 1.0f}),
                                      engine::math::rotateY(engine::math::radians(-12)));
    }
    void updateMenuSelection(int selection) {
        using namespace engine::math;
        const Mat4 panel = menuPanelTransform();
        const float centerY = .36f - .40f * static_cast<float>(std::clamp(selection, 0, 3));
        constexpr std::array<float, 4> textHalfWidths = {.414f, .738f, .738f, .414f};
        auto& cursor = std::get<engine::graphics::ModelInstance>(menuScene.objects.at(selectionCursorIndex));
        cursor.transform = multiply(panel, translation({-textHalfWidths[std::clamp(selection, 0, 3)] - .12f,
                                                        centerY, 0}));
    }
    void updateMenuPlayer(float time, int selection) {
        menuPlayer.updateMenu(time);
        for (auto& object : menuScene.objects) {
            auto* character = std::get_if<engine::graphics::ModelInstance>(&object);
            if (!character || character->model != &assets.model(game::ModelId::PlayerCat))
                continue;
            const auto& pose = menuPlayer.pose();
            const float ground = pose.boundsCenter().y - pose.boundsHalfExtent().y;
            const auto anchor = menuPlayer.anchor();
            character->pose = &pose;
            character->transform = engine::math::multiply(
                engine::math::translation({GameConfig::kMenuCatX, -ground + menuPlayer.lift(), GameConfig::kMenuCatZ}),
                engine::math::multiply(
                    engine::math::rotateY(-engine::math::kPi * .5f + engine::math::radians(10)),
                    engine::math::translation({-anchor.x, 0, -anchor.z})));
        }
        updateMenuSelection(selection);
    }
};
Renderer::Renderer(std::filesystem::path assetRoot) : assetRoot_(std::move(assetRoot)) {
    if (assetRoot_.empty()) {
        assetRoot_ = SUBWAY_ASSET_DIR;
    }
}
Renderer::~Renderer() = default;
bool Renderer::initialize() {
    if (impl_) {
        return true;
    }
    try {
        impl_ = std::make_unique<Impl>(assetRoot_);
        return true;
    } catch (const std::exception& error) {
        std::cerr << "Renderer initialization failed: " << error.what() << '\n';
        return false;
    }
}
void Renderer::resizeViewport() const {
    if (impl_) {
        impl_->drawable = impl_->window.drawableSize();
    }
}
void Renderer::setWindowTitle(const std::string& title) const {
    if (impl_) {
        impl_->window.setTitle(title);
    }
}
void Renderer::toggleRetroMode() const {
    if (impl_) {
        impl_->graphics.setRetroMode(!impl_->graphics.retroMode());
    }
}
void Renderer::toggleDitheredShadows() const {
    if (impl_) {
        impl_->graphics.setPsxDitheredShadows(!impl_->graphics.psxDitheredShadows());
    }
}
bool Renderer::ditheredShadows() const {
    return impl_ && impl_->graphics.psxDitheredShadows();
}
void Renderer::toggleDistanceFog() const {
    if (impl_) {
        impl_->graphics.setPsxDistanceFog(!impl_->graphics.psxDistanceFog());
    }
}
bool Renderer::distanceFog() const {
    return impl_ && impl_->graphics.psxDistanceFog();
}
engine::graphics::RenderMode Renderer::renderMode() const {
    return impl_ ? impl_->graphics.mode() : engine::graphics::RenderMode::Normal;
}
void Renderer::renderMenu(const float time, const int selection) const {
    if (!impl_) {
        return;
    }
    resizeViewport();
    if (impl_->drawable.width <= 0 || impl_->drawable.height <= 0) {
        return;
    }
    // The street stays frozen while the separate menu pose gives the cat a subtle foot tap.
    impl_->updateMenuPlayer(time, selection);
    impl_->graphics.render(impl_->menuScene, impl_->drawable);
    impl_->window.swap();
}
void Renderer::render(const float playerX, const float roadScroll, const float gameTime,
                      [[maybe_unused]] const int score, const std::span<const Entity> entities,
                      const std::span<const CollectEffect> collectEffects, const float effectRemainder,
                      const float nightBlend, const float playerSpeed, const float cameraBlend) const {
    if (!impl_) {
        return;
    }
    resizeViewport();
    if (impl_->drawable.width <= 0 || impl_->drawable.height <= 0) {
        return;
    }
    impl_->graphics.render(impl_->world.build(playerX, roadScroll, gameTime, entities, collectEffects, effectRemainder,
                                              nightBlend, playerSpeed, cameraBlend),
                           impl_->drawable);
    impl_->window.swap();
}
