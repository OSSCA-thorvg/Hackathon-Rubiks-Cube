#include "graphics/Layout.hpp"

#include <algorithm>

namespace rubiks::graphics {

CanvasLayout layout(std::uint32_t width, std::uint32_t height,
                    ViewMode mode) noexcept
{
    const auto canvas_width = static_cast<float>(width);
    const auto canvas_height = static_cast<float>(height);
    const float unit = std::min(canvas_width, canvas_height);

    // A region left at its default is not rendered, so each mode only fills
    // in the views it shows.
    CanvasLayout placement;

    switch (mode) {
        case ViewMode::Cube3D: {
            const float side = kCubeOnlyRegionSide * unit;
            placement.cube = Rect{(canvas_width - side) * 0.5f,
                                  (canvas_height - side) * 0.5f, side, side};
            return placement;
        }
        case ViewMode::Net: {
            // The net alone is free to use both canvas extents, so it grows
            // until whichever one runs out first.
            const float face_side = std::min(
                kNetOnlyWidth * canvas_width / static_cast<float>(kNetColumns),
                kNetOnlyHeight * canvas_height /
                    static_cast<float>(kNetRows));
            const float net_width = face_side * static_cast<float>(kNetColumns);
            const float net_height = face_side * static_cast<float>(kNetRows);
            placement.net = Rect{(canvas_width - net_width) * 0.5f,
                                 (canvas_height - net_height) * 0.5f,
                                 net_width, net_height};
            return placement;
        }
        case ViewMode::Both:
            break;
    }

    const float cube_side = kCubeRegionSide * unit;
    const float face_side = kNetFaceSide * unit;
    const float net_width = face_side * static_cast<float>(kNetColumns);
    const float net_height = face_side * static_cast<float>(kNetRows);

    placement.cube = Rect{(canvas_width - cube_side) * 0.5f,
                          kCubeRegionTop * unit, cube_side, cube_side};
    placement.net = Rect{(canvas_width - net_width) * 0.5f, kNetTop * unit,
                         net_width, net_height};
    return placement;
}

}  // namespace rubiks::graphics
