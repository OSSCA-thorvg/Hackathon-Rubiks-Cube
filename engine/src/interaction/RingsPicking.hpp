#pragma once

#include <optional>

#include "cube/Surface.hpp"
#include "graphics/Rect.hpp"

/**
 * Turning a pointer position on the ring diagram into the sticker under it.
 *
 * Simpler again than the net's, and for the same reason: the diagram is already
 * screen-space. What it cannot do is index a grid, because the fifty-four slots
 * sit where circles cross rather than on rows and columns, so the answer is the
 * nearest of them and a distance to decide whether that counts as a hit.
 */
namespace rubiks::interaction {

/** The sticker a pointer landed on in the ring diagram. */
struct RingsPick {
    cube::SurfaceSticker sticker;
};

/**
 * Finds the sticker under a point in the drawing buffer.
 *
 * A miss anywhere the press is not close enough to a slot, which is most of the
 * diagram: the loops themselves are not grabbable, only the stickers strung on
 * them.
 */
[[nodiscard]] std::optional<RingsPick> pick_rings(float x, float y,
                                                  const graphics::Rect& rect,
                                                  int size);

}  // namespace rubiks::interaction
