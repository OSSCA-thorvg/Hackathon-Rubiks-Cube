#pragma once

#include <optional>

#include "cube/CubeMove.hpp"
#include "cube/Cubie.hpp"
#include "graphics/Rect.hpp"

/**
 * Turning a pointer position on the unfolded net into a layer turn.
 *
 * Simpler than the 3D side and deliberately unrelated to it: the net is
 * already screen-space, so this is point-in-rectangle arithmetic rather than a
 * ray cast, and its screen directions are fixed, so there is nothing to
 * project and no candidate axis to score.
 */
namespace rubiks::interaction {

/** The net cell a pointer landed on. */
struct NetPick {
    cube::Face face;
    int col;
    int row;
};

/**
 * Finds the net cell under a point in the drawing buffer.
 *
 * The empty corners of the cross are misses, as is anything outside `rect`:
 * a press there is on the background, not on a face.
 */
[[nodiscard]] std::optional<NetPick> pick_net(float x, float y,
                                              const graphics::Rect& rect,
                                              int size) noexcept;

}  // namespace rubiks::interaction
