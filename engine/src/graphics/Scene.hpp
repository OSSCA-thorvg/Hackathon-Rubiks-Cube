#pragma once

#include <array>
#include <vector>

#include "graphics/Color.hpp"
#include "math/Types.hpp"

/**
 * Intermediate representations flowing between pipeline passes.
 *
 * Each stage has its own type so a pass cannot be applied to the wrong space
 * by accident; the compiler rejects `view(camera)` applied to view-space data.
 */
namespace rubiks::graphics {

/** Quad in world space. */
struct WorldFace {
    std::array<math::Vec3, 4> points{};
    Color color{};
};

struct WorldScene {
    std::vector<WorldFace> faces;
};

/** Quad in view space, where the camera sits at the origin looking down -Z. */
struct ViewFace {
    std::array<math::Vec3, 4> points{};
    Color color{};
};

struct ViewScene {
    std::vector<ViewFace> faces;
};

/**
 * Quad after the perspective divide.
 *
 * Culling reads `ndc`, which still has +Y up so a front face has a positive
 * signed area. Depth sorting reads `depth`, the mean view-space z of the four
 * corners; it is negative in front of the camera and decreases with distance.
 */
struct ClipFace {
    std::array<math::Vec2, 4> ndc{};
    float depth = 0.0f;
    Color color{};
};

struct ClipScene {
    std::vector<ClipFace> faces;
};

}  // namespace rubiks::graphics
