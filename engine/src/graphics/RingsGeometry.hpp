#pragma once

#include <optional>
#include <vector>

#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"
#include "cube/Surface.hpp"
#include "graphics/ActiveRotation.hpp"
#include "graphics/Palette.hpp"
#include "graphics/Rect.hpp"
#include "graphics/RenderScene.hpp"
#include "graphics/SlotRing.hpp"
#include "math/Types.hpp"

namespace rubiks::graphics {

/**
 * The cube as the rings a turn can cycle, rather than as six faces.
 *
 * Each axis becomes one centre and each of its layers one circle about that
 * centre, so 3N circles in all. Every sticker belongs to exactly two of them
 * -- one for each axis that is not its face's own -- and is drawn where those
 * two circles cross. On a 3x3 that is nine rings of twelve slots: a hundred
 * and eight memberships, two apiece over fifty-four stickers, and the equality
 * closing is the whole reason the picture exists.
 *
 * A turn is then what it actually is: three slots along one circle. A sticker
 * carried round arrives at a crossing where its *other* membership has changed
 * -- it has stepped off one circle onto another -- which is the thing neither
 * the 3D cube nor the unfolded net can show.
 *
 * Which cycle runs through which slots is `cube::ring_slots()`, the same
 * screen-free answer the net rides. All this adds is where the slots are.
 */

/** Distance from the middle to each axis's centre, in centre separations. */
inline constexpr float kRingCenterDistance = 0.5773502692f;

/**
 * How far the outer layers' circles sit either side of the middle one.
 *
 * The middle circle has the radius of the centre separation exactly, so it
 * passes through the other two centres, and the layers either side of it are
 * this much larger and smaller.
 *
 * Not a free choice. Each ring's twelve slots have to come round its own centre
 * in the order the domain cycles them, or the loop through them crosses itself,
 * and that only happens when one family's outer circle reaches inside another's
 * inner circle. The band that works is 0.005 to 0.295; this is where the fifty
 * four slots are furthest apart inside it.
 */
inline constexpr float kRingRadiusOffset = 0.24f;

/** How much of that gap a sticker fills; the rest reads as background. */
inline constexpr float kRingsStickerScale = 0.62f;

/** Ring thickness, as a share of the same gap. */
inline constexpr float kRingsStrokeScale = 0.10f;

/** How much thicker the rings a turn is running along are drawn. */
inline constexpr float kRingsActiveStrokeScale = 2.6f;

/**
 * How close a press has to fall to count as landing on a slot.
 *
 * As a share of the gap between the two nearest slots, so half of it is the
 * point where two slots would start competing. A little under that leaves a
 * press that lands squarely between two of them a miss rather than a coin toss.
 */
inline constexpr float kRingsPickReach = 0.45f;

/**
 * A ring the diagram should pick out, because a press is offering it.
 *
 * No cell, unlike the net's guides: the loops are already drawn, so offering
 * one is a matter of weight rather than of another line over the top.
 */
struct RingsGuide {
    cube::Axis axis;
    int layer;
};

/**
 * Where a sticker rests in the diagram drawn in `rect`.
 *
 * Nothing when the layout does not place it, which for a well-formed cube
 * cannot happen and is a guard rather than a case.
 */
[[nodiscard]] std::optional<math::Vec2> rings_slot_position(
    const cube::SurfaceSticker& sticker, const Rect& rect, int size);

/**
 * Where an axis's loops are centred, in the diagram drawn in `rect`.
 *
 * The slots on one loop are nowhere near evenly spaced, so their middle is not
 * this point and nothing can recover it from the loop alone.
 *
 * The size is asked for because the whole figure is fitted to the rectangle
 * around the widest stroke it will draw, and how thick that is follows from
 * how close the slots of that cube come.
 */
[[nodiscard]] math::Vec2 rings_axis_center(cube::Axis axis, const Rect& rect,
                                           int size);

/**
 * One of the nine loops, laid over the diagram drawn in `rect`.
 *
 * A knot on every slot carrying that circle's own tangent, and more between
 * them wherever the slots leave a wide gap. `arc_arm` bends each cubic into an
 * arc, so the finished loop is the circle itself rather than something threaded
 * through the slots -- as long as no one piece is asked to span too much of it.
 */
[[nodiscard]] SlotRing rings_ring(cube::Axis axis, int layer, const Rect& rect,
                                  int size);

/**
 * How far apart the two closest slots fall, in the diagram drawn in `rect`.
 *
 * The one measure everything in the diagram is sized against, and the only way
 * to ask what the layout scaled to without measuring two centres apart and
 * dividing -- which is what a caller was reduced to.
 *
 * It shrinks as a cube grows: the loops of one axis share the same band of
 * radii however many of them there are, so more layers means more crossings in
 * the same figure. Everything drawn and everything picked follows it, which is
 * what keeps a 9x9 diagram from drawing its stickers over one another.
 */
[[nodiscard]] float rings_slot_spacing(const Rect& rect, int size);

/**
 * Which way a positive turn carries the sticker resting in one of a ring's
 * slots, as a unit vector on the screen.
 *
 * Nothing when that ring does not carry it. Answering this without building
 * the whole loop matters because a press only ever wants the direction: the
 * curve between the slots has no say in it.
 */
[[nodiscard]] std::optional<math::Vec2> rings_slot_tangent(
    cube::Axis axis, int layer, const Rect& rect, int size,
    const cube::SurfaceSticker& sticker);

/**
 * The fractional slot index a point lies at, going round the ring's centre.
 *
 * The inverse of sampling the loop, and what a drag on this view is measured
 * in. Degrees would not do: the twelve slots are anything from fourteen to a
 * hundred and sixty-eight degrees apart, and the drawing moves a sticker by
 * slot index, so a finger and the sticker it grabbed would come apart.
 *
 * Nothing when the ring is empty; otherwise always an answer, since a point
 * has an angle about the centre wherever it is.
 */
[[nodiscard]] std::optional<float> rings_slot_at(cube::Axis axis, int layer,
                                                 const Rect& rect, int size,
                                                 const math::Vec2& point);

/** Builds the ring diagram as screen-space quads and strokes filling `rect`. */
[[nodiscard]] RenderScene build_rings_scene(const cube::CubeState& state,
                                            const Rect& rect,
                                            Palette palette = Palette::Classic);

/**
 * The same with a turn in progress drawn into it.
 *
 * `nullopt` is the resting diagram above. Stickers stay axis-aligned however
 * far round they are carried: the net can tilt a cell against the direction its
 * ring set off in because every one of those directions is square to the
 * screen, and here none of them is.
 */
[[nodiscard]] RenderScene build_rings_scene(
    const cube::CubeState& state, const Rect& rect,
    const std::optional<ActiveRotation>& active,
    const std::vector<RingsGuide>& guides = {},
    Palette palette = Palette::Classic);

}  // namespace rubiks::graphics
