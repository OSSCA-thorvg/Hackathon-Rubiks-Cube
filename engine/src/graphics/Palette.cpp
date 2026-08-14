#include "graphics/Palette.hpp"

namespace rubiks::graphics {

Color to_color(cube::FaceColor color) noexcept
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

}  // namespace rubiks::graphics
