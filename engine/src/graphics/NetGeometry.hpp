#pragma once

#include <array>
#include <optional>
#include <vector>

#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"
#include "cube/Cubie.hpp"
#include "cube/Surface.hpp"
#include "graphics/ActiveRotation.hpp"
#include "graphics/Palette.hpp"
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

/**
 * The cubie face one net cell displays.
 *
 * Which is a surface sticker and nothing more, so it is the domain's type
 * under a name that reads at the call sites here. A cell of the drawing and a
 * slot of a ring have to be the same thing for a turn to be drawable as
 * motion along the ring.
 */
using NetCell = cube::SurfaceSticker;

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

/** Which cell of the drawing shows a sticker; the inverse of `net_cell`. */
struct NetPosition {
    cube::Face face;
    int col;
    int row;
};

[[nodiscard]] std::optional<NetPosition> net_position(const NetCell& cell,
                                                      int size) noexcept;

/** Side of one unfolded face in the rectangle the net fills. */
[[nodiscard]] float net_face_side(const Rect& rect) noexcept;

/** Side of one cell of an unfolded face. */
[[nodiscard]] float net_cell_side(const Rect& rect, int size) noexcept;

/** Center of one cell of an unfolded face, in the rectangle's space. */
[[nodiscard]] math::Vec2 net_cell_center(cube::Face face, int col, int row,
                                         const Rect& rect, int size) noexcept;

/**
 * One guide line to draw: the path the pressed cell would take.
 *
 * The cell as well as the turn, because the two axes the cross has to cut are
 * drawn as one loop every cell of the band shares, while a Z turn is drawn
 * where it happens and each cell of it goes round its own circle.
 */
struct NetGuide {
    cube::Axis axis;
    int layer;
    NetCell cell;
};

[[nodiscard]] constexpr bool operator==(const NetGuide& a,
                                        const NetGuide& b) noexcept
{
    return a.axis == b.axis && a.layer == b.layer && a.cell == b.cell;
}

/**
 * The net drawn from a colouring of stickers instead of from a cube.
 *
 * What somebody is painting is not a cube yet and mostly will not be one until
 * the last square is right, so there is no `CubeState` to hand this -- and
 * making one would mean building a cube nobody has checked, which is the one
 * thing the domain will not do. A painting is the weaker thing, and this draws
 * the weaker thing.
 *
 * The colours arrive in the order `cube::surface_stickers()` counts them, the
 * same order the reading blames squares in, so that what a refusal points at
 * and what is drawn cannot disagree.
 *
 * `blamed` is outlined rather than filled over: the colour of a square that is
 * being complained about is exactly what its owner needs to see while mending
 * it.
 */
[[nodiscard]] RenderScene build_net_painting(
    const std::vector<cube::FaceColor>& painting, int size, const Rect& rect,
    Palette palette, const std::vector<int>& blamed);

/** Blamed-square outline thickness, as a share of one cell. */
inline constexpr float kNetBlameWidthCells = 0.12f;

/** Guide line thickness, as a share of one cell. */
inline constexpr float kNetGuideWidthCells = 0.14f;

/**
 * The `guides` as strokes over the net drawn in `rect`.
 *
 * The very path its cell rides, so what a press promises and what the turn
 * then does cannot drift apart.
 */
[[nodiscard]] RenderScene build_net_guides(const std::vector<NetGuide>& guides,
                                           const Rect& rect, int size);

/**
 * How a piece on the move is lifted off the cells it passes over.
 *
 * A turn crowds the drawing: a piece on its way between two faces crosses the
 * blocks it goes by, and which of two flat squares of color is on top is not
 * something the squares themselves can say. So a piece the turn is carrying is
 * drawn after every resting cell, a little larger, and over its own shadow --
 * a hand holding it off the page rather than a hole cut in the page for it.
 *
 * The drawing underneath does not move. Opening a margin around a turn was
 * tried at some length and abandoned: every arrangement of it either broke the
 * cross into strips, or slid the net sideways, or shifted the ground under the
 * very piece it was making room for, and none of it read as well as simply
 * letting the piece pass over.
 *
 * All three follow `ActiveRotation::opening`, so they are there for as long as
 * the turn is being made and gone by the time it lands.
 */
