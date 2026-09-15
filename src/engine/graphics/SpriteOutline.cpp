#include "engine/graphics/SpriteOutline.h"
#include "engine/assets/AlphaDistanceField.h"
#include <limits>
#include <stdexcept>

namespace engine::graphics {
SpriteOutline::SpriteOutline(const Texture& sprite, int radiusPixels, bool discardIsolatedPixels)
    : radius_(radiusPixels) {
    if (radiusPixels <= 0 || radiusPixels > std::numeric_limits<int>::max() - 2) {
        throw std::invalid_argument("Sprite outline radius must be positive");
    }
    const auto source = sprite.size();
    const auto data = assets::makeAlphaDistanceField(sprite.alphaMask(), source.width, source.height, radiusPixels + 2,
                                                     discardIsolatedPixels);
    field_ = Texture::fromRedChannel({data.width, data.height}, data.pixels);
    transform_ = math::multiply(math::translation({.5f - data.sourceCenter.x, .5f - data.sourceCenter.y, 0}),
                                math::scale({float(data.width) / source.width, float(data.height) / source.height, 1}));
}
} // namespace engine::graphics
