#include "Renderer.h"
#include "GameConfig.h"
#include "engine/assets/GltfLoader.h"
#include "engine/assets/SpriteGeometry.h"
#include "engine/graphics/SceneRenderer.h"
#include "engine/platform/Window.h"
#include "game/MenuIconAnimation.h"
#include "game/WorldScene.h"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
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
constexpr std::array<std::array<std::string_view, 7>, 26> kMenuLowercaseGlyphs = {{
    {"00000", "00000", "01110", "00001", "01111", "10001", "01111"}, // a
    {"10000", "10000", "10110", "11001", "10001", "10001", "11110"}, // b
    {"00000", "00000", "01111", "10000", "10000", "10000", "01111"}, // c
    {"00001", "00001", "01101", "10011", "10001", "10001", "01111"}, // d
    {"00000", "00000", "01110", "10001", "11111", "10000", "01111"}, // e
    {"00110", "01001", "01000", "11100", "01000", "01000", "01000"}, // f
    {"00000", "01111", "10001", "10001", "01111", "00001", "01110"}, // g
    {"10000", "10000", "10110", "11001", "10001", "10001", "10001"}, // h
    {"00100", "00000", "01100", "00100", "00100", "00100", "01110"}, // i
    {"00010", "00000", "00110", "00010", "00010", "10010", "01100"}, // j
    {"10000", "10000", "10010", "10100", "11000", "10100", "10010"}, // k
    {"01100", "00100", "00100", "00100", "00100", "00100", "01110"}, // l
    {"00000", "00000", "11010", "10101", "10101", "10101", "10101"}, // m
    {"00000", "00000", "10110", "11001", "10001", "10001", "10001"}, // n
    {"00000", "00000", "01110", "10001", "10001", "10001", "01110"}, // o
    {"00000", "11110", "10001", "10001", "11110", "10000", "10000"}, // p
    {"00000", "01111", "10001", "10001", "01111", "00001", "00001"}, // q
    {"00000", "00000", "10110", "11001", "10000", "10000", "10000"}, // r
    {"00000", "00000", "01111", "10000", "01110", "00001", "11110"}, // s
    {"00100", "00100", "11111", "00100", "00100", "00101", "00010"}, // t
    {"00000", "00000", "10001", "10001", "10001", "10011", "01101"}, // u
    {"00000", "00000", "10001", "10001", "10001", "01010", "00100"}, // v
    {"00000", "00000", "10001", "10001", "10101", "10101", "01010"}, // w
    {"00000", "00000", "10001", "01010", "00100", "01010", "10001"}, // x
    {"00000", "10001", "10001", "10001", "01111", "00001", "01110"}, // y
    {"00000", "00000", "11111", "00010", "00100", "01000", "11111"}, // z
}};
constexpr std::array<std::array<std::string_view, 7>, 10> kMenuDigits = {{
    {"01110", "10001", "10011", "10101", "11001", "10001", "01110"},
    {"00100", "01100", "00100", "00100", "00100", "00100", "01110"},
    {"01110", "10001", "00001", "00010", "00100", "01000", "11111"},
    {"11110", "00001", "00001", "01110", "00001", "00001", "11110"},
    {"00010", "00110", "01010", "10010", "11111", "00010", "00010"},
    {"11111", "10000", "10000", "11110", "00001", "00001", "11110"},
    {"01110", "10000", "10000", "11110", "10001", "10001", "01110"},
    {"11111", "00001", "00010", "00100", "01000", "01000", "01000"},
    {"01110", "10001", "10001", "01110", "10001", "10001", "01110"},
    {"01110", "10001", "10001", "01111", "00001", "00001", "01110"},
}};
constexpr std::array<std::string_view, 7> kMenuLess = {
    "00001", "00010", "00100", "01000", "00100", "00010", "00001"};
constexpr std::array<std::string_view, 7> kMenuGreater = {
    "10000", "01000", "00100", "00010", "00100", "01000", "10000"};
constexpr std::array<std::string_view, 7> kMenuPercent = {
    "11001", "11010", "00010", "00100", "01000", "01011", "10011"};
constexpr std::array<std::string_view, 7> kMenuColon = {
    "00000", "00100", "00100", "00000", "00100", "00100", "00000"};
constexpr std::array<std::string_view, 7> kMenuExclamation = {
    "00100", "00100", "00100", "00100", "00100", "00000", "00100"};
constexpr std::array<std::string_view, 7> kMenuSlash = {
    "00001", "00010", "00010", "00100", "01000", "01000", "10000"};
constexpr std::array<std::string_view, 7> kMenuDash = {
    "00000", "00000", "00000", "01110", "00000", "00000", "00000"};

const std::array<std::string_view, 7>* menuGlyph(char character) {
    if (character >= 'A' && character <= 'Z')
        return &kMenuGlyphs[character - 'A'];
    if (character >= 'a' && character <= 'z')
        return &kMenuLowercaseGlyphs[character - 'a'];
    if (character >= '0' && character <= '9')
        return &kMenuDigits[character - '0'];
    switch (character) {
    case ':': return &kMenuColon;
    case '!': return &kMenuExclamation;
    case '/': return &kMenuSlash;
    case '-': return &kMenuDash;
    case '<': return &kMenuLess;
    case '>': return &kMenuGreater;
    case '%': return &kMenuPercent;
    default: return nullptr;
    }
}

struct MenuTextLine {
    std::string_view text;
    float pixelSize;
    float centerY;
    float centerX = 0;
};
constexpr float kDeveloperNameLeft = -.90f;
constexpr float kDeveloperPixelSize = .046f;
constexpr float developerNameCenter(std::string_view name) {
    return kDeveloperNameLeft + (name.size() * 6 - 1) * kDeveloperPixelSize * .5f;
}
constexpr std::array<MenuTextLine, 7> kCreditsLines = {{
    {"Developed by:", .036f, 1.13f},
    {"DedThred", kDeveloperPixelSize, .73f, developerNameCenter("DedThred")},
    {"Lorenzzo", kDeveloperPixelSize, .37f, developerNameCenter("Lorenzzo")},
    {"rdhd", kDeveloperPixelSize, .01f, developerNameCenter("rdhd")},
    {"Main character", .034f, -.64f},
    {"model by", .034f, -.91f},
    {"AzDrekstar", .046f, -1.27f},
}};
constexpr float kCreditsBackY = -1.90f;
constexpr float kCreditsBackPixelSize = .055f;
constexpr std::array<std::string_view, 3> kDeveloperIconFiles = {"elio.png", "lorenzo.png", "pedro.png"};
// These exports have an opaque purple matte. Recover only its border-connected pixels;
// Texture keeps authored alpha untouched when the files are exported with transparency.
constexpr std::array<std::uint8_t, 3> kDeveloperMatte = {69, 40, 60};
constexpr float kDeveloperIconX = -1.22f, kDeveloperIconSize = .34f;
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

