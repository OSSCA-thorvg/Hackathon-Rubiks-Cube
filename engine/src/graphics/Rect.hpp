#pragma once

namespace rubiks::graphics {

/** An axis-aligned region of the drawing buffer, in pixels from the top left. */
struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

}  // namespace rubiks::graphics
