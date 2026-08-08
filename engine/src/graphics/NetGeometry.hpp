#pragma once

#include <array>

#include "cube/CubeState.hpp"
#include "cube/Cubie.hpp"
#include "graphics/Rect.hpp"
#include "graphics/RenderScene.hpp"

namespace rubiks::graphics {

/**
 * The unfolded cube: all six faces laid flat so the whole state is visible.
 *
 * The net goes straight from the domain to screen-space quads without the
 * pipeline. It is already two-dimensional, nothing occludes anything, and
 * there is no perspective, so model, view, projection, culling and depth
 * sorting would all be identity. That the result still slots into the same
 * RenderScene is a consequence of keeping the renderer boundary purely 2D.
 */

/** How much of its cell a net sticker fills; the rest reads as background. */
inline constexpr float kNetStickerScale = 0.88f;

/** Position of a face in the 4-wide, 3-tall cross. */
struct NetBlock {
    int column;
    int row;
};

/** The cubie face one net cell displays. */
struct NetCell {
    int x;
    int y;
    int z;
    cube::Face face;
};

/** The six faces in the order they are emitted, forming a standard cross. */
[[nodiscard]] std::array<cube::Face, 6> net_faces() noexcept;

[[nodiscard]] NetBlock net_block(cube::Face face) noexcept;

/**
 * Maps a cell of an unfolded face to the cubie face it shows.
 *
 * `col` and `row` are 0 ... N-1 within the face, counted left to right and
 * top to bottom as drawn. The mapping is the standard unfolding with Front
 * facing the viewer, chosen so that cells meeting at a seam in the net are
 * the two faces of the same cubie edge on the real cube.
 */
[[nodiscard]] NetCell net_cell(cube::Face face, int col, int row,
                               int size) noexcept;

/** Builds the net as screen-space quads filling `rect`. */
[[nodiscard]] RenderScene build_net_scene(const cube::CubeState& state,
                                          const Rect& rect);

}  // namespace rubiks::graphics
