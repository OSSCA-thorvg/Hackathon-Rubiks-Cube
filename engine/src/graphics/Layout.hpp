#pragma once

#include <cstdint>

#include "graphics/Rect.hpp"

namespace rubiks::graphics {

/**
 * Where the two views of the cube sit in the drawing buffer.
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
};

/** Side of the square 3D region. */
inline constexpr float kCubeRegionSide = 0.58f;
/** Gap above the 3D region. */
inline constexpr float kCubeRegionTop = 0.01f;
/** Side of one net face; the net is 4 of these across and 3 down. */
inline constexpr float kNetFaceSide = 0.12f;
/** Gap above the net block. */
inline constexpr float kNetTop = 0.62f;

inline constexpr int kNetColumns = 4;
inline constexpr int kNetRows = 3;

[[nodiscard]] CanvasLayout layout(std::uint32_t width,
                                  std::uint32_t height) noexcept;

}  // namespace rubiks::graphics