inline constexpr float kNetLiftCells = 0.08f;
/**
 * The least a storey pushes its shadow along each axis, in pixels.
 *
 * The renderer's drop shadow moves by whole pixels, and at the largest cubes a
 * cell is a few pixels across, so a distance in cells alone would round to
 * nothing there and the band would fall no further than the face. A pixel a
 * storey keeps the storeys apart at any size -- a quarter over, so that the
 * renderer's angle arithmetic in float cannot bring exactly one down to zero.
 * At ordinary sizes the cell distance is well above this and it never applies.
 */
inline constexpr float kNetLiftMinPixels = 1.25f;
/**
 * The shadow falls diagonally, and its distance is measured along the
 * diagonal: a push of one pixel right and one down is this far.
 */
inline constexpr float kNetLiftDiagonal = 1.41421356f;
/** How much larger a piece is drawn while it is off the page. */
inline constexpr float kNetLiftScale = 0.035f;
/** How dark the shadow goes under a piece. */
inline constexpr float kNetShadowAlpha = 100.0f;
/**
 * The softness of the shadow's edge, as a share of its distance.
 *
 * Half: the blur is of the same order as the offset, so the shadow reads as
 * pushed out and softened rather than as a second, sharp copy of the piece.
 */
inline constexpr float kNetShadowSigmaShare = 0.5f;

/**
 * Which storey of the drawing each part of a turn is held on.
 *
 * Three of them, counting the resting net as the ground. A face that spins in
 * place is one up; the pieces of its band are two, because a piece is a strip
 * of one face crossing another face's block and the eye reads the strip as the
 * thing going over. Drawing them the other way round has a three-by-three
 * block cut across a strip that is meant to be sliding past it.
 *
 * Everything above is a multiple of these: how far its shadow falls, and how
 * much larger it is drawn.
 */
inline constexpr float kNetFaceStorey = 1.0f;
inline constexpr float kNetBandStorey = 2.0f;

/** A turn named by the axis it turns and which way, without a layer mask. */
struct NetTurn {
    cube::Axis axis;
    /** Index of the turning layer along `axis`. */
    int layer;
    /** +1 when the step asks for a positive turn about `axis`. */
    int sign;
};

/**
 * The turn that carries a net cell one cell along a drawn direction.
 *
 * Exactly one of `col_step` and `row_step` is +1 or -1. The step may run off
 * the face, which is the point: both positions are read in cube space, and the
 * cube's surface has no edge, so a direction that leaves the drawing is still
 * a direction on the cube. That is what makes this total, and what lets it
 * work without a table of signs per face.
 *
 * Lives here rather than beside the picking because both the pointer and the
 * drawing need it: one to turn a drag into a move, the other to send a cell
 * the way its sticker goes.
 */
[[nodiscard]] std::optional<NetTurn> net_step_turn(cube::Face face, int col,
                                                   int row, int col_step,
                                                   int row_step,
                                                   int size) noexcept;

/** Builds the net as screen-space quads filling `rect`. */
[[nodiscard]] RenderScene build_net_scene(const cube::CubeState& state,
                                          const Rect& rect,
                                          Palette palette = Palette::Classic);

/**
 * The same with a turn in progress drawn into it.
 *
 * `nullopt` is the resting net above, so a caller that always has an optional
 * to hand never has to branch. The angle is not capped: the rings wrap, so a
 * drag carried past a quarter keeps going round the loop it is on.
 */
[[nodiscard]] RenderScene build_net_scene(
    const cube::CubeState& state, const Rect& rect,
    const std::optional<ActiveRotation>& active,
    Palette palette = Palette::Classic);

}  // namespace rubiks::graphics
