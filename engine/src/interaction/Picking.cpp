#include "interaction/Picking.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "graphics/CubeGeometry.hpp"

namespace rubiks::interaction {
namespace {

using cube::Axis;
using cube::Face;
using math::Vec3;

/** One of the cube's six outer planes. */
struct OuterPlane {
    int axis;     // 0 = x, 1 = y, 2 = z
    float sign;   // which side of the origin the plane sits on
    Face face;
};

constexpr std::array<OuterPlane, 6> kOuterPlanes{{
    {0, +1.0f, Face::Right},
    {0, -1.0f, Face::Left},
    {1, +1.0f, Face::Up},
    {1, -1.0f, Face::Down},
    {2, +1.0f, Face::Front},
    {2, -1.0f, Face::Back},
}};

[[nodiscard]] float component(const Vec3& v, int axis) noexcept
{
    if (axis == 0) return v.x;
    if (axis == 1) return v.y;
    return v.z;
}

/** The cell a coordinate falls in, half-open towards the larger index. */
[[nodiscard]] int cell_index(float coordinate, int size) noexcept
{
    const float pitch = 2.0f * graphics::kCubeHalfExtent /
                        static_cast<float>(size);
    const auto index = static_cast<int>(
        std::floor((coordinate + graphics::kCubeHalfExtent) / pitch));

    // The far face lands exactly on the upper bound, which the formula would
    // put one cell past the end.
    return std::clamp(index, 0, size - 1);
}

}  // namespace

std::optional<Ray> pointer_ray(float x, float y, const graphics::Camera& camera,
                               const graphics::Rect& viewport) noexcept
{
    if (!std::isfinite(x) || !std::isfinite(y)) return std::nullopt;
    if (viewport.width <= 0.0f || viewport.height <= 0.0f) return std::nullopt;

    // The inverse of ViewportPass.
    const float ndc_x = (x - viewport.x) / viewport.width * 2.0f - 1.0f;
    const float ndc_y = 1.0f - (y - viewport.y) / viewport.height * 2.0f;
    if (ndc_x < -1.0f || ndc_x > 1.0f) return std::nullopt;
    if (ndc_y < -1.0f || ndc_y > 1.0f) return std::nullopt;

    const math::Mat4 to_world =
        math::inverse(math::multiply(camera.projection(), camera.view()));

    // z = -1 is the near plane in the OpenGL-style convention math::perspective
    // targets, so this is the point the pointer sits over on that plane.
    const math::Vec4 clip{ndc_x, ndc_y, -1.0f, 1.0f};
    const math::Vec4 unprojected = math::apply(to_world, clip);
    if (unprojected.w == 0.0f) return std::nullopt;

    const Vec3 on_near_plane{unprojected.x / unprojected.w,
                             unprojected.y / unprojected.w,
                             unprojected.z / unprojected.w};

    const Vec3 direction = on_near_plane - camera.eye();
    if (math::length(direction) <= kRayEpsilon) return std::nullopt;

    return Ray{camera.eye(), math::normalize(direction)};
}

std::optional<Pick> pick_cube(const Ray& ray, int size) noexcept
{
    if (size <= 0) return std::nullopt;

    constexpr float half = graphics::kCubeHalfExtent;

    std::optional<Pick> nearest;
    float nearest_distance = 0.0f;

    for (const auto& plane : kOuterPlanes) {
        const float along = component(ray.direction, plane.axis);

        // Only a plane whose outward normal opposes the ray can be the
        // surface the pointer sees; the rest are the far side or edge on.
        if (plane.sign * along >= -kRayEpsilon) continue;

        const float distance =
            (plane.sign * half - component(ray.origin, plane.axis)) / along;
        if (distance <= kRayEpsilon) continue;

        const Vec3 point = ray.origin + ray.direction * distance;

        bool within_face = true;
        for (int axis = 0; axis < 3; ++axis) {
            if (axis == plane.axis) continue;
            if (std::abs(component(point, axis)) > half) {
                within_face = false;
                break;
            }
        }
        if (!within_face) continue;

        if (nearest && distance >= nearest_distance) continue;

        nearest_distance = distance;
        nearest = Pick{plane.face, cell_index(point.x, size),
                       cell_index(point.y, size), cell_index(point.z, size),
                       point};
    }

    return nearest;
}

int layer_of(const Pick& pick, Axis axis) noexcept
{
    switch (axis) {
        case Axis::X:
            return pick.x;
        case Axis::Y:
            return pick.y;
        case Axis::Z:
            break;
    }
    return pick.z;
}

Axis axis_of(Face face) noexcept
{
    switch (face) {
        case Face::Right:
        case Face::Left:
            return Axis::X;
        case Face::Up:
        case Face::Down:
            return Axis::Y;
        case Face::Front:
        case Face::Back:
            break;
    }
    return Axis::Z;
}

}  // namespace rubiks::interaction
