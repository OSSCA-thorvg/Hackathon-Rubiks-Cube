#pragma once

#include <optional>

#include "graphics/Color.hpp"

namespace rubiks::graphics {

/**
 * The neutral ground the software canvas is painted with.
 *
 * Two values rather than the three a person chooses from: `System` is a
 * question about the machine, and it is answered before anything reaches
 * here. What the renderer needs is one color, and what names it is one of
 * two grounds.
 *
 * This is not the sticker palette. A palette is six shades a cube is read
 * by; a theme is the one surface the cube is read against. The two axes are
 * independent and every combination of them has to stay legible.
 */
enum class CanvasTheme { Light, Dark };

/**
 * The theme an integer names, or nothing when it names none.
 *
 * For a value that arrives as a primitive across the C ABI, and for a caller
 * handed an enum it cannot vouch for; the one place the question is answered
 * for this enum. The switch has no default on purpose, so a theme added to
 * the enum and not here is a warning from the compiler rather than a value
 * refused in silence.
 */
[[nodiscard]] constexpr std::optional<CanvasTheme> canvas_theme_from(
    int value) noexcept
{
    const auto named = static_cast<CanvasTheme>(value);
    if (static_cast<int>(named) != value) return std::nullopt;

    switch (named) {
        case CanvasTheme::Light:
        case CanvasTheme::Dark:
            return named;
    }
    return std::nullopt;
}

/**
 * The opaque background a theme asks for.
 *
 * Light is not white. A white ground would take the white stickers with it
 * and leave a cube with a hole in one face, so it is a light neutral that
 * the white face still stands out of and that the black seams still read
 * against.
 *
 * Dark keeps the value the renderer has always cleared to, so the pixel
 * contract every existing scene test was written against is unchanged.
 */
[[nodiscard]] Color canvas_background(CanvasTheme theme) noexcept;

}  // namespace rubiks::graphics
