#pragma once

#include <vector>

#include "cube/CubeMove.hpp"
#include "cube/Cubie.hpp"

namespace rubiks::cube {

/**
 * The cube's surface as named stickers, and what a quarter turn does to them.
 *
 * `CubeState` answers "what color is at this position", which is the question
 * rendering a still frame asks. Drawing a turn in progress asks a different
 * one: where does *this* sticker go. Color cannot answer it — six colors over
 * fifty-four stickers, so an error that permutes stickers within one face
 * reads as correct. So a sticker gets an identity here, and the permutation
 * that carries it is the same one `CubeState` already turns cubies with.
 *
 * Screen-free on purpose: this is a property of the cube, not of any view of
 * it, and both the unfolded net and the ring diagram lay coordinates over the
 * same cycles.
 */

/** A cubie slot as index coordinates; each of x, y, z is 0 ... N-1. */
struct CubiePosition {
    int x;
    int y;
    int z;
};

/** One sticker of the surface: which cubie, and which of its faces. */
struct SurfaceSticker {
    int x;
    int y;
    int z;
    Face face;
};

[[nodiscard]] constexpr bool operator==(const CubiePosition& a,
                                        const CubiePosition& b) noexcept
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

[[nodiscard]] constexpr bool operator==(const SurfaceSticker& a,
                                        const SurfaceSticker& b) noexcept
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.face == b.face;
}

/** The axis a face points along. */
[[nodiscard]] Axis axis_of(Face face) noexcept;

/** The layer along a face's own axis where that face is the outer surface. */
[[nodiscard]] int outer_layer(Face face, int size) noexcept;

/**
 * The layers a run of depths measured from `face` selects.
 *
 * Depth 1 is the face itself and depths grow inwards, which is how every
 * notation of a slice or a wide move reads: `R` is depth 1, `Rw` is 1 through
 * 2, `2R` is 2 through 2. Written here rather than at either caller because
 * the two callers are inverses of each other -- a command turns a depth range
 * into layers and a move log turns layers back into one -- and a convention
 * with two implementations is a convention with two chances to differ.
 *
 * Which end of the axis a face sits at is the whole of the difference between
 * the two directions, and `outer_layer()` above already answers it.
 *
 * @return zero for a run that is empty, reversed, or reaches past the far
 *         side, which is the mask no move can have.
 */
[[nodiscard]] LayerMask depth_layers(Face face, int first_depth, int last_depth,
                                     int size) noexcept;

[[nodiscard]] int coordinate_on(Axis axis, const CubiePosition& p) noexcept;
[[nodiscard]] int coordinate_on(Axis axis, const SurfaceSticker& s) noexcept;

/** Where a cubie lands after one positive quarter turn about `axis`. */
[[nodiscard]] CubiePosition turned_position(Axis axis, const CubiePosition& p,
                                            int size) noexcept;

/**
 * The face a sticker shows from after one positive quarter turn about `axis`.
 *
 * The two faces perpendicular to the axis keep their sticker, so they map to
 * themselves.
 */
[[nodiscard]] Face turned_face(Axis axis, Face face) noexcept;

/** Where a sticker is after one positive quarter turn about `axis`. */
[[nodiscard]] SurfaceSticker turned_sticker(const SurfaceSticker& sticker,
                                            Axis axis, int size) noexcept;

/**
 * Every sticker of the cube's surface, once each, face by face.
 *
 * Screen-free like the rest of this header, which is why it lives here rather
 * than beside one of the two views that draw them: both need to walk all 6N^2
 * of them, and a drawing has no say in which ones exist.
 *
 * Empty for a size below 1.
 */
[[nodiscard]] std::vector<SurfaceSticker> surface_stickers(int size);

/**
 * The 4N stickers one layer cycles, in the order the positive turn moves them.
 *
 * Consecutive slots are neighbours on the surface, so the sequence is a closed
 * ring that a drawing can lay a curve along. A quarter turn advances every
 * sticker by exactly N slots, which is what makes one turn a quarter of the
 * ring whatever the axis.
 *
 * Only this one canonical direction is returned; a negative turn is the same
 * ring walked backwards, which is the caller's index arithmetic rather than
 * a second cycle to keep in step.
 *
 * Empty for a size below 2 or a layer outside the cube.
 */
[[nodiscard]] std::vector<SurfaceSticker> ring_slots(Axis axis, int layer,
                                                     int size);

}  // namespace rubiks::cube
