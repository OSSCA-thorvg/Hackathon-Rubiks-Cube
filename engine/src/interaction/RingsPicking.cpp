#include "interaction/RingsPicking.hpp"

#include <cmath>

#include "graphics/RingsGeometry.hpp"

namespace rubiks::interaction {

std::optional<RingsPick> pick_rings(float x, float y,
                                    const graphics::Rect& rect, int size)
{
    if (!std::isfinite(x) || !std::isfinite(y)) return std::nullopt;
    if (rect.width <= 0.0f || rect.height <= 0.0f || size < 2) {
        return std::nullopt;
    }

    const float reach =
        graphics::kRingsPickReach * graphics::rings_slot_spacing(rect, size);

    std::optional<RingsPick> nearest;
    float closest = reach;

    for (const auto& sticker : cube::surface_stickers(size)) {
        const auto at = graphics::rings_slot_position(sticker, rect, size);
        if (!at) continue;

        const float away =
            std::sqrt((at->x - x) * (at->x - x) + (at->y - y) * (at->y - y));
        if (away >= closest) continue;

        closest = away;
        nearest = RingsPick{sticker};
    }
    return nearest;
}

}  // namespace rubiks::interaction
