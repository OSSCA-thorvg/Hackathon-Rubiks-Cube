#include "graphics/ConvexHull.hpp"

#include <algorithm>
#include <cstddef>

namespace rubiks::graphics {
namespace {

float cross(const math::Vec2& o, const math::Vec2& a, const math::Vec2& b) noexcept
{
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

}  // namespace

std::vector<math::Vec2> convex_hull(std::vector<math::Vec2> points)
{
    // Andrew's monotone chain. Sorting first and removing duplicates is what
    // lets the two chains below meet without a special case for repeats.
    std::sort(points.begin(), points.end(),
              [](const math::Vec2& a, const math::Vec2& b) {
                  return a.x < b.x || (a.x == b.x && a.y < b.y);
              });
    points.erase(std::unique(points.begin(), points.end(),
                             [](const math::Vec2& a, const math::Vec2& b) {
                                 return a.x == b.x && a.y == b.y;
                             }),
                 points.end());
    if (points.size() < 3) return points;

    std::vector<math::Vec2> hull(2 * points.size());
    std::size_t k = 0;

    for (const auto& point : points) {
        while (k >= 2 && cross(hull[k - 2], hull[k - 1], point) <= 0.0f) --k;
        hull[k++] = point;
    }
    for (std::size_t i = points.size() - 1, t = k + 1; i-- > 0; ) {
        while (k >= t && cross(hull[k - 2], hull[k - 1], points[i]) <= 0.0f) --k;
        hull[k++] = points[i];
    }

    hull.resize(k - 1);
    return hull;
}

}  // namespace rubiks::graphics
