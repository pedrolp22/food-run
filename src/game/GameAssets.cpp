#include "game/GameAssets.h"
#include "GameConfig.h"

namespace game {
using namespace engine::graphics;
namespace {
struct TextureSpec {
    TextureId id;
    const char* file;
    bool repeat;
};
struct ModelSpec {
    ModelId id;
    const char* directory;
    float height;
};
constexpr std::array textures = {
    TextureSpec{TextureId::Road, "road.png", true},       TextureSpec{TextureId::Skyscraper, "skyscraper.png", true},
    TextureSpec{TextureId::Wall, "wall_brick.png", true}, TextureSpec{TextureId::Water, "water.png", false},
    TextureSpec{TextureId::Salad, "salad.png", false},    TextureSpec{TextureId::Salmon, "salmon.png", false},
    TextureSpec{TextureId::Soda, "soda.png", false},      TextureSpec{TextureId::Burger, "burger.png", false},
    TextureSpec{TextureId::Fries, "fries.png", false},    TextureSpec{TextureId::Sidewalk, "sidewalk.png", true}};
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
        if (collectible) {
            spriteMeshes_[index] = makeSpriteMesh(textures_[index]);
            // Stray alpha pixels in food photos should not emit separate little ripple rings.
            spriteOutlines_[index] = SpriteOutline(textures_[index], GameConfig::kCollectibleRippleRadiusPixels, true);
        }
    }
    for (const auto& asset : models) {
        models_[static_cast<std::size_t>(asset.id)] =
            Model(root / "models" / asset.directory / "scene.gltf", asset.height);
    }
}
const Texture& GameAssets::texture(TextureId id) const {
    return textures_.at(static_cast<std::size_t>(id));
}
const Model& GameAssets::model(ModelId id) const {
    return models_.at(static_cast<std::size_t>(id));
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
