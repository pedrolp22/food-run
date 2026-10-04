#include "game/GameAssets.h"
#include "GameConfig.h"

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
    const char* directory;
    float height;
};
constexpr std::array textures = {
    TextureSpec{TextureId::Road, "road.png", true, true}, TextureSpec{TextureId::Skyscraper, "skyscraper.png", false, true},
    TextureSpec{TextureId::Wall, "wall_brick.png", true, true}, TextureSpec{TextureId::Water, "water.png", false},
    TextureSpec{TextureId::Salad, "salad.png", false},    TextureSpec{TextureId::Salmon, "salmon.png", false},
    TextureSpec{TextureId::Soda, "soda.png", false},      TextureSpec{TextureId::Burger, "burger.png", false},
    TextureSpec{TextureId::Fries, "fries.png", false}, TextureSpec{TextureId::Sidewalk, "sidewalk.png", true, true},
    TextureSpec{TextureId::BuildingTop, "building_top.png", false, true}};
constexpr std::array models = {
    ModelSpec{ModelId::Cone, "traffic_cone", 1.0f}, ModelSpec{ModelId::Barrier, "barrier", 0.9f},
    ModelSpec{ModelId::TrashCan, "trash_can", 0.75f}, ModelSpec{ModelId::Bench, "bench", 0.7f}};
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
        textures_[index] = Texture(root / asset.file, {mipmapped ? TextureFilter::Mipmapped : TextureFilter::Nearest,
                                                       asset.repeat, true, collectible});
        if (asset.materialDetail) {
            const auto stem = std::filesystem::path(asset.file).stem().string();
            surfaceMaps_[index] = Texture(root / "materials" / (stem + "_surface.png"),
                                         {TextureFilter::Trilinear, asset.repeat, true});
            normalMaps_[index] = Texture(root / "materials" / (stem + "_normal.png"),
                                        {TextureFilter::Trilinear, asset.repeat, true});
            if (asset.id == TextureId::Skyscraper) {
                glassMaps_[index] = Texture(root / "materials" / (stem + "_glass.png"),
                                           {TextureFilter::Trilinear, asset.repeat, true});
            }
        }
        if (collectible) {
            spriteMeshes_[index] = makeSpriteMesh(textures_[index]);
            // Stray alpha pixels in food photos should not emit separate little ripple rings.
            spriteOutlines_[index] = SpriteOutline(textures_[index], GameConfig::kCollectibleRippleRadiusPixels, true);
        }
    }
    for (const auto& asset : models) {
        auto data = engine::assets::loadGltf(root / "models" / asset.directory / "scene.gltf", asset.height);
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
