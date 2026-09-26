#pragma once

#include <cstdint>

#include "graphics/Rect.hpp"

namespace rubiks::graphics {

/**
 * Which regions of the canvas are drawn into.
 *
 * Deliberately not a list of every picture the app can show. Which regions are
 * up and which drawing fills the flat one are independent, so they are two
 * enums rather than one whose entries multiply: a single list has no room for
 * "the cube and the ring diagram" without splitting Both in two, and splitting
 * it again for every flat drawing added after that.
 */
enum class ViewMode : std::uint8_t {
    Cube3D = 0,
    Both = 1,
    Flat = 2,
};

/**
 * Which drawing fills the flat region, wherever that region is.
 *
 * `Both` stacks them, the net over the rings. They are two views of the same
 * fifty-four stickers -- one laid out as faces and one as the cycles a turn
 * runs on -- so watching a turn cross between them is the point of having it.
 */
enum class FlatStyle : std::uint8_t {
    Net = 0,
    Rings = 1,
    Both = 2,
};

/**
 * Where the views of the cube sit in the drawing buffer.
 *
 * All measurements are fractions of the shorter canvas side, so the split
 * keeps its proportions on any aspect ratio, and both regions are centered
 * horizontally.
 *
 * The cube region is deliberately square. That pins the camera aspect at 1
 * regardless of the canvas shape, which is what lets the rendered scene
 * contract state its sample points as fractions of the cube region instead of
 * restricting the whole contract to square canvases.
 */
struct CanvasLayout {
    Rect cube;
    Rect net;
    Rect rings;
};

[[nodiscard]] constexpr bool operator==(const CanvasLayout& a,
                                        const CanvasLayout& b) noexcept
{
    return a.cube == b.cube && a.net == b.net && a.rings == b.rings;
}

/** Side of the square 3D region. */
inline constexpr float kCubeRegionSide = 0.58f;
/** Gap above the 3D region. */
inline constexpr float kCubeRegionTop = 0.01f;
/** Side of one net face; the net is 4 of these across and 3 down. */
inline constexpr float kNetFaceSide = 0.12f;
/** Gap above the net block. */
inline constexpr float kNetTop = 0.62f;

/** Side of the square cube region when it is the only visible view. */
inline constexpr float kCubeOnlyRegionSide = 0.84f;

/**
 * The 3D region and the ring diagram sharing the canvas.
 *
 * Their own set of numbers, because the ring diagram is very nearly square
 * while the net is a wide 4-by-3 band. Dropped into the net's band the diagram
 * would be penned in by the short side and come out much smaller than the net
 * it replaced, so the cube gives up some height for it instead.
 */
inline constexpr float kCubeWithRingsSide = 0.44f;
inline constexpr float kRingsWithCubeSide = 0.48f;
inline constexpr float kRingsWithCubeTop = 0.49f;

/**
 * Regions stacked one above another, measured in net face sides.
 *
 * Everything in a stack is sized against the net's own face, because the net
 * is the one part with a shape it cannot give up: four across and three down.
 * The square regions are given the net's height so no one of them dominates,
 * and the whole stack then grows until the canvas runs out one way or other.
 */
inline constexpr float kStackGapFaces = 0.30f;
inline constexpr float kStackSquareFaces = 3.0f;
inline constexpr float kStackWidthFraction = 0.92f;
inline constexpr float kStackHeightFraction = 0.96f;
/** Maximum fraction of the canvas width used by a net-only block. */
inline constexpr float kNetOnlyWidth = 0.88f;
/** Maximum fraction of the canvas height used by a net-only block. */
inline constexpr float kNetOnlyHeight = 0.78f;

/**
 * Side of the square the ring diagram is drawn in, when it is the only view.
 *
 * Square because the diagram is a disc: its nine circles reach the same
 * distance whichever way you measure, so anything but a square would leave the
 * long side empty.
 */
inline constexpr float kRingsOnlyRegionSide = 0.92f;

inline constexpr int kNetColumns = 4;
inline constexpr int kNetRows = 3;

[[nodiscard]] CanvasLayout layout(std::uint32_t width, std::uint32_t height,
                                  ViewMode mode = ViewMode::Both,
                                  FlatStyle style = FlatStyle::Net) noexcept;

}  // namespace rubiks::graphics
