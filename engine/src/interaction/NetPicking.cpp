#include "interaction/NetPicking.hpp"

#include <algorithm>
#include <cmath>

#include "graphics/Layout.hpp"
#include "graphics/NetGeometry.hpp"

namespace rubiks::interaction {

std::optional<NetPick> pick_net(float x, float y, const graphics::Rect& rect,
                                int size) noexcept
{
    if (!std::isfinite(x) || !std::isfinite(y)) return std::nullopt;
    if (size <= 0 || rect.width <= 0.0f || rect.height <= 0.0f) {
        return std::nullopt;
    }

    if (x < rect.x || x >= rect.x + rect.width) return std::nullopt;
    if (y < rect.y || y >= rect.y + rect.height) return std::nullopt;

    const float face_side =
        rect.width / static_cast<float>(graphics::kNetColumns);
    const auto block_column = static_cast<int>((x - rect.x) / face_side);
    const auto block_row = static_cast<int>((y - rect.y) / face_side);

    for (const auto face : graphics::net_faces()) {
        const auto block = graphics::net_block(face);
        if (block.column != block_column || block.row != block_row) continue;

        const float cell = face_side / static_cast<float>(size);
        const float local_x =
            x - rect.x - static_cast<float>(block_column) * face_side;
        const float local_y =
            y - rect.y - static_cast<float>(block_row) * face_side;

        // Clamped rather than trusted: the divisions above are exact enough
        // for a face but a point on the very last edge must not index past it.
        const int col =
            std::clamp(static_cast<int>(local_x / cell), 0, size - 1);
        const int row =
            std::clamp(static_cast<int>(local_y / cell), 0, size - 1);
        return NetPick{face, col, row};
    }

    // Inside the rectangle but on one of the four corners the cross leaves
    // empty, which is background like anywhere else off a face.
    return std::nullopt;
}

}  // namespace rubiks::interaction
