#pragma once

#include <array>
#include <vector>

#include "graphics/Color.hpp"
#include "math/Types.hpp"

namespace rubiks::graphics {

/** Screen-space quad ready to be filled by a renderer. */
struct RenderFace {
    std::array<math::Vec2, 4> points{};
    Color color{};
};

/**
 * Final output of the graphics pipeline and the only input a renderer takes.
 *
 * Deliberately free of ThorVG types and of depth information: faces are
 * already ordered back to front, so a renderer draws them in order and needs
 * to know nothing about the 3D scene they came from.
 */
struct RenderScene {
    std::vector<RenderFace> faces;
};

}  // namespace rubiks::graphics
