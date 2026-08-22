#include "graphics/CanvasTheme.hpp"

namespace rubiks::graphics {

Color canvas_background(CanvasTheme theme) noexcept
{
    if (theme == CanvasTheme::Light) {
        return Color{231, 235, 240, 255};
    }
    return Color{32, 32, 32, 255};
}

}  // namespace rubiks::graphics
