#pragma once

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
