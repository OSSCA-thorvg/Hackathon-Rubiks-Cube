#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "cube/Surface.hpp"
#include "graphics/Color.hpp"
#include "graphics/RenderScene.hpp"
#include "math/Types.hpp"

namespace rubiks::graphics {

/**
 * The closed curve a layer's band travels along in some drawing of the cube.
 *
 * A layer turn is a rigid motion on the cube but a permutation on a flat
 * drawing of it. Rather than a different treatment per axis or per view, every
 * band is the same thing: 4N resting slots threaded onto one closed loop,
 * walked a quarter of the way round per quarter turn. What differs is only the
 * shape of the loop, which is data -- and where that shape comes from is the
 * view's business, not this class's.
 *
 * A loop arrives as knots with the directions the curve runs into and out of
 * them, and leaves as cubic segments, a point for any slot index, and a stroke.
 * The unfolded net rounds a polyline into its knots; the ring diagram puts one
 * knot on each slot carrying that circle's tangent, which makes its loops exact
 * circles. Neither of those constructions is here.
 */

struct RingPoint {
    math::Vec2 position{};
    /** Unit length, pointing the way a positive turn carries stickers. */
    math::Vec2 tangent{};
};

/**
 * How long a cubic's control arms have to be to draw a circular arc.
 *
 * `turn` is the angle in radians between the directions at the two ends and
 * `chord` the straight distance between them. With no turn at all this is a
 * third of the chord, the usual way of writing a straight line as a cubic; at
 * a right angle it is the 0.5523 r that approximates a quarter circle. So one
 * expression covers a run, a rounded corner and a whole circle, and there is
 * one place that knows how a cubic is bent into an arc.
 */
[[nodiscard]] float arc_arm(float chord, float turn) noexcept;

class SlotRing {
public:
    /**
     * A point of the loop, with the way it runs into and out of it.
     *
     * The two are kept apart because a corner is described by the direction
     * into it and the direction out of it, and because an arc that reaches all
     * the way back to a slot is merged onto that slot by rewriting only the
     * outgoing one.
     */
    struct Knot {
        math::Vec2 point{};
        math::Vec2 in_direction{};
        math::Vec2 out_direction{};
    };

    SlotRing() = default;
    SlotRing(std::vector<Knot> knots, std::vector<std::size_t> slot_knots,
             std::vector<cube::SurfaceSticker> slots);

    [[nodiscard]] bool empty() const noexcept { return slots_.empty(); }
    [[nodiscard]] std::size_t slot_count() const noexcept
    {
        return slots_.size();
    }

    /** Which slot a resting sticker occupies, if this ring carries it. */
    [[nodiscard]] std::optional<std::size_t> slot_of(
        const cube::SurfaceSticker& sticker) const noexcept;

    /** The sticker resting in a slot. */
    [[nodiscard]] cube::SurfaceSticker sticker_at(
        std::size_t slot) const noexcept
    {
        return slots_[slot];
    }

    /**
     * The loop at a slot index, which may be fractional and may run off
     * either end: the ring is closed, so the index wraps.
     */
    [[nodiscard]] RingPoint at(float slot) const noexcept;

    /** The whole loop as one closed path a renderer can stroke. */
    [[nodiscard]] RenderStroke stroke(float width, const Color& color) const;

private:
    std::vector<Knot> knots_;
    /** Segment i runs from knot i to the next, wrapping at the end. */
    std::vector<RenderSegment> segments_;
    /** Knot index of each slot, ascending; knots between them shape a bend. */
    std::vector<std::size_t> slot_knots_;
    std::vector<cube::SurfaceSticker> slots_;
};

}  // namespace rubiks::graphics