void appendMenuText(PrimitiveData& outline, PrimitiveData& face, std::string_view text, float top, float pixelSize,
                    float centerX = 0) {
    const float width = (static_cast<float>(text.size()) * 6 - 1) * pixelSize;
    const float left = centerX - width * .5f;
    for (std::size_t character = 0; character < text.size(); ++character) {
        const auto* glyph = menuGlyph(text[character]);
        if (!glyph)
            continue;
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((*glyph)[row][column] != '1')
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

ModelData makeMenuLettering(std::span<const MenuTextLine> lines) {
    ModelData data;
    PrimitiveData outline, face;
    outline.material.baseColor = {.075f, .035f, .018f, 1};
    outline.material.roughness = .72f;
    face.material.baseColor = {1.0f, .82f, .40f, 1};
    face.material.emissiveFactor = {.20f, .12f, .035f};
    face.material.roughness = .6f;
    for (const auto& line : lines)
        appendMenuText(outline, face, line.text, line.centerY + line.pixelSize * 3.5f, line.pixelSize, line.centerX);
    data.primitives.push_back(std::move(outline));
    data.primitives.push_back(std::move(face));
    return data;
}

ModelData makeMenuLettering(std::string_view label, float pixelSize) {
    const MenuTextLine line{label, pixelSize, 0};
    return makeMenuLettering(std::span{&line, 1});
}

ModelData makeControlsHelpText() {
    constexpr std::array lines = {MenuTextLine{"GAME CONTROLS", .009f, -.77f},
                                  MenuTextLine{"Press <- / -> or A / D to move!", .0048f, -.875f}};
    return makeMenuLettering(lines);
}

ModelData makeGameOverPanel() {
    ModelData data;
    PrimitiveData panel;
    panel.material.baseColor = {.004f, .008f, .018f, 1};
    panel.material.emissiveFactor = {.003f, .006f, .014f};
    panel.material.roughness = 1;
    panel.vertices = {
        -.5f, -.5f, 0, 0, 0, 1, 0, 0,
         .5f, -.5f, 0, 0, 0, 1, 1, 0,
         .5f,  .5f, 0, 0, 0, 1, 1, 1,
        -.5f,  .5f, 0, 0, 0, 1, 0, 1,
    };
    panel.indices = {0, 1, 2, 0, 2, 3};
    data.primitives.push_back(std::move(panel));
    return data;
}

ModelData makeDeveloperIcon(const engine::assets::SpriteGeometry& geometry, const std::filesystem::path& path) {
    ModelData data;
    PrimitiveData front, sides;
    for (auto* primitive : {&front, &sides}) {
        primitive->material.baseTexture = path;
        primitive->material.baseTextureNearest = true;
        primitive->material.baseTextureOpaqueBorderColor = kDeveloperMatte;
        primitive->material.alphaCutoff = .1f;
        primitive->material.metallic = 0;
    }
    front.material.roughness = .36f;
    sides.material.roughness = .78f;
    sides.material.baseColor = {.60f, .60f, .60f, 1};
    // SpriteGeometry uses separate quads for its front, back and each contour wall.
    for (std::size_t i = 0; i < geometry.vertices.size(); i += 4 * engine::assets::SpriteGeometry::stride) {
        const bool isFront = geometry.vertices[i + 7] > .5f;
        auto& primitive = isFront ? front : sides;
        const auto first = static_cast<unsigned int>(primitive.vertices.size() / engine::assets::kModelVertexStride);
        // Model textures use top-first image UVs; the extrusion's alpha mask is bottom-first.
        for (int corner = 0; corner < 4; ++corner) {
            const auto* vertex = geometry.vertices.data() + i + corner * engine::assets::SpriteGeometry::stride;
            // A slight dome in the front normals gives a broad, soft varnish highlight.
            const Vec3 normal = isFront ? engine::math::normalize({vertex[0] * .55f, vertex[1] * .55f, 1})
                                        : Vec3{vertex[5], vertex[6], vertex[7]};
            primitive.vertices.insert(primitive.vertices.end(), {vertex[0], vertex[1], vertex[2],
                normal.x, normal.y, normal.z, vertex[3], 1 - vertex[4]});
        }
        primitive.indices.insert(primitive.indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
    }
    data.primitives.push_back(std::move(front));
    data.primitives.push_back(std::move(sides));
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
enum class GraphicsControl {
    None, DisplayMode, Quality, RenderScale, AntiAliasing, Vsync, Shadows, Occlusion, Reflections, MaterialDetail,
    FoliageParticles, RetroMode, DitheredShadows, DistanceFog
};
struct MenuItemDefinition {
    std::string_view label;
    GraphicsControl control = GraphicsControl::None;
};
std::span<const MenuItemDefinition> menuDefinitions(Renderer::MenuPage page) {
    static constexpr std::array main = {MenuItemDefinition{"PLAY"}, MenuItemDefinition{"OPTIONS"},
                                        MenuItemDefinition{"CREDITS"}, MenuItemDefinition{"EXIT"}};
    static constexpr std::array credits = {MenuItemDefinition{"BACK"}};
    static constexpr std::array gameOver = {MenuItemDefinition{"Play Again"}, MenuItemDefinition{"Main Menu"}};
    static constexpr std::array options = {
        MenuItemDefinition{"Display mode", GraphicsControl::DisplayMode},
        MenuItemDefinition{"Quality", GraphicsControl::Quality},
        MenuItemDefinition{"Render scale", GraphicsControl::RenderScale},
        MenuItemDefinition{"Anti aliasing", GraphicsControl::AntiAliasing},
        MenuItemDefinition{"VSync", GraphicsControl::Vsync},
        MenuItemDefinition{"Shadows", GraphicsControl::Shadows},
        MenuItemDefinition{"Occlusion", GraphicsControl::Occlusion},
        MenuItemDefinition{"Reflections", GraphicsControl::Reflections},
        MenuItemDefinition{"Material detail", GraphicsControl::MaterialDetail},
        MenuItemDefinition{"Particles", GraphicsControl::FoliageParticles},
        MenuItemDefinition{"Retro mode", GraphicsControl::RetroMode},
        MenuItemDefinition{"Shadow dither", GraphicsControl::DitheredShadows},
        MenuItemDefinition{"Distance fog", GraphicsControl::DistanceFog},
        MenuItemDefinition{"BACK"}
    };
    switch (page) {
    case Renderer::MenuPage::Main: return main;
    case Renderer::MenuPage::Credits: return credits;
    case Renderer::MenuPage::Options: return options;
    case Renderer::MenuPage::GameOver: return gameOver;
    }
    return main;
}
std::string_view menuTitle(Renderer::MenuPage page) {
    switch (page) {
    case Renderer::MenuPage::Main: return "FOOD RUN";
    case Renderer::MenuPage::Credits: return "CREDITS";
    case Renderer::MenuPage::Options: return "OPTIONS";
    case Renderer::MenuPage::GameOver: return "Game Over!";
    }
    return "FOOD RUN";
}
constexpr std::size_t kMenuItemCapacity = 14;
constexpr std::size_t kExtraFirstItem = 10;
constexpr float kOptionPixelSize = .030f;
constexpr float kMainHeaderPixelSize = .043f;
constexpr float kGameOverHeaderPixelSize = .038f;
constexpr float kOptionLeft = -1.73f;
constexpr float kOptionRight = 1.73f;
constexpr float kOptionRowStep = .32f;
constexpr float kOptionExtraGap = .35f;
float optionRowY(std::size_t index) {
    return 1.62f - kOptionRowStep * index - (index >= kExtraFirstItem ? kOptionExtraGap : 0);
}
float textHalfWidth(std::string_view text, float pixelSize) {
    return (text.size() * 6 - 1) * pixelSize * .5f;
}
} // namespace

struct Renderer::Impl {
    // All GPU assets must die before the window's GL context.
    engine::platform::Window window;
    std::filesystem::path settingsFile;
    game::GraphicsSettings preferences;
    engine::graphics::SceneRenderer graphics;
    game::GameAssets assets;
    game::WorldScene world;
    game::PlayerCharacter menuPlayer;
    engine::graphics::Model title;
    engine::graphics::Model creditsLettering;
    engine::graphics::Model extraHeading;
    engine::graphics::Model menuCursor;
    engine::graphics::Model scoreText;
    engine::graphics::Model fitFoodText;
    engine::graphics::Model gameOverScoreText;
    engine::graphics::Model gameOverFitFoodText;
    engine::graphics::Model gameOverPanel;
    engine::graphics::Model controlsHelpText;
    std::string scoreTextValue;
    std::string fitFoodTextValue;
    std::string gameOverScoreTextValue;
    std::string gameOverFitFoodTextValue;
    std::vector<engine::graphics::DrawCommand> hudObjects;
    struct MenuItem {
        engine::graphics::Model label, value;
        std::string valueText;
        std::size_t labelIndex = 0, valueIndex = 0;
        float halfWidth = 0, centerY = 0, hover = 0;
        engine::math::Mat4 transform = engine::math::identity();
    };
    std::array<MenuItem, kMenuItemCapacity> menuItems;
    struct DeveloperIcon {
        engine::graphics::Texture silhouette;
        engine::assets::SpriteGeometry geometry;
        engine::graphics::Model model;
        game::MenuIconAnimation animation;
        std::size_t objectIndex = 0;
        engine::math::Mat4 transform = engine::math::identity();
    };
    std::array<DeveloperIcon, kDeveloperIconFiles.size()> developerIcons;
    engine::graphics::Scene menuScene;
    engine::graphics::Size drawable;
    std::size_t cursorIndex = 0, titleIndex = 0, creditsIndex = 0, extraIndex = 0;
    Renderer::MenuPage menuPage = Renderer::MenuPage::Main;
    float previousMenuTime = -1;

    Impl(const std::filesystem::path& root, const std::filesystem::path& config, bool visible)
        : window({.title = GameConfig::kWindowTitle, .size = {.width = 720, .height = 1280}, .visible = visible,
                  .borderless = game::GraphicsSettings::load(config).displayMode == game::DisplayMode::Borderless}),
          settingsFile(config), preferences(game::GraphicsSettings::load(config)), graphics(preferences.renderSettings()),
          assets(root, GameConfig::kMipmappedTextures), world(assets),
          menuPlayer(*assets.model(game::ModelId::PlayerCat).animationData()),
          creditsLettering(makeMenuLettering(kCreditsLines)), extraHeading(makeMenuLettering("EXTRA", .030f)),
          menuCursor(makeMenuCursor()),
          menuScene(world.build(0, 0, 0, {}, {})), drawable(window.drawableSize()) {
        controlsHelpText = engine::graphics::Model(makeControlsHelpText());
        gameOverPanel = engine::graphics::Model(makeGameOverPanel());
        hudObjects.reserve(5);
        window.setVsync(preferences.vsync);
        preferences.vsync = window.vsync();
        graphics.setRetroMode(preferences.retroMode);
        world.setFoliageParticles(preferences.foliageParticles);
        menuScene.camera = game::menuCamera(menuAspect());
        menuScene.light.direction = {GameConfig::kMenuLightDirection[0], GameConfig::kMenuLightDirection[1],
                                     GameConfig::kMenuLightDirection[2]};
        menuScene.light.shadow.center = {0, 1, GameConfig::kPlayerZ};
        menuScene.light.shadow.halfWidth = 12;
        menuScene.light.shadow.halfHeight = 12;
        engine::graphics::ModelInstance text;
        text.pbrSurface = true;
        titleIndex = menuScene.objects.size();
        menuScene.objects.emplace_back(text);
        creditsIndex = menuScene.objects.size();
        menuScene.objects.emplace_back(text);
        extraIndex = menuScene.objects.size();
        menuScene.objects.emplace_back(text);
        for (std::size_t i = 0; i < developerIcons.size(); ++i) {
            auto& icon = developerIcons[i];
            const auto path = root / "textures" / "dev_icons" / kDeveloperIconFiles[i];
            icon.silhouette = engine::graphics::Texture(path,
                {engine::graphics::TextureFilter::Nearest, false, true, true, kDeveloperMatte});
            const auto size = icon.silhouette.size();
            icon.geometry = engine::assets::extrudeSprite(icon.silhouette.alphaMask(), size.width, size.height);
            icon.model = engine::graphics::Model(makeDeveloperIcon(icon.geometry, path));
            icon.objectIndex = menuScene.objects.size();
            menuScene.objects.emplace_back(text);
        }
        for (auto& item : menuItems) {
            item.labelIndex = menuScene.objects.size();
            menuScene.objects.emplace_back(text);
            item.valueIndex = menuScene.objects.size();
            menuScene.objects.emplace_back(text);
        }
        text.model = &menuCursor;
        cursorIndex = menuScene.objects.size();
        menuScene.objects.emplace_back(text);
        buildMenuPage();
        updateMenuSelection(0, -1, 0);
        saveSettings();
    }
    void saveSettings() const {
        if (!preferences.save(settingsFile))
            std::cerr << "Could not save graphics settings to " << settingsFile << '\n';
    }
    void updateScoreText(int score, int health, int healthyFoods, float controlsHelpAlpha, float hudOpacity = 1) {
        hudObjects.clear();
        const float aspect = preferences.retroMode
                                 ? static_cast<float>(GameConfig::kPsxInternalWidth) / GameConfig::kPsxInternalHeight
                                 : drawable.height > 0 ? static_cast<float>(drawable.width) / drawable.height : 1;
        constexpr float margin = .025f;
        constexpr float heartSize = .09f;
        constexpr float heartGap = .025f;
        const float heartY = 1 - margin - heartSize * .5f;
        health = std::clamp(health, 0, GameConfig::kMaxHealth);
        for (int i = 0; i < GameConfig::kMaxHealth; ++i) {
            const auto& texture = assets.texture(i < health ? game::TextureId::HeartRed : game::TextureId::HeartLost);
            engine::graphics::Sprite heart;
            heart.texture = &texture;
            heart.aspect = texture.aspect();
            heart.opacity = hudOpacity;
            heart.model = multiply(translation({-aspect + margin + heartSize * .5f + i * (heartSize + heartGap),
                                                heartY, 0}), scale({heartSize * heart.aspect, heartSize, 1}));
            hudObjects.emplace_back(heart);
        }
        const std::string text = "Score: " + std::to_string(score);
        if (text != scoreTextValue) {
            scoreText = engine::graphics::Model(makeMenuLettering(text, .012f));
            scoreTextValue = text;
        }
        const std::string fitFood = "Fit Food: " + std::to_string(healthyFoods);
        if (fitFood != fitFoodTextValue) {
            fitFoodText = engine::graphics::Model(makeMenuLettering(fitFood, .012f));
            fitFoodTextValue = fitFood;
        }
        constexpr float pixelSize = .012f;
        constexpr float iconSize = .068f;
        constexpr float iconGap = .018f;
        const float textLeft = -aspect + margin + iconSize + iconGap;
        const float scoreY = 1 - margin - heartSize - .025f - 3.5f * pixelSize;
        const auto appendMetric = [&](const engine::graphics::Model& model, const std::string& label,
                                      game::TextureId icon, float y) {
            const auto& texture = assets.texture(icon);
            engine::graphics::Sprite marker;
            marker.texture = &texture;
            marker.aspect = texture.aspect();
            marker.opacity = hudOpacity;
            marker.model = multiply(translation({-aspect + margin + iconSize * .5f, y, 0}),
                                    scale({iconSize * marker.aspect, iconSize, 1}));
            hudObjects.emplace_back(marker);
            engine::graphics::ModelInstance textModel;
            textModel.model = &model;
            textModel.pbrSurface = true;
            textModel.opacity = hudOpacity;
            textModel.transform = translation({textLeft + textHalfWidth(label, pixelSize), y, 0});
            hudObjects.emplace_back(textModel);
        };
        appendMetric(scoreText, text, game::TextureId::PointsStar, scoreY);
        appendMetric(fitFoodText, fitFood, game::TextureId::Salad, scoreY - .112f);
        if (controlsHelpAlpha > 0) {
            engine::graphics::ModelInstance help;
            help.model = &controlsHelpText;
            help.pbrSurface = true;
            help.opacity = std::clamp(controlsHelpAlpha, 0.0f, 1.0f) * hudOpacity;
            hudObjects.emplace_back(help);
        }
        world.setOverlayObjects(hudObjects);
    }
    void updateGameOverMetrics(int score, int healthyFoods) {
        const bool gameOver = menuPage == Renderer::MenuPage::GameOver;
        menuScene.overlayObjects.clear();
        if (!gameOver) {
            return;
        }
        const std::string text = "Score: " + std::to_string(score);
        if (text != gameOverScoreTextValue) {
            gameOverScoreText = engine::graphics::Model(makeMenuLettering(text, .013f));
            gameOverScoreTextValue = text;
        }
        const std::string fitFood = "Fit Food: " + std::to_string(healthyFoods);
        if (fitFood != gameOverFitFoodTextValue) {
            gameOverFitFoodText = engine::graphics::Model(makeMenuLettering(fitFood, .013f));
            gameOverFitFoodTextValue = fitFood;
        }
        constexpr float iconSize = .09f;
        constexpr float gap = .022f;
        // Keep the screen-space counters aligned with the Game Over menu column.
        constexpr float metricsCenterX = .28f;
        const auto appendMetric = [&](const engine::graphics::Model& model, const std::string& label,
                                      game::TextureId icon, float y) {
            constexpr float pixelSize = .013f;
            const float textWidth = 2 * textHalfWidth(label, pixelSize);
            const float totalWidth = iconSize + gap + textWidth;
            const float left = metricsCenterX - totalWidth * .5f;
            const auto& texture = assets.texture(icon);
            engine::graphics::Sprite marker;
            marker.texture = &texture;
            marker.aspect = texture.aspect();
            marker.model = multiply(translation({left + iconSize * .5f, y, 0}),
                                    scale({iconSize * marker.aspect, iconSize, 1}));
            menuScene.overlayObjects.emplace_back(marker);
            engine::graphics::ModelInstance textModel;
            textModel.model = &model;
            textModel.pbrSurface = true;
            textModel.transform = translation({left + iconSize + gap + textWidth * .5f, y, 0});
            menuScene.overlayObjects.emplace_back(textModel);
        };
        appendMetric(gameOverScoreText, text, game::TextureId::PointsStar, .24f);
        appendMetric(gameOverFitFoodText, fitFood, game::TextureId::Salad, .08f);
    }
    float aspect() const {
        return drawable.height > 0 ? static_cast<float>(drawable.width) / drawable.height : 1.0f;
    }
    engine::graphics::Size menuRaster() const {
        if (!preferences.retroMode) {
            const float scale = preferences.renderScale / 100.0f;
            return {std::max(1, static_cast<int>(std::lround(drawable.width * scale))),
                    std::max(1, static_cast<int>(std::lround(drawable.height * scale)))};
        }
        // Keep the console raster coarse while giving the compact font enough native pixels.
        const float height = std::max(320.0f, 360.0f / aspect());
        return {std::max(1, static_cast<int>(std::lround(height * aspect()))),
                std::max(1, static_cast<int>(std::lround(height)))};
    }
    float menuAspect() const {
        const auto raster = menuRaster();
        return static_cast<float>(raster.width) / raster.height;
    }
    engine::math::Mat4 menuPanelTransform(float horizontalCenterOverride = -1.0f) const {
        using namespace engine::math;
        const auto& camera = menuScene.camera;
        const float distance = (camera.position.z - GameConfig::kPlayerZ) * .85f;
        const Vec3 forward = normalize(camera.target - camera.position);
        const Vec3 right = normalize(cross(forward, camera.up));
        const Vec3 up = cross(right, forward);
        const float halfHeight = distance * std::tan(radians(camera.verticalFov) * .5f);
        const float halfWidth = halfHeight * menuAspect();
        const bool options = menuPage == Renderer::MenuPage::Options;
        const float panelHeight = options ? 6.0f : menuPage == Renderer::MenuPage::GameOver ? 6.0f :
                                  menuPage == Renderer::MenuPage::Main ? 3.5f : 4.2f;
        const float size = std::min(halfWidth * (options ? .94f / 4.0f : .90f / 3.2f),
                                    halfHeight * 1.60f / panelHeight);
        const float horizontalCenter = horizontalCenterOverride >= 0
                                           ? horizontalCenterOverride
                                           : menuPage == Renderer::MenuPage::GameOver ? .64f : options ? .52f : .50f;
        const Vec3 center{camera.position.x + forward.x * distance + right.x * halfWidth * horizontalCenter,
                          camera.position.y + forward.y * distance + up.y * halfHeight * .04f,
                          camera.position.z + forward.z * distance + up.z * halfHeight * .04f};
        const float pitch = std::atan2(forward.y, -forward.z);
        return multiply(translation(center),
                        multiply(rotateX(pitch), multiply(rotateY(radians(options ? -6 : -18)), scale({size, size, size}))));
    }
    engine::graphics::ModelInstance& instance(std::size_t index) {
        return std::get<engine::graphics::ModelInstance>(menuScene.objects.at(index));
    }
    std::string controlValue(GraphicsControl control) const {
        const auto onOff = [](bool value) { return value ? "On" : "Off"; };
        constexpr std::array qualityNames = {"Off", "Low", "Medium", "High"};
        constexpr std::array presetNames = {"Low", "Medium", "High", "Custom"};
        switch (control) {
        case GraphicsControl::Quality: return presetNames[static_cast<int>(preferences.preset())];
        case GraphicsControl::DisplayMode:
            return preferences.displayMode == game::DisplayMode::Borderless ? "Borderless" : "Windowed";
        case GraphicsControl::RenderScale: return std::to_string(preferences.renderScale) + "%";
        case GraphicsControl::AntiAliasing: return onOff(preferences.antiAliasing);
        case GraphicsControl::Vsync: return onOff(preferences.vsync);
        case GraphicsControl::Shadows: return qualityNames[static_cast<int>(preferences.shadows)];
        case GraphicsControl::Occlusion: return onOff(preferences.ambientOcclusion);
        case GraphicsControl::Reflections: return qualityNames[static_cast<int>(preferences.reflections)];
        case GraphicsControl::MaterialDetail: return onOff(preferences.materialDetail);
        case GraphicsControl::FoliageParticles: return onOff(preferences.foliageParticles);
        case GraphicsControl::RetroMode: return onOff(preferences.retroMode);
        case GraphicsControl::DitheredShadows: return onOff(preferences.ditheredShadows);
        case GraphicsControl::DistanceFog: return onOff(preferences.distanceFog);
        default: return {};
        }
    }
    void refreshMenuValues() {
        const auto definitions = menuDefinitions(menuPage);
        for (std::size_t i = 0; i < definitions.size(); ++i) {
            auto& item = menuItems[i];
            const std::string value = controlValue(definitions[i].control);
            if (value == item.valueText)
                continue;
            item.valueText = value;
            if (!value.empty()) {
                item.value = engine::graphics::Model(makeMenuLettering(value, kOptionPixelSize));
                instance(item.valueIndex).model = &item.value;
            }
        }
    }
    void buildMenuPage() {
        const bool main = menuPage == Renderer::MenuPage::Main;
        const bool credits = menuPage == Renderer::MenuPage::Credits;
        const bool options = menuPage == Renderer::MenuPage::Options;
        const bool gameOver = menuPage == Renderer::MenuPage::GameOver;
        const float titlePixelSize = main ? kMainHeaderPixelSize : gameOver ? kGameOverHeaderPixelSize : .058f;
        title = engine::graphics::Model(makeMenuLettering(menuTitle(menuPage), titlePixelSize));
        instance(titleIndex).model = &title;
        instance(creditsIndex).model = credits ? &creditsLettering : nullptr;
        instance(extraIndex).model = options ? &extraHeading : nullptr;
        for (auto& icon : developerIcons) {
            instance(icon.objectIndex).model = credits ? &icon.model : nullptr;
            icon.animation = {};
        }
        const auto definitions = menuDefinitions(menuPage);
        for (std::size_t i = 0; i < menuItems.size(); ++i) {
            auto& item = menuItems[i];
            instance(item.valueIndex).model = nullptr;
            item.valueText.clear();
            item.hover = 0;
            if (i >= definitions.size()) {
                instance(item.labelIndex).model = nullptr;
                continue;
            }
            const auto& label = definitions[i].label;
            const float pixelSize = main ? .068f : gameOver ? .025f : credits ? kCreditsBackPixelSize :
                                    options && i == 0 ? .024f : kOptionPixelSize;
            item.label = engine::graphics::Model(makeMenuLettering(label, pixelSize));
            instance(item.labelIndex).model = &item.label;
            item.halfWidth = textHalfWidth(label, pixelSize);
            item.centerY = main ? .662f - .66f * i : gameOver ? -.45f - .28f * i :
                           credits ? kCreditsBackY : optionRowY(i);
        }
        refreshMenuValues();
    }
    bool adjustMenuItem(int selected, int direction) {
        using game::GraphicsPreset;
        using game::GraphicsQuality;
        const auto definitions = menuDefinitions(menuPage);
        if (selected < 0 || selected >= static_cast<int>(definitions.size()))
            return false;
        const auto cycle = [direction](int value, int count) {
            return (value + (direction < 0 ? count - 1 : 1)) % count;
        };
        switch (definitions[selected].control) {
        case GraphicsControl::None: return false;
        case GraphicsControl::DisplayMode: {
            const auto mode = preferences.displayMode == game::DisplayMode::Windowed
                                  ? game::DisplayMode::Borderless : game::DisplayMode::Windowed;
            if (!window.setBorderless(mode == game::DisplayMode::Borderless))
                return false;
            preferences.displayMode = mode;
            break;
        }
        case GraphicsControl::Quality:
            preferences.applyPreset(static_cast<GraphicsPreset>(cycle(
                preferences.preset() == GraphicsPreset::Custom ? 2 : static_cast<int>(preferences.preset()), 3)));
            break;
        case GraphicsControl::RenderScale: {
            constexpr std::array scales = {50, 75, 100};
            const int current = preferences.renderScale == 50 ? 0 : preferences.renderScale == 75 ? 1 : 2;
            preferences.renderScale = scales[cycle(current, 3)];
            break;
        }
        case GraphicsControl::AntiAliasing: preferences.antiAliasing = !preferences.antiAliasing; break;
        case GraphicsControl::Vsync:
            window.setVsync(!preferences.vsync);
            preferences.vsync = window.vsync();
            break;
        case GraphicsControl::Shadows:
            preferences.shadows = static_cast<GraphicsQuality>(cycle(static_cast<int>(preferences.shadows), 4)); break;
        case GraphicsControl::Occlusion: preferences.ambientOcclusion = !preferences.ambientOcclusion; break;
        case GraphicsControl::Reflections:
            preferences.reflections = static_cast<GraphicsQuality>(cycle(static_cast<int>(preferences.reflections), 4));
            break;
        case GraphicsControl::MaterialDetail: preferences.materialDetail = !preferences.materialDetail; break;
        case GraphicsControl::FoliageParticles: preferences.foliageParticles = !preferences.foliageParticles; break;
        case GraphicsControl::RetroMode: preferences.retroMode = !preferences.retroMode; break;
        case GraphicsControl::DitheredShadows: preferences.ditheredShadows = !preferences.ditheredShadows; break;
        case GraphicsControl::DistanceFog: preferences.distanceFog = !preferences.distanceFog; break;
        }
        graphics.setSettings(preferences.renderSettings());
        graphics.setRetroMode(preferences.retroMode);
        world.setFoliageParticles(preferences.foliageParticles);
        refreshMenuValues();
        saveSettings();
        return true;
    }
    void updateMenuSelection(int selection, int pressed, float time) {
        using namespace engine::math;
        const auto definitions = menuDefinitions(menuPage);
        const int selected = std::clamp(selection, 0, static_cast<int>(definitions.size()) - 1);
        const Mat4 panel = menuPanelTransform();
        const float dt = previousMenuTime < 0 ? 1.0f : std::clamp(time - previousMenuTime, 0.0f, .1f);
        previousMenuTime = time;
        const bool options = menuPage == Renderer::MenuPage::Options;
        const bool gameOver = menuPage == Renderer::MenuPage::GameOver;
        const Mat4 titlePanel = gameOver ? menuPanelTransform(.50f) : panel;
        instance(titleIndex).transform = multiply(titlePanel,
            translation({0, options || gameOver ? 2.28f : 1.6995f, 0}));
        instance(creditsIndex).transform = multiply(panel, translation({0, gameOver ? .94f : 0, 0}));
        for (std::size_t i = 0; i < developerIcons.size(); ++i) {
            auto& icon = developerIcons[i];
            if (menuPage == Renderer::MenuPage::Credits)
                icon.animation.advance(dt);
            const auto pose = icon.animation.pose();
            const Vec3 anchor{kDeveloperIconX + pose.offsetX * kDeveloperIconSize,
                              kCreditsLines[i + 1].centerY, .065f};
            icon.transform = multiply(panel, multiply(translation(anchor),
                multiply(rotation({0, 0, 1}, pose.roll), multiply(rotateY(pose.yaw),
                    scale({kDeveloperIconSize, kDeveloperIconSize, .08f})))));
            instance(icon.objectIndex).transform = icon.transform;
        }
        const float extraY = (optionRowY(kExtraFirstItem - 1) + optionRowY(kExtraFirstItem)) * .5f;
        instance(extraIndex).transform = multiply(panel, translation({kOptionLeft + textHalfWidth("EXTRA", .030f), extraY, 0}));
        for (std::size_t i = 0; i < definitions.size(); ++i) {
            auto& item = menuItems[i];
            item.hover = std::lerp(item.hover, i == static_cast<std::size_t>(selected) ? 1.0f : 0.0f,
                                   1 - std::exp(-12 * dt));
            const bool down = static_cast<int>(i) == pressed;
            const float s = 1 + (options ? .03f : .06f) * item.hover - (down ? .09f : 0);
            const float x = menuPage == Renderer::MenuPage::Main ? (static_cast<float>(i) - 1.5f) * .10f : 0;
            item.transform = multiply(panel, multiply(translation({x, item.centerY, .07f * item.hover - (down ? .14f : 0)}),
                                                       scale({s, s, s})));
            const float labelX = options && !item.valueText.empty() ? kOptionLeft + item.halfWidth : 0;
            instance(item.labelIndex).transform = multiply(item.transform, translation({labelX, 0, 0}));
            instance(item.labelIndex).emissiveScale = 1 + .8f * item.hover;
            const float valueX = item.valueText.empty() ? 0 :
                kOptionRight - textHalfWidth(item.valueText, kOptionPixelSize);
            instance(item.valueIndex).transform = multiply(item.transform, translation({valueX, 0, 0}));
            instance(item.valueIndex).emissiveScale = 1 + .8f * item.hover;
        }
        const auto& item = menuItems[selected];
        const float cursorX = options ? kOptionLeft - .18f : -item.halfWidth - .23f;
        instance(cursorIndex).transform = multiply(item.transform, translation({cursorX, 0, .08f}));
    }
    std::optional<engine::math::Vec2> menuPointer(int x, int y) const {
        using namespace engine::math;
        int width = 0, height = 0;
        SDL_GetWindowSize(window.nativeHandle(), &width, &height);
        if (width <= 0 || height <= 0 || x < 0 || y < 0 || x >= width || y >= height)
            return std::nullopt;
        const auto raster = menuRaster();
        const float fit = std::min(static_cast<float>(width) / raster.width, static_cast<float>(height) / raster.height);
        return Vec2{(2.0f * x - width) / (raster.width * fit), (height - 2.0f * y) / (raster.height * fit)};
    }
    int menuHitTest(int x, int y) const {
        using namespace engine::math;
        const auto pointer = menuPointer(x, y);
        if (!pointer)
            return -1;
        if (menuPage == Renderer::MenuPage::GameOver) {
            for (std::size_t i = 0; i < menuDefinitions(menuPage).size(); ++i) {
                if (std::abs(pointer->x) <= .95f && std::abs(pointer->y - menuItems[i].centerY) <= .10f)
                    return static_cast<int>(i);
            }
            return -1;
        }
        const Vec2 point = *pointer;
        const Mat4 vp = menuScene.camera.viewProjection(menuAspect());
        const auto project = [&](const Mat4& transform, Vec3 p) {
            const Mat4 m = multiply(vp, transform);
            const float w = m.values[3] * p.x + m.values[7] * p.y + m.values[11] * p.z + m.values[15];
            const Vec3 v = transformPoint(m, p);
            return Vec2{v.x / w, v.y / w};
        };
        const auto side = [](Vec2 a, Vec2 b, Vec2 p) {
            return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
        };
        for (std::size_t i = 0; i < menuDefinitions(menuPage).size(); ++i) {
            const auto& item = menuItems[i];
            const bool options = menuPage == Renderer::MenuPage::Options;
            const float half = options ? kOptionRight + .05f : item.halfWidth + .12f;
            const float height = options ? .1125f : menuPage == Renderer::MenuPage::Main ? .27f : .24f;
            const std::array<Vec2, 4> corners = {project(item.transform, {-half, -height, .105f}),
                project(item.transform, {half, -height, .105f}), project(item.transform, {half, height, .105f}),
                project(item.transform, {-half, height, .105f})};
            bool positive = false, negative = false;
            for (int edge = 0; edge < 4; ++edge) {
                const float cross = side(corners[edge], corners[(edge + 1) % 4], point);
                positive |= cross > 0;
                negative |= cross < 0;
            }
            if (!(positive && negative))
                return static_cast<int>(i);
        }
        return -1;
    }
    int developerIconHitTest(int x, int y) const {
        using namespace engine::math;
        if (menuPage != Renderer::MenuPage::Credits)
            return -1;
        const auto pointer = menuPointer(x, y);
        if (!pointer)
            return -1;
        const auto& camera = menuScene.camera;
        const Vec3 forward = normalize(camera.target - camera.position);
        const Vec3 right = normalize(cross(forward, camera.up));
        const Vec3 up = cross(right, forward);
        const float sy = pointer->y * std::tan(radians(camera.verticalFov) * .5f);
        const float sx = pointer->x * std::tan(radians(camera.verticalFov) * .5f) * menuAspect();
        const Vec3 direction = normalize({forward.x + right.x * sx + up.x * sy,
            forward.y + right.y * sx + up.y * sy, forward.z + right.z * sx + up.z * sy});
        int hit = -1;
        float nearest = std::numeric_limits<float>::max();
        for (std::size_t i = 0; i < developerIcons.size(); ++i) {
            const auto& icon = developerIcons[i];
            const Mat4 inverse = inverseAffine(icon.transform);
            const Vec3 origin = transformPoint(inverse, camera.position);
            const Vec3 ray = transformPoint(inverse, {camera.position.x + direction.x,
                camera.position.y + direction.y, camera.position.z + direction.z}) - origin;
            // Reject the enclosing box before testing the pixel contour and alpha cutout.
            const auto center = icon.model.boundsCenter(), half = icon.model.boundsHalfExtent();
            const std::array o = {origin.x, origin.y, origin.z}, d = {ray.x, ray.y, ray.z};
            const std::array c = {center.x, center.y, center.z}, h = {half.x, half.y, half.z};
            float enter = 0, leave = nearest;
            bool outside = false;
            for (int axis = 0; axis < 3; ++axis) {
                if (std::abs(d[axis]) < 1e-7f) {
                    outside |= o[axis] < c[axis] - h[axis] || o[axis] > c[axis] + h[axis];
                } else {
                    const float a = (c[axis] - h[axis] - o[axis]) / d[axis];
                    const float b = (c[axis] + h[axis] - o[axis]) / d[axis];
                    enter = std::max(enter, std::min(a, b));
                    leave = std::min(leave, std::max(a, b));
                }
            }
            if (outside || enter > leave)
                continue;
            const auto& geometry = icon.geometry;
            for (std::size_t triangle = 0; triangle < geometry.indices.size(); triangle += 3) {
                std::array<const float*, 3> v{};
                for (int corner = 0; corner < 3; ++corner)
                    v[corner] = geometry.vertices.data() +
                        geometry.indices[triangle + corner] * engine::assets::SpriteGeometry::stride;
                const Vec3 a{v[0][0], v[0][1], v[0][2]};
                const Vec3 ab = Vec3{v[1][0], v[1][1], v[1][2]} - a;
                const Vec3 ac = Vec3{v[2][0], v[2][1], v[2][2]} - a;
                const Vec3 p = cross(ray, ac);
                const float determinant = dot(ab, p);
                if (std::abs(determinant) < 1e-7f)
                    continue;
                const Vec3 offset = origin - a;
                const float u = dot(offset, p) / determinant;
                const Vec3 q = cross(offset, ab);
                const float vWeight = dot(ray, q) / determinant;
                const float distance = dot(ac, q) / determinant;
                if (u < 0 || vWeight < 0 || u + vWeight > 1 || distance < 0 || distance >= nearest)
                    continue;
                const float uvX = v[0][3] * (1 - u - vWeight) + v[1][3] * u + v[2][3] * vWeight;
                const float uvY = v[0][4] * (1 - u - vWeight) + v[1][4] * u + v[2][4] * vWeight;
                const auto size = icon.silhouette.size();
                const int px = std::clamp(int(uvX * size.width), 0, size.width - 1);
                const int py = std::clamp(int(uvY * size.height), 0, size.height - 1);
                if (icon.silhouette.alphaMask()[py * size.width + px] < 26)
                    continue;
                nearest = distance;
                hit = static_cast<int>(i);
            }
        }
        return hit;
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
            character->transform = multiply(translation({GameConfig::kMenuCatX, -ground + menuPlayer.lift(), GameConfig::kMenuCatZ}),
                multiply(engine::math::rotateY(engine::math::radians(GameConfig::kMenuCharacterYawDegrees)),
                         translation({-anchor.x, 0, -anchor.z})));
        }
        updateMenuSelection(selection, pressed, time);
    }
};
Renderer::Renderer(std::filesystem::path assetRoot, std::filesystem::path settingsFile, bool visible)
    : assetRoot_(std::move(assetRoot)), settingsFile_(std::move(settingsFile)), visible_(visible) {
    if (assetRoot_.empty()) {
        char* base = SDL_GetBasePath();
        const std::filesystem::path folder = base ? std::filesystem::path(reinterpret_cast<const char8_t*>(base)) : std::filesystem::current_path();
        SDL_free(base);
        assetRoot_ = std::filesystem::is_directory(folder / "assets") ? folder / "assets" :
                     std::filesystem::path(SUBWAY_ASSET_DIR);
    }
    if (settingsFile_.empty())
        settingsFile_ = assetRoot_.parent_path() / "game.cfg";
}
Renderer::~Renderer() = default;
bool Renderer::initialize() {
    if (impl_)
        return true;
    try {
        impl_ = std::make_unique<Impl>(assetRoot_, settingsFile_, visible_);
        return true;
    } catch (const std::exception& error) {
        std::cerr << "Renderer initialization failed: " << error.what() << '\n';
        return false;
    }
}
void Renderer::resizeViewport() const {
    if (impl_)
        impl_->drawable = impl_->window.drawableSize();
}
void Renderer::setWindowTitle(const std::string& title) const {
    if (impl_)
        impl_->window.setTitle(title);
}
bool Renderer::ditheredShadows() const { return impl_ && impl_->preferences.ditheredShadows; }
bool Renderer::distanceFog() const { return impl_ && impl_->preferences.distanceFog; }
engine::graphics::RenderMode Renderer::renderMode() const {
    return impl_ && impl_->preferences.retroMode ? engine::graphics::RenderMode::Psx : engine::graphics::RenderMode::Normal;
}
game::GraphicsSettings Renderer::graphicsSettings() const { return impl_ ? impl_->preferences : game::GraphicsSettings{}; }
void Renderer::setMenuPage(MenuPage page, int selection) const {
    if (!impl_ || impl_->menuPage == page)
        return;
    impl_->menuPage = page;
    impl_->buildMenuPage();
    impl_->previousMenuTime = -1;
    impl_->updateMenuSelection(selection, -1, std::max(impl_->previousMenuTime, 0.0f));
}
int Renderer::menuItemCount() const { return impl_ ? static_cast<int>(menuDefinitions(impl_->menuPage).size()) : 0; }
std::string Renderer::menuItemValue(int selection) const {
    if (!impl_ || selection < 0 || selection >= menuItemCount())
        return {};
    return impl_->controlValue(menuDefinitions(impl_->menuPage)[selection].control);
}
bool Renderer::adjustMenuItem(int selection, int direction) const {
    return impl_ && impl_->adjustMenuItem(selection, direction);
}
int Renderer::menuHitTest(int x, int y) const { return impl_ ? impl_->menuHitTest(x, y) : -1; }
int Renderer::developerIconHitTest(int x, int y) const { return impl_ ? impl_->developerIconHitTest(x, y) : -1; }
void Renderer::clickDeveloperIcon(int icon, bool doubleClick) const {
    if (impl_ && impl_->menuPage == MenuPage::Credits && icon >= 0 &&
        icon < static_cast<int>(impl_->developerIcons.size()))
        impl_->developerIcons[icon].animation.click(doubleClick);
}
void Renderer::renderMenu(const float time, const int selection, const int pressed, const int score,
                          const int healthyFoods) const {
    if (!impl_)
        return;
    resizeViewport();
    if (impl_->drawable.width <= 0 || impl_->drawable.height <= 0)
        return;
    impl_->updateGameOverMetrics(score, healthyFoods);
    impl_->menuScene.camera = game::menuCamera(impl_->menuAspect());
    impl_->updateMenuPlayer(time, selection, pressed);
    impl_->graphics.setRetroMode(impl_->preferences.retroMode);
    impl_->graphics.render(impl_->menuScene, impl_->drawable, impl_->menuRaster());
    impl_->window.swap();
}
void Renderer::renderGameOver(const float playerX, const float roadScroll, const float gameTime, const int score,
                              const std::span<const Entity> entities,
                              const std::span<const CollectEffect> collectEffects, const float nightBlend,
                              const float cameraBlend, const int health, const int healthyFoods,
                              const game::GameOverReason deathReason, const float controlsHelpAlpha,
                              const float transitionTime,
                              const int selection, const int pressed) const {
    if (!impl_)
        return;
    resizeViewport();
    if (impl_->drawable.width <= 0 || impl_->drawable.height <= 0)
        return;

    const float elapsed = std::max(transitionTime, 0.0f);
    const float hudOpacity = std::clamp(1.0f - elapsed / GameConfig::kGameOverHudFadeSeconds, 0.0f, 1.0f);
    const float panelLinear = std::clamp((elapsed - GameConfig::kGameOverHudFadeSeconds) /
                                             GameConfig::kGameOverPanelDropSeconds,
                                         0.0f, 1.0f);
    const float panelProgress = panelLinear * panelLinear * (3.0f - 2.0f * panelLinear);
    const float panelOffsetY = (1.0f - panelProgress) * 2.15f;

    const float deathProgress = std::clamp(elapsed / GameConfig::kPlayerDeathAnimationSeconds, 0.0f, 1.0f);
    const auto& scene = impl_->world.build(playerX, roadScroll, gameTime, entities, collectEffects, 0,
                                           nightBlend, 0, cameraBlend, impl_->aspect(), deathReason, deathProgress);
    impl_->updateScoreText(score, health, healthyFoods, controlsHelpAlpha, hudOpacity);
    impl_->updateMenuSelection(selection, pressed, elapsed);
    std::vector<engine::graphics::DrawCommand> overlay = impl_->hudObjects;

    if (panelProgress > 0) {
        engine::graphics::ModelInstance panel;
        panel.model = &impl_->gameOverPanel;
        panel.pbrSurface = true;
        panel.opacity = .57f * panelProgress;
        panel.transform = multiply(translation({0, panelOffsetY, 0}),
                                   scale({2.0f * impl_->aspect(), 2.0f, 1}));
        overlay.emplace_back(panel);

        engine::graphics::ModelInstance heading;
        heading.model = &impl_->title;
        heading.pbrSurface = true;
        heading.opacity = panelProgress;
        heading.transform = translation({0, .56f + panelOffsetY, .02f});
        overlay.emplace_back(heading);

        const std::string scoreLabel = "Score: " + std::to_string(score);
        if (scoreLabel != impl_->gameOverScoreTextValue) {
            impl_->gameOverScoreText = engine::graphics::Model(makeMenuLettering(scoreLabel, .020f));
            impl_->gameOverScoreTextValue = scoreLabel;
        }
        const std::string foodLabel = "Fit Food: " + std::to_string(healthyFoods);
        if (foodLabel != impl_->gameOverFitFoodTextValue) {
            impl_->gameOverFitFoodText = engine::graphics::Model(makeMenuLettering(foodLabel, .020f));
            impl_->gameOverFitFoodTextValue = foodLabel;
        }
        constexpr float iconSize = .12f;
        constexpr float iconGap = .032f;
        const auto appendMetric = [&](const engine::graphics::Model& model, const std::string& label,
                                      game::TextureId icon, const float y) {
            constexpr float pixelSize = .020f;
            const float textWidth = 2 * textHalfWidth(label, pixelSize);
            const float left = -(iconSize + iconGap + textWidth) * .5f;
            const auto& texture = impl_->assets.texture(icon);
            engine::graphics::Sprite marker;
            marker.texture = &texture;
            marker.aspect = texture.aspect();
            marker.opacity = panelProgress;
            marker.model = multiply(translation({left + iconSize * .5f, y + panelOffsetY, .01f}),
                                    scale({iconSize * marker.aspect, iconSize, 1}));
            overlay.emplace_back(marker);
            engine::graphics::ModelInstance text;
            text.model = &model;
            text.pbrSurface = true;
            text.opacity = panelProgress;
            text.transform = translation({left + iconSize + iconGap + textWidth * .5f,
                                           y + panelOffsetY, .02f});
            overlay.emplace_back(text);
        };
        appendMetric(impl_->gameOverScoreText, scoreLabel, game::TextureId::PointsStar, .16f);
        appendMetric(impl_->gameOverFitFoodText, foodLabel, game::TextureId::Salad, -.04f);

        for (std::size_t i = 0; i < 2; ++i) {
            const auto& item = impl_->menuItems[i];
            engine::graphics::ModelInstance option;
            option.model = &item.label;
            option.pbrSurface = true;
            option.opacity = panelProgress;
            option.emissiveScale = i == static_cast<std::size_t>(std::clamp(selection, 0, 1))
                                       ? 1.8f : 1.0f;
            option.transform = multiply(translation({0, item.centerY + panelOffsetY, .02f}),
                                         scale({1.0f + .06f * item.hover, 1.0f + .06f * item.hover, 1}));
            overlay.emplace_back(option);
        }
    }

    impl_->world.setOverlayObjects(overlay);
    impl_->graphics.setRetroMode(impl_->preferences.retroMode);
    impl_->graphics.render(scene, impl_->drawable);
    impl_->window.swap();
}
void Renderer::render(const float playerX, const float roadScroll, const float gameTime,
                      const int score, const std::span<const Entity> entities,
                      const std::span<const CollectEffect> collectEffects, const float effectRemainder,
                      const float nightBlend, const float playerSpeed, const float cameraBlend,
                      const int health, const float controlsHelpAlpha, const int healthyFoods) const {
    if (!impl_)
        return;
    resizeViewport();
    if (impl_->drawable.width <= 0 || impl_->drawable.height <= 0)
        return;
    const auto& scene = impl_->world.build(playerX, roadScroll, gameTime, entities, collectEffects, effectRemainder,
                                           nightBlend, playerSpeed, cameraBlend, impl_->aspect());
    impl_->updateScoreText(score, health, healthyFoods, controlsHelpAlpha);
    impl_->graphics.render(scene, impl_->drawable);
    impl_->window.swap();
}
