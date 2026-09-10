#pragma once

#include "engine/graphics/Model.h"
#include "game/Entities.h"
#include <array>

namespace game {
enum class TextureId { Road, Skyscraper, Wall, Water, Salad, Salmon, Soda, Burger, Fries, Count };
enum class ModelId { Cone, Barrier, TrashCan, Bench, Count };
// The game's asset catalog; the graphics library knows no asset names or game rules.
class GameAssets {
  public:
    explicit GameAssets(const std::filesystem::path& root, bool mipmapped = true);
    const engine::graphics::Texture& texture(TextureId id) const;
    const engine::graphics::Model& model(ModelId id) const;
    const engine::graphics::Texture& collectible(const Entity& entity) const;

  private:
    std::array<engine::graphics::Texture, static_cast<std::size_t>(TextureId::Count)> textures_;
    std::array<engine::graphics::Model, static_cast<std::size_t>(ModelId::Count)> models_;
};
} // namespace game
