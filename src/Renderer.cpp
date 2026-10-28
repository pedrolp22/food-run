#include "Renderer.h"
#include "GameConfig.h"
#include "engine/assets/GltfLoader.h"
#include "engine/graphics/SceneRenderer.h"
#include "engine/platform/Window.h"
#include "game/WorldScene.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
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

// Five-by-seven pixel glyphs. Neighboring pixels meet to form a single, solid letter.
constexpr std::array<std::array<std::string_view, 7>, 26> kMenuGlyphs = {{
    {"01110", "10001", "10001", "11111", "10001", "10001", "10001"}, // A
    {"11110", "10001", "10001", "11110", "10001", "10001", "11110"}, // B
    {"01111", "10000", "10000", "10000", "10000", "10000", "01111"}, // C
    {"11110", "10001", "10001", "10001", "10001", "10001", "11110"}, // D
    {"11111", "10000", "10000", "11110", "10000", "10000", "11111"}, // E
    {"11111", "10000", "10000", "11110", "10000", "10000", "10000"}, // F
    {"01111", "10000", "10000", "10111", "10001", "10001", "01111"}, // G
    {"10001", "10001", "10001", "11111", "10001", "10001", "10001"}, // H
    {"11111", "00100", "00100", "00100", "00100", "00100", "11111"}, // I
    {"00111", "00010", "00010", "00010", "10010", "10010", "01100"}, // J
    {"10001", "10010", "10100", "11000", "10100", "10010", "10001"}, // K
    {"10000", "10000", "10000", "10000", "10000", "10000", "11111"}, // L
    {"10001", "11011", "10101", "10101", "10001", "10001", "10001"}, // M
    {"10001", "11001", "10101", "10011", "10001", "10001", "10001"}, // N
    {"01110", "10001", "10001", "10001", "10001", "10001", "01110"}, // O
    {"11110", "10001", "10001", "11110", "10000", "10000", "10000"}, // P
    {"01110", "10001", "10001", "10001", "10101", "10010", "01101"}, // Q
    {"11110", "10001", "10001", "11110", "10100", "10010", "10001"}, // R
    {"01111", "10000", "10000", "01110", "00001", "00001", "11110"}, // S
    {"11111", "00100", "00100", "00100", "00100", "00100", "00100"}, // T
    {"10001", "10001", "10001", "10001", "10001", "10001", "01110"}, // U
    {"10001", "10001", "10001", "10001", "10001", "01010", "00100"}, // V
    {"10001", "10001", "10001", "10101", "10101", "10101", "01010"}, // W
    {"10001", "10001", "01010", "00100", "01010", "10001", "10001"}, // X
    {"10001", "10001", "01010", "00100", "00100", "00100", "00100"}, // Y
    {"11111", "00001", "00010", "00100", "01000", "10000", "11111"}, // Z
}};
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

void appendMenuText(PrimitiveData& outline, PrimitiveData& face, std::string_view text, float top,
                    float pixelSize) {
    const float width = (static_cast<float>(text.size()) * 6 - 1) * pixelSize;
    const float left = -width * .5f;
    for (std::size_t character = 0; character < text.size(); ++character) {
        const int letter = text[character] - 'A';
        if (letter < 0 || letter >= static_cast<int>(kMenuGlyphs.size()))
            continue;
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if (kMenuGlyphs[letter][row][column] != '1')
                    continue;
                const float x = left + (static_cast<float>(character) * 6 + column + .5f) * pixelSize;
                const float y = top - (row + .5f) * pixelSize;
                // A dark, extruded silhouette sits behind the light front of each letter.
                appendBlock(outline, {x, y, 0}, {pixelSize * 1.2f, pixelSize * 1.2f, .15f});
                appendBlock(face, {x, y, .085f}, {pixelSize, pixelSize, .04f});
            }
        }
    }
}

ModelData makeMenuLettering(std::string_view label, float pixelSize) {
    ModelData data;
    PrimitiveData outline, face;
    outline.material.baseColor = {.075f, .035f, .018f, 1};
    outline.material.roughness = .72f;
    face.material.baseColor = {1.0f, .82f, .40f, 1};
    face.material.emissiveFactor = {.20f, .12f, .035f};
    face.material.roughness = .6f;
    appendMenuText(outline, face, label, pixelSize * 3.5f, pixelSize);
    data.primitives.push_back(std::move(outline));
    data.primitives.push_back(std::move(face));
    return data;
}

