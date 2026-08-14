#pragma once

#include "cube/CubeMove.hpp"
#include "graphics/Rect.hpp"
#include "graphics/SlotRing.hpp"

namespace rubiks::graphics {

/**
 * The loop a layer's band travels along in the unfolded net.
 *
 * No flat unfolding keeps all three axes contiguous, so rather than a different
 * drawing per axis, every band is one `SlotRing`: 4N resting slots threaded
 * onto a closed loop, walked a face's width per quarter turn.
 *
 * The net's loop is a rounded polyline, not a curve fitted through the slots.
 * Its straight parts carry the slots and its corners are exact circular arcs,
 * all of one radius -- as wide as the loop's tightest corner allows. That is
 * what makes it read as one clean shape, rather than one rounded three cells at
 * the end where a band is cut and half a cell where it drops back onto a slot.
 *
 * It is also what keeps a cell turning one way: along a straight run the
 * tangent does not move at all, and through an arc it sweeps steadily from one
 * end to the other. A curve pulled through the slots instead has to bow either
 * side of each of them, and a cell riding it rocks backwards before it turns.
 */

/**
 * How far past the net a loop reaches where the cross leaves its band cut.
 *
 * In cells. It is also most of how far a cell rounding a cut is carried
 * outside the drawing, which is what a turn is allowed to overhang by.
 */
inline constexpr float kNetBreakReachCells = 0.30f;

/**
 * The ring one layer's band follows, laid over the net drawn in `rect`.
 *
 * Empty when the layer turns nothing that the net draws.
 */
[[nodiscard]] SlotRing net_ring(cube::Axis axis, int layer, const Rect& rect,
                                int size);

}  // namespace rubiks::graphics
