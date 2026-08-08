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
};

}  // namespace rubiks::graphics
