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

/** The classic white as an object under light, for the 3D view alone. */
inline constexpr Color kPaperWhite{216, 216, 216, 255};

/**
 * The colour a sticker has as a lit object, for the 3D view.
 *
 * The same as to_color() except that the classic white is kPaperWhite, so
 * the lit face towards the key can be brighter than the sticker's own colour
 * and a glint has room above it. The flat views draw to_color() as it is.
 */
[[nodiscard]] Color to_lit_color(cube::FaceColor color,
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

/**
 * The colour a square is ringed in when a painting is being complained about.
 *
 * Near-white and nearly opaque, which is the one thing left after the six
 * stickers and the three guides have taken their hues: a complaint has to read
 * over any of the six it might land on, and must not be mistaken for a fourth
 * axis. Its job is to be seen and to leave the colour underneath visible,
 * since that colour is exactly what its owner is being asked to look at.
 */
[[nodiscard]] Color blame_color() noexcept;

}  // namespace rubiks::graphics
