#pragma once

#include "cube/Cubie.hpp"
#include "graphics/Color.hpp"

namespace rubiks::graphics {

/**
 * The only place the domain's color enum turns into pixels.
 *
 * The mapping lives on the graphics side because the domain has no notion of
 * a channel value; cube::FaceColor names a sticker, not a shade.
 */
[[nodiscard]] Color to_color(cube::FaceColor color) noexcept;

}  // namespace rubiks::graphics
