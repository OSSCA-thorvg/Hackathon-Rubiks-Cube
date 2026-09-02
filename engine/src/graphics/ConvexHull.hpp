#pragma once

#include <vector>

#include "math/Types.hpp"

namespace rubiks::graphics {

/**
 * The convex hull of a set of points in the plane, counter-clockwise in a
 * +Y-up frame, with collinear points on the hull's edges left out.
 *
 * Used twice, for the same reason both times: the shadow of a convex box is
 * the hull of its corners' shadows, and the silhouette of a convex box is the
 * hull of its corners on screen. Fewer than three distinct points give back
 * whatever there is, which is degenerate and drawn as nothing.
 */
[[nodiscard]] std::vector<math::Vec2> convex_hull(std::vector<math::Vec2> points);

}  // namespace rubiks::graphics