ModelData makeMenuCursor() {
    ModelData data;
    PrimitiveData cursor;
    cursor.material.baseColor = {1.0f, .74f, .30f, 1};
    cursor.material.emissiveFactor = {.14f, .065f, .012f};
    cursor.material.roughness = .38f;
    appendBlock(cursor, {-.035f, .075f, .12f}, {.07f, .07f, .09f});
    appendBlock(cursor, {.035f, 0, .12f}, {.07f, .07f, .09f});
    appendBlock(cursor, {-.035f, -.075f, .12f}, {.07f, .07f, .09f});
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
    engine::graphics::Model menuTitle;
    std::array<engine::graphics::Model, 4> menuLabels;
    engine::graphics::Model menuCursor;
    engine::graphics::Scene menuScene;
    engine::graphics::Size drawable;
    std::size_t selectionCursorIndex = 0;
    std::size_t menuTitleIndex = 0;
    std::array<std::size_t, 4> menuLabelIndices{};
    std::array<float, 4> menuHover{};
    float previousMenuTime = -1;
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
          menuPlayer(*assets.model(game::ModelId::PlayerCat).animationData()),
          menuTitle(makeMenuLettering("SUBWAY MONEY", .043f)),
          menuLabels{engine::graphics::Model(makeMenuLettering("PLAY", .068f)),
                     engine::graphics::Model(makeMenuLettering("OPTIONS", .068f)),
                     engine::graphics::Model(makeMenuLettering("CREDITS", .068f)),
                     engine::graphics::Model(makeMenuLettering("EXIT", .068f))},
          menuCursor(makeMenuCursor()),
          menuScene(world.build(0, 0, 0, {}, {})), drawable(window.drawableSize()) {
        menuScene.camera = game::menuCamera(aspect());
        // The menu is close to the camera, so concentrate shadow-map detail on the street around it.
        menuScene.light.direction = {GameConfig::kMenuLightDirection[0], GameConfig::kMenuLightDirection[1],
                                     GameConfig::kMenuLightDirection[2]};
        menuScene.light.shadow.center = {0, 1, GameConfig::kPlayerZ};
        menuScene.light.shadow.halfWidth = 12;
        menuScene.light.shadow.halfHeight = 12;
        engine::graphics::ModelInstance text;
        text.pbrSurface = true;
        text.model = &menuTitle;
        menuTitleIndex = menuScene.objects.size();
        menuScene.objects.emplace_back(text);
        for (std::size_t i = 0; i < menuLabels.size(); ++i) {
            text.model = &menuLabels[i];
            menuLabelIndices[i] = menuScene.objects.size();
            menuScene.objects.emplace_back(text);
        }
        engine::graphics::ModelInstance cursor;
        cursor.model = &menuCursor;
        cursor.pbrSurface = true;
        selectionCursorIndex = menuScene.objects.size();
        menuScene.objects.emplace_back(cursor);
        updateMenuSelection(0, -1, 0);
    }
    float aspect() const {
        return drawable.height > 0 ? static_cast<float>(drawable.width) / drawable.height : 1.0f;
    }
    engine::math::Mat4 menuPanelTransform() const {
        using namespace engine::math;
        // Put the text alongside the cat, angled toward the camera instead of on a screen-space plane.
        const float distance = (menuScene.camera.position.z - GameConfig::kPlayerZ) * .85f;
        const auto& camera = menuScene.camera;
        const Vec3 forward = normalize(camera.target - camera.position);
        const Vec3 right = normalize(cross(forward, camera.up));
        const Vec3 up = cross(right, forward);
        const float halfHeight = distance * std::tan(radians(camera.verticalFov) * .5f);
        const float halfWidth = halfHeight * aspect();
        const float size = std::min(halfWidth * .90f / 3.2f, halfHeight * 1.60f / 3.5f);
        const Vec3 center{camera.position.x + forward.x * distance + right.x * halfWidth * .50f,
                          camera.position.y + forward.y * distance + up.y * halfHeight * .04f,
                          camera.position.z + forward.z * distance + up.z * halfHeight * .04f};
        const float pitch = std::atan2(forward.y, -forward.z);
        return multiply(translation(center),
                        multiply(rotateX(pitch), multiply(rotateY(radians(-18)), scale({size, size, size}))));
    }
    static float labelX(int index) { return (static_cast<float>(index) - 1.5f) * .10f; }
    static float labelY(int index) { return .662f - .66f * static_cast<float>(index); }
    static float labelHalfWidth(int index) {
        constexpr std::array<float, 4> widths = {.782f, 1.394f, 1.394f, .782f};
        return widths[index];
    }
    void updateMenuSelection(int selection, int pressed, float time) {
        using namespace engine::math;
        const Mat4 panel = menuPanelTransform();
        const int selected = std::clamp(selection, 0, 3);
        const float dt = previousMenuTime < 0 ? 1.0f : std::clamp(time - previousMenuTime, 0.0f, .1f);
        previousMenuTime = time;
        auto& title = std::get<engine::graphics::ModelInstance>(menuScene.objects.at(menuTitleIndex));
        title.transform = multiply(panel, translation({0, 1.6995f, 0}));
        for (int i = 0; i < 4; ++i) {
            const float target = i == selected ? 1.0f : 0.0f;
            menuHover[i] = std::lerp(menuHover[i], target, 1 - std::exp(-12 * dt));
            const bool down = i == pressed;
            const float s = 1 + .06f * menuHover[i] - (down ? .09f : 0);
            const Mat4 offset = multiply(translation({labelX(i), labelY(i), .07f * menuHover[i] - (down ? .14f : 0)}),
                                         scale({s, s, s}));
            auto& item = std::get<engine::graphics::ModelInstance>(menuScene.objects.at(menuLabelIndices[i]));
            item.transform = multiply(panel, offset);
            item.emissiveScale = 1 + .8f * menuHover[i];
        }
        auto& cursor = std::get<engine::graphics::ModelInstance>(menuScene.objects.at(selectionCursorIndex));
        cursor.transform = multiply(panel, translation({labelX(selected) - labelHalfWidth(selected) - .23f,
                                                        labelY(selected), .08f}));
    }
    int menuHitTest(int x, int y) const {
        using namespace engine::math;
        int width = 0, height = 0;
        SDL_GetWindowSize(window.nativeHandle(), &width, &height);
        if (width <= 0 || height <= 0 || x < 0 || y < 0 || x >= width || y >= height)
            return -1;
        const Vec2 point{2.0f * x / width - 1, 1 - 2.0f * y / height};
        const Mat4 vp = menuScene.camera.viewProjection(aspect());
        const auto project = [&](const Mat4& transform, Vec3 p) {
            const Mat4 m = multiply(vp, transform);
            const float w = m.values[3] * p.x + m.values[7] * p.y + m.values[11] * p.z + m.values[15];
            const Vec3 v = transformPoint(m, p);
            return Vec2{v.x / w, v.y / w};
        };
        const auto side = [](Vec2 a, Vec2 b, Vec2 p) {
            return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
        };
        for (int i = 0; i < 4; ++i) {
            const auto& item = std::get<engine::graphics::ModelInstance>(menuScene.objects.at(menuLabelIndices[i]));
            const float half = labelHalfWidth(i) + .12f;
            const std::array<Vec2, 4> corners = {project(item.transform, {-half, -.31f, .105f}),
                                                  project(item.transform, {half, -.31f, .105f}),
                                                  project(item.transform, {half, .31f, .105f}),
                                                  project(item.transform, {-half, .31f, .105f})};
            bool positive = false, negative = false;
            for (int edge = 0; edge < 4; ++edge) {
                const float cross = side(corners[edge], corners[(edge + 1) % 4], point);
                positive |= cross > 0;
                negative |= cross < 0;
            }
            if (!(positive && negative))
                return i;
        }
        return -1;
    }
    void updateMenuPlayer(float time, int selection, int pressed) {
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
                    engine::math::rotateY(engine::math::radians(GameConfig::kMenuCharacterYawDegrees)),
                    engine::math::translation({-anchor.x, 0, -anchor.z})));
        }
        updateMenuSelection(selection, pressed, time);
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
int Renderer::menuHitTest(int x, int y) const {
    return impl_ ? impl_->menuHitTest(x, y) : -1;
}
void Renderer::renderMenu(const float time, const int selection, const int pressed) const {
    if (!impl_) {
        return;
    }
    resizeViewport();
    if (impl_->drawable.width <= 0 || impl_->drawable.height <= 0) {
        return;
    }
    // The street stays frozen while the separate menu pose gives the cat a subtle foot tap.
    impl_->menuScene.camera = game::menuCamera(impl_->aspect());
    impl_->updateMenuPlayer(time, selection, pressed);
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
                                              nightBlend, playerSpeed, cameraBlend, impl_->aspect()),
                           impl_->drawable);
    impl_->window.swap();
}
