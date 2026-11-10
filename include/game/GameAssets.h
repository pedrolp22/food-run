#pragma once

#include "engine/graphics/Model.h"
#include "engine/graphics/SpriteOutline.h"
#include "game/Entities.h"
#include <array>

namespace game {
enum class TextureId {
    Road,
    Skyscraper,
    Wall,
    Water,
    Salad,
    Salmon,
    Soda,
    Burger,
    Fries,
    Sidewalk,
    BuildingTop,
    Skybox,
    Cloud1,
    Cloud2,
    Cloud3,
    BrickWindow,
    BrickWall,
    BrickDoor,
    Grass,
    Fence,
    Leaf,
    Moon,
    ConcreteTiles,
    HeartRed,
    HeartLost,
    PointsStar,
    Count
};
enum class ModelId { Cone, Barrier, TrashCan, Bench, Tree, StreetLamp, PlayerCat, Count };
// The game's asset catalog; the graphics library knows no asset names or game rules.
class GameAssets {
  public:
    explicit GameAssets(const std::filesystem::path& root, bool mipmapped = true);
    const engine::graphics::Texture& texture(TextureId id) const;
    const engine::graphics::Model& model(ModelId id) const;
    engine::graphics::MaterialTextures materialTextures(TextureId id) const;
    engine::graphics::MaterialTextures materialTextures(const engine::graphics::Texture& texture) const;
    const engine::graphics::Texture& collectible(const Entity& entity) const;
    const engine::graphics::Mesh& collectibleMesh(const Entity& entity) const;
    const engine::graphics::SpriteOutline& collectibleOutline(const Entity& entity) const;

  private:
    std::array<engine::graphics::Texture, static_cast<std::size_t>(TextureId::Count)> textures_;
    std::array<engine::graphics::Texture, static_cast<std::size_t>(TextureId::Count)> surfaceMaps_;
    std::array<engine::graphics::Texture, static_cast<std::size_t>(TextureId::Count)> normalMaps_;
    std::array<engine::graphics::Texture, static_cast<std::size_t>(TextureId::Count)> glassMaps_;
    std::array<engine::graphics::Mesh, static_cast<std::size_t>(TextureId::Count)> spriteMeshes_;
    std::array<engine::graphics::SpriteOutline, static_cast<std::size_t>(TextureId::Count)> spriteOutlines_;
    std::array<engine::graphics::Model, static_cast<std::size_t>(ModelId::Count)> models_;
};
} // namespace game
