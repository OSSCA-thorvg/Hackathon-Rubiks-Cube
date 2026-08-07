#pragma once

#include "graphics/Color.hpp"
#include "graphics/Scene.hpp"
#include "math/Types.hpp"

namespace rubiks::graphics {

/**
 * The six face colors of one cube, each independently assignable.
 *
 * Phase 4 gives every 1x1x1 cubie its own instance of this so a single cubie
 * can carry stickers of different colors; Phase 3 uses one cube with the
 * standard colors.
 */
struct CubeFaceColors {
    Color right;  // +X
    Color left;   // -X
    Color up;     // +Y
    Color down;   // -Y
    Color front;  // +Z
    Color back;   // -Z
};

/** Standard Rubik's Cube colors used by the rendered scene contract. */
[[nodiscard]] CubeFaceColors standard_cube_colors() noexcept;

/**
 * Appends the six faces of an axis-aligned cube to `scene`.
 *
 * Every face is wound counter-clockwise as seen from outside the cube, which
 * is what back-face culling relies on.
 */
void append_cube(WorldScene& scene, const math::Vec3& center, float half_extent,
                 const CubeFaceColors& colors);

/**
 * Builds the Phase 3 scene: one cube of edge length 2 centered at the origin.
 *
 * This is the seam Phase 4 replaces with a CubeState-driven 3x3x3 layout.
 */
[[nodiscard]] WorldScene build_scene();

}  // namespace rubiks::graphics
