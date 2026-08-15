#pragma once

#include "cube/CubeMove.hpp"
#include "cube/Cubie.hpp"
#include "graphics/Color.hpp"

namespace rubiks::graphics {

/**
 * Which set of six shades the stickers are drawn in.
 *
 * `Classic` is the standard cube. `HighContrast` keeps the six apart for a
 * deuteranope, for whom the standard cube's warm three collapse into one
 * yellow-brown -- and a cube is read by color alone, so that collapse is not a
 * cosmetic problem but an unreadable board.
 */
enum class Palette { Classic, HighContrast };

/**
 * The only place the domain's color enum turns into pixels.
 *
 * The mapping lives on the graphics side because the domain has no notion of
 * a channel value; cube::FaceColor names a sticker, not a shade. That is also
 * what lets `HighContrast` draw `Orange` as a magenta: the enum names which
 * face a sticker belongs to, and the shade is this function's answer.
 *
 * The palette defaults to `Classic` so that the scenes every existing test was
 * derived from keep asking for the colors they assert. A caller that means the
 * other one says so, and there are three such callers, all in Application.
 */
[[nodiscard]] Color to_color(cube::FaceColor color,
                             Palette palette = Palette::Classic) noexcept;

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
