#include "graphics/Palette.hpp"

namespace rubiks::graphics {

namespace {

Color classic(cube::FaceColor color) noexcept
{
    switch (color) {
        case cube::FaceColor::Red:
            return Color{183, 18, 52, 255};
        case cube::FaceColor::Orange:
            return Color{255, 88, 0, 255};
        case cube::FaceColor::White:
            return Color{255, 255, 255, 255};
        case cube::FaceColor::Yellow:
            return Color{255, 213, 0, 255};
        case cube::FaceColor::Green:
            return Color{0, 155, 72, 255};
        case cube::FaceColor::Blue:
            break;
    }
    return Color{0, 70, 173, 255};
}

/**
 * The same six faces, chosen so no pair collapses for any dichromat.
 *
 * Picked by measuring rather than by eye: each candidate set was put through
 * the protanopia, deuteranopia and tritanopia projections and scored by the
 * smallest CIELAB distance among its fifteen pairs under *all three*. The
 * standard cube scores 12 that way and this scores 40, and the test beside
 * these values fixes that property rather than the numbers themselves.
 *
 * All three had to be scored together because the first two lose the opposite
 * axis to the third. A set spread along blue-yellow reads beautifully to a
 * deuteranope and collapses for a tritanope, and the first draft of this
 * palette did exactly that -- it beat the standard cube for red-green
 * deficiencies and came out *worse* than it for blue-yellow. The one
 * dimension no deficiency takes away is lightness, so the set is first of all
 * a ladder of it, and hue does the separating that is left over.
 *
 * Reading the ladder from the top: White, then Yellow as an amber and Green as
 * a turquoise near each other in lightness but at opposite ends of what a
 * dichromat still sees, then Orange as a dull plum, then Red and Blue at the
 * bottom, apart because no deficiency touches how dark a colour is.
 *
 * Two things the measurement contradicted are worth keeping written down. Red
 * and Orange are the pair a sighted person assumes is the problem, but on the
 * standard cube Yellow/Orange collapses first. And protanopia needs no
 * separate design: it and deuteranopia lose the same axis, so the set that
 * clears one clears the other -- it is tritanopia, which nobody expects,
 * that pulls against them.
 */
Color high_contrast(cube::FaceColor color) noexcept
{
    switch (color) {
        case cube::FaceColor::Red:
            return Color{120, 0, 0, 255};
        case cube::FaceColor::Orange:
            return Color{128, 64, 107, 255};
        case cube::FaceColor::White:
            return Color{255, 255, 255, 255};
        case cube::FaceColor::Yellow:
            return Color{240, 153, 10, 255};
        case cube::FaceColor::Green:
            return Color{56, 224, 208, 255};
        case cube::FaceColor::Blue:
            break;
    }
    return Color{0, 0, 123, 255};
}

}  // namespace

Color to_lit_color(cube::FaceColor color, Palette palette) noexcept
{
    // Paper white rather than 255: the lit view multiplies a sticker's colour
    // by a brightness that passes 1 on the face towards the key, and a 255
    // white clamps there -- flat, and with no room for a glint to be brighter
    // than the sticker it lands on. At 216 the top face shades like the
    // others (220 to 246 across it at the home view) and a reflection still
    // has somewhere to go. Only the classic white: the high-contrast set is
    // drawn unlit and must come back byte for byte, and the flat views take
    // to_color() and stay as bright as the page.
    if (palette == Palette::Classic && color == cube::FaceColor::White) {
        return kPaperWhite;
    }
    return to_color(color, palette);
}

Color to_color(cube::FaceColor color, Palette palette) noexcept
{
    return palette == Palette::HighContrast ? high_contrast(color)
                                            : classic(color);
}

Color guide_color(cube::Axis axis) noexcept
{
    switch (axis) {
        case cube::Axis::X:
            return Color{0, 224, 255, 235};
        case cube::Axis::Y:
            return Color{255, 72, 208, 235};
        case cube::Axis::Z:
            break;
    }
    return Color{176, 255, 64, 235};
}

Color blame_color() noexcept
{
    return Color{250, 250, 250, 245};
}

}  // namespace rubiks::graphics
