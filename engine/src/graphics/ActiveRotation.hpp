#pragma once

#include "cube/CubeMove.hpp"

namespace rubiks::graphics {

/**
 * A layer turn in progress, at an angle the domain cannot represent.
 *
 * CubeState only ever holds quarter turns, so the continuous angle of a drag
 * lives here instead. Interaction produces it and the application hands it to
 * the scene builder, which keeps the dependency one-way: graphics never needs
 * to know that an interaction module exists.
 *
 * The absence of a turn is `std::nullopt` at the use site rather than a zero
 * angle, so `axis` and `layers` are never read in a state where they have no
 * meaning.
 */
struct ActiveRotation {
    cube::Axis axis;
    cube::LayerMask layers;
    /** Positive is clockwise seen from the positive end of `axis`, as in
     *  CubeMove::quarter_turns. */
    float angle_degrees;
    /**
     * How far a moving piece is held off the drawing: 0 settled, 1 lifted.
     *
     * How much larger it is drawn and how far its shadow falls follow this
     * rather than the angle. Following the angle made them come and go inside
     * a single drag -- lifted by the middle of the quarter and set down again
     * by the end of it, so the drawing rolled -- when what a hand does is pick
     * it up once, hold it up for as long as the turn is being held, and put it
     * down as the turn settles.
     *
     * Interaction knows which of those is happening and the drawing does not,
     * so it is decided there and carried here. Zero by default: a turn nobody
     * is holding is a turn drawn settled.
     */
    float opening = 0.0f;
};

[[nodiscard]] constexpr bool operator==(const ActiveRotation& a,
                                        const ActiveRotation& b) noexcept
{
    return a.axis == b.axis && a.layers == b.layers &&
           a.angle_degrees == b.angle_degrees && a.opening == b.opening;
}

}  // namespace rubiks::graphics
