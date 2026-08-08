#include "graphics/Layout.hpp"

#include <algorithm>

namespace rubiks::graphics {

CanvasLayout layout(std::uint32_t width, std::uint32_t height) noexcept
{
    const auto canvas_width = static_cast<float>(width);
    const auto canvas_height = static_cast<float>(height);
    const float unit = std::min(canvas_width, canvas_height);

    const float cube_side = kCubeRegionSide * unit;
    const float face_side = kNetFaceSide * unit;
    const float net_width = face_side * static_cast<float>(kNetColumns);
    const float net_height = face_side * static_cast<float>(kNetRows);

    CanvasLayout placement;
    placement.cube = Rect{(canvas_width - cube_side) * 0.5f,
                          kCubeRegionTop * unit, cube_side, cube_side};
    placement.net = Rect{(canvas_width - net_width) * 0.5f, kNetTop * unit,
                         net_width, net_height};
    return placement;
}

}  // namespace rubiks::graphics
