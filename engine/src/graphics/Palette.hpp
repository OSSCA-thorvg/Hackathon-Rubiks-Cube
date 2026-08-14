#pragma once

#include "cube/CubeMove.hpp"
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

/**
 * The color a guide line takes, by the axis its ring turns about.
 *
 * Three hues a third of the wheel apart, and all three chosen from the gaps
 * the stickers leave: cyan, magenta and lime sit between blue and green,
 * between blue and red, and between green and yellow. A guide has to read as a
 * line over the drawing rather than as another sticker in it.
 */
[[nodiscard]] Color guide_color(cube::Axis axis) noexcept;

}  // namespace rubiks::graphics
