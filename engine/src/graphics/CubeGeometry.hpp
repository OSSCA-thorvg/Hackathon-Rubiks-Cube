#pragma once

#include "cube/CubeState.hpp"
#include "cube/Cubie.hpp"
#include "graphics/Color.hpp"
#include "graphics/Scene.hpp"
#include "math/Types.hpp"

namespace rubiks::graphics {

/** Half the edge length of the whole cube, whatever N is. */
inline constexpr float kCubeHalfExtent = 1.0f;

/**
 * How much of its grid cell a sticker fills.
 *
 * The remainder is the seam. Because only surface stickers are emitted and
 * cubies have no body faces, a seam always shows the background color, which
 * is what makes seam pixels a decisive check rather than an approximate one.
 */
inline constexpr float kStickerScale = 0.92f;

/** Distance from a cubie's center to its sticker plane. */
[[nodiscard]] float sticker_half_extent(int size) noexcept;

/** Center of the cubie at index `i` along one axis, for an N of `size`. */
[[nodiscard]] float cubie_center(int i, int size) noexcept;

/**
 * Appends one sticker quad, wound counter-clockwise as seen from outside.
 *
 * Back-face culling depends on that winding, so this function is the single
 * place the six corner orders are written down.
 */
void append_sticker(WorldScene& scene, const math::Vec3& center,
                    float half_extent, cube::Face face, const Color& color);

/**
 * Builds the world-space scene for a cube state.
 *
 * Only stickers on the outside of the cube are emitted: a cubie's face is
 * visible exactly when that cubie sits in the outermost layer along the
 * face's axis. At N = 3 that is 54 quads, of which the fixed camera keeps 27.
 */
[[nodiscard]] WorldScene build_cube_scene(const cube::CubeState& state);

}  // namespace rubiks::graphics
