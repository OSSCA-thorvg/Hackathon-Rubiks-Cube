#pragma once

#include <optional>

#include "cube/CubeMove.hpp"
#include "cube/Face.hpp"
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
 * The palette an integer names, or nothing when it names none.
 *
 * For a value that arrives as a primitive across the C ABI, and for a caller
 * handed an enum it cannot vouch for; the one place the question is answered
 * for this enum. The switch has no default on purpose, so a palette added to
 * the enum and not here is a warning from the compiler rather than a value
 * refused in silence.
 */
[[nodiscard]] constexpr std::optional<Palette> palette_from(int value) noexcept
{
    const auto named = static_cast<Palette>(value);
    if (static_cast<int>(named) != value) return std::nullopt;

    switch (named) {
        case Palette::Classic:
        case Palette::HighContrast:
            return named;
    }
    return std::nullopt;
}

/**
 * The only place the domain's color enum turns into pixels.
 *
 * The mapping lives on the graphics side because the domain has no notion of
 * a channel value; cube::FaceColor names a sticker, not a shade. That is also
 * what lets `HighContrast` draw `Orange` as a magenta: the enum names which
 * face a sticker belongs to, and the shade is this function's answer.
 *
 * The palette defaults to `Classic` so that the scenes every existing test was
 * derived from keep asking for the colors they assert. The engine's own
 * callers are the scene builders of the cube, the net and the rings, and each
 * of them passes the palette it was handed.
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

/**
 * The colours the keyboard's place on a painting is ringed in: the page's own
 * focus blue, over a dark edge.
 *
 * The blue is the one a focused control on the page is outlined in, so the
 * place reads as where the keyboard is. The edge under it is what lets it read
 * on white and yellow stickers as well as on blue ones.
 */
[[nodiscard]] Color cursor_color() noexcept;
[[nodiscard]] Color cursor_edge_color() noexcept;

}  // namespace rubiks::graphics
