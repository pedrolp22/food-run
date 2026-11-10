#include "game/GameAssets.h"
#include "GameConfig.h"
#include "game/PlayerCharacter.h"

namespace game {
using namespace engine::graphics;
namespace {
struct TextureSpec {
    TextureId id;
    const char* file;
    bool repeat;
    bool materialDetail = false;
};
struct ModelSpec {
    ModelId id;
    const char* file;
    float height;
};
constexpr std::array textures = {
    TextureSpec{TextureId::Road, "textures/road.png", true, true},
    TextureSpec{TextureId::Skyscraper, "textures/skyscraper/skyscraper.png", false, true},
    TextureSpec{TextureId::Wall, "wall_brick.png", true, true},
    TextureSpec{TextureId::Water, "textures/collectibles/water.png", false},
    TextureSpec{TextureId::Salad, "textures/collectibles/salad.png", false},
    TextureSpec{TextureId::Salmon, "textures/collectibles/salmon.png", false},
    TextureSpec{TextureId::Soda, "textures/collectibles/soda.png", false},
    TextureSpec{TextureId::Burger, "textures/collectibles/burger.png", false},
    TextureSpec{TextureId::Fries, "textures/collectibles/fries.png", false},
    TextureSpec{TextureId::Sidewalk, "textures/sidewalk.png", true, true},
    TextureSpec{TextureId::ConcreteTiles, "textures/concretetiles.png", true, true},
    TextureSpec{TextureId::BuildingTop, "textures/skyscraper/building_top.png", false, true},
    TextureSpec{TextureId::Skybox, "textures/sky/skybox.png", false},
    TextureSpec{TextureId::Cloud1, "textures/sky/cloud1.png", false},
    TextureSpec{TextureId::Cloud2, "textures/sky/cloud2.png", false},
    TextureSpec{TextureId::Cloud3, "textures/sky/cloud3.png", false},
    TextureSpec{TextureId::BrickWindow, "textures/brick_building/brick_building_window.png", true, true},
    TextureSpec{TextureId::BrickWall, "textures/brick_building/brick_building_wall.png", true, true},
    TextureSpec{TextureId::BrickDoor, "textures/brick_building/brick_building_door.png", false, true},
    TextureSpec{TextureId::Grass, "textures/grassy/grass.png", true, true},
    TextureSpec{TextureId::Fence, "textures/grassy/spikedfence_decor.png", true, true},
    TextureSpec{TextureId::Leaf, "textures/grassy/leaf.png", false, true},
    TextureSpec{TextureId::Moon, "textures/sky/moon.png", false},
    TextureSpec{TextureId::HeartRed, "textures/ui/heart-red.png", false},
    TextureSpec{TextureId::HeartLost, "textures/ui/heart-lost.png", false},
    TextureSpec{TextureId::PointsStar, "textures/ui/points-star.png", false}};
constexpr std::array models = {ModelSpec{ModelId::Cone, "traffic_cone/scene.gltf", 1.0f},
                               ModelSpec{ModelId::Barrier, "barrier/scene.gltf", 0.9f},
                               ModelSpec{ModelId::TrashCan, "trash_can/scene.gltf", 0.75f},
                               ModelSpec{ModelId::Bench, "bench/scene.gltf", 0.7f},
                               ModelSpec{ModelId::Tree, "tree/scene.gltf", 1.0f},
                               ModelSpec{ModelId::StreetLamp, "street_lamp/scene.gltf", 1.0f},
                               ModelSpec{ModelId::PlayerCat, "gato/gato.gltf", GameConfig::kPlayerModelHeight}};
static_assert(textures.size() == static_cast<std::size_t>(TextureId::Count));
static_assert(models.size() == static_cast<std::size_t>(ModelId::Count));
TextureId collectibleTexture(const Entity& entity) {
    constexpr TextureId slow[] = {TextureId::Soda, TextureId::Burger, TextureId::Fries};
    constexpr TextureId score[] = {TextureId::Water, TextureId::Salad, TextureId::Salmon};
    const auto index = static_cast<unsigned int>(entity.textureIndex) % 3;
    return entity.type == EntityType::SlowCollectible ? slow[index] : score[index];
}
} // namespace
GameAssets::GameAssets(const std::filesystem::path& root, bool mipmapped) {
    for (const auto& asset : textures) {
        const auto index = static_cast<std::size_t>(asset.id);
        const bool collectible = asset.id >= TextureId::Water && asset.id <= TextureId::Fries;
        const bool skyTexture = asset.id == TextureId::Skybox || asset.id == TextureId::Moon ||
                                (asset.id >= TextureId::Cloud1 && asset.id <= TextureId::Cloud3);
        const auto filter = !mipmapped || skyTexture || asset.id == TextureId::Leaf ? TextureFilter::Nearest
                            : asset.materialDetail                                  ? TextureFilter::Trilinear
                                                                                    : TextureFilter::Mipmapped;
        textures_[index] = Texture(root / asset.file, {filter, asset.repeat, true, collectible});
        if (asset.materialDetail) {
            const auto stem = std::filesystem::path(asset.file).stem().string();
            surfaceMaps_[index] =
                Texture(root / "materials" / (stem + "_surface.png"), {TextureFilter::Trilinear, asset.repeat, true});
            normalMaps_[index] =
                Texture(root / "materials" / (stem + "_normal.png"), {TextureFilter::Trilinear, asset.repeat, true});
            if (asset.id == TextureId::Skyscraper || asset.id == TextureId::BrickWindow) {
                glassMaps_[index] =
                    Texture(root / "materials" / (stem + "_glass.png"), {TextureFilter::Trilinear, asset.repeat, true});
            }
        }
        if (collectible) {
            spriteMeshes_[index] = makeSpriteMesh(textures_[index]);
            // Stray alpha pixels in food photos should not emit separate little ripple rings.
            spriteOutlines_[index] = SpriteOutline(textures_[index], GameConfig::kCollectibleRippleRadiusPixels, true);
        }
    }
    for (const auto& asset : models) {
        auto data = engine::assets::loadGltf(root / "models" / asset.file, asset.height);
        if (asset.id == ModelId::PlayerCat)
            preparePlayerModel(data);
        for (auto& primitive : data.primitives) {
            auto& material = primitive.material;
            // The game's companions supplement assets that have no authored surface maps.
            const auto companion =
                material.baseTexture.parent_path() / (material.baseTexture.stem().string() + "_surface.png");
            if (!material.baseTexture.empty() && material.metallicRoughnessTexture.empty() &&
                std::filesystem::exists(companion)) {
                material.metallicRoughnessTexture = companion;
                material.packedReflectivity = true;
                if (material.occlusionTexture.empty()) {
                    material.occlusionTexture = companion;
                }
            }
            const auto normalCompanion =
                material.baseTexture.parent_path() / (material.baseTexture.stem().string() + "_normal.png");
            if (!material.baseTexture.empty() && material.normalTexture.empty() &&
                std::filesystem::exists(normalCompanion)) {
                material.normalTexture = normalCompanion;
                if (asset.id == ModelId::Tree) {
                    material.normalScale = GameConfig::kTreeNormalScale;
                }
            }
        }
        models_[static_cast<std::size_t>(asset.id)] = Model(data);
    }
}
const Texture& GameAssets::texture(TextureId id) const {
    return textures_.at(static_cast<std::size_t>(id));
}
const Model& GameAssets::model(ModelId id) const {
    return models_.at(static_cast<std::size_t>(id));
}
MaterialTextures GameAssets::materialTextures(TextureId id) const {
    const auto index = static_cast<std::size_t>(id);
    const auto& properties = surfaceMaps_.at(index);
    const auto& normal = normalMaps_.at(index);
    const auto& glass = glassMaps_.at(index);
    return {.properties = properties.id() ? &properties : nullptr,
            .normal = normal.id() ? &normal : nullptr,
            .occlusion = properties.id() ? &properties : nullptr,
            .normalScale = id == TextureId::Grass   ? GameConfig::kGrassNormalScale
                           : id == TextureId::Fence ? GameConfig::kFenceNormalScale
                           : id == TextureId::Leaf  ? GameConfig::kLeafNormalScale
                                                    : 1,
            .packedReflectivity = true,
            .glass = glass.id() ? &glass : nullptr};
}
MaterialTextures GameAssets::materialTextures(const Texture& texture) const {
    for (std::size_t index = 0; index < textures_.size(); ++index) {
        if (&textures_[index] == &texture) {
            return materialTextures(static_cast<TextureId>(index));
        }
    }
    return {};
}
const Texture& GameAssets::collectible(const Entity& entity) const {
    return texture(collectibleTexture(entity));
}
const Mesh& GameAssets::collectibleMesh(const Entity& entity) const {
    return spriteMeshes_.at(static_cast<std::size_t>(collectibleTexture(entity)));
}
const SpriteOutline& GameAssets::collectibleOutline(const Entity& entity) const {
    return spriteOutlines_.at(static_cast<std::size_t>(collectibleTexture(entity)));
}
} // namespace game
