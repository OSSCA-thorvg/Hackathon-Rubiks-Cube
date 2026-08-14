#include "graphics/Layout.hpp"

#include <algorithm>

namespace rubiks::graphics {
namespace {

/** A stack of regions being laid out, one taken off the top at a time. */
struct Stack {
    float face_side;
    /** Top of the next region to be taken; moves down as they are.  */
    float cursor;
    /** Horizontal middle of the canvas, which every region is centred on. */
    float middle;
};

/**
 * Sizes a stack of regions, all measured against the net's face.
 *
 * `squares` is how many square regions ride with the net; the net itself is
 * always there, because it is the only part of a stack whose proportions are
 * not ours to choose. The whole stack then grows until the canvas runs out
 * one way or the other.
 */
[[nodiscard]] Stack stacked(float canvas_width, float canvas_height,
                            int squares) noexcept
{
    const float tall = static_cast<float>(kNetRows) +
                       static_cast<float>(squares) *
                           (kStackSquareFaces + kStackGapFaces);
    const float face_side =
        std::min(kStackWidthFraction * canvas_width /
                     static_cast<float>(kNetColumns),
                 kStackHeightFraction * canvas_height / tall);

    return Stack{face_side, (canvas_height - tall * face_side) * 0.5f,
                 canvas_width * 0.5f};
}

/** Takes the next region off a stack, centred, and steps past it. */
[[nodiscard]] Rect take_row(Stack& stack, float width, float height) noexcept
{
    const Rect row{stack.middle - 0.5f * width, stack.cursor, width, height};
    stack.cursor += height + stack.face_side * kStackGapFaces;
    return row;
}

}  // namespace

CanvasLayout layout(std::uint32_t width, std::uint32_t height, ViewMode mode,
                    FlatStyle style) noexcept
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
        case ViewMode::Flat: {
            if (style == FlatStyle::Both) {
                // The net over the rings, which is the order they were built
                // in and the order a turn is easiest to follow between them.
                Stack stack = stacked(canvas_width, canvas_height, 1);
                const float square = stack.face_side * kStackSquareFaces;

                placement.net = take_row(
                    stack, stack.face_side * static_cast<float>(kNetColumns),
                    stack.face_side * static_cast<float>(kNetRows));
                placement.rings = take_row(stack, square, square);
                return placement;
            }

            if (style == FlatStyle::Rings) {
                const float side = kRingsOnlyRegionSide * unit;
                placement.rings =
                    Rect{(canvas_width - side) * 0.5f,
                         (canvas_height - side) * 0.5f, side, side};
                return placement;
            }

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

    if (style == FlatStyle::Both) {
        // All three, in the order they were built: the cube, then its faces
        // laid flat, then the cycles those faces turn on.
        Stack stack = stacked(canvas_width, canvas_height, 2);
        const float square = stack.face_side * kStackSquareFaces;

        placement.cube = take_row(stack, square, square);
        placement.net = take_row(
            stack, stack.face_side * static_cast<float>(kNetColumns),
            stack.face_side * static_cast<float>(kNetRows));
        placement.rings = take_row(stack, square, square);
        return placement;
    }

    if (style == FlatStyle::Rings) {
        const float cube_side = kCubeWithRingsSide * unit;
        const float rings_side = kRingsWithCubeSide * unit;

        placement.cube = Rect{(canvas_width - cube_side) * 0.5f,
                              kCubeRegionTop * unit, cube_side, cube_side};
        placement.rings = Rect{(canvas_width - rings_side) * 0.5f,
                               kRingsWithCubeTop * unit, rings_side,
                               rings_side};
        return placement;
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
