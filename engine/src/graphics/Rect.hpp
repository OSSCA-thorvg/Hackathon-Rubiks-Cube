#pragma once

namespace rubiks::graphics {

/** An axis-aligned region of the drawing buffer, in pixels from the top left. */
struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

[[nodiscard]] constexpr bool operator==(const Rect& a, const Rect& b) noexcept
{
    return a.x == b.x && a.y == b.y && a.width == b.width &&
           a.height == b.height;
}

}  // namespace rubiks::graphics
