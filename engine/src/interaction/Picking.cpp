#include "interaction/Picking.hpp"

#include <algorithm>
#include <cmath>

#include "cube/Surface.hpp"
#include "graphics/3d/CubeGeometry.hpp"

namespace rubiks::interaction {
namespace {

using cube::Face;
using math::Vec3;

[[nodiscard]] float component(const Vec3& v, cube::Axis axis) noexcept
{
    switch (axis) {
        case cube::Axis::X:
            return v.x;
        case cube::Axis::Y:
            return v.y;
        case cube::Axis::Z:
            break;
    }
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
    const math::Vec2 ndc = graphics::pixels_to_ndc(viewport, math::Vec2{x, y});
    if (ndc.x < -1.0f || ndc.x > 1.0f) return std::nullopt;
    if (ndc.y < -1.0f || ndc.y > 1.0f) return std::nullopt;

    const math::Mat4 to_world =
        math::inverse(math::multiply(camera.projection(), camera.view()));

    // z = -1 is the near plane in the OpenGL-style convention math::perspective
    // targets, so this is the point the pointer sits over on that plane.
    const math::Vec4 clip{ndc.x, ndc.y, -1.0f, 1.0f};
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

    // Each face is the outer plane at one end of its axis.
    for (const Face face : cube::faces()) {
        const cube::Axis normal = cube::axis_of(face);
        const float sign = cube::is_positive_face(face) ? 1.0f : -1.0f;
        const float along = component(ray.direction, normal);

        // Only a plane whose outward normal opposes the ray can be the
        // surface the pointer sees; the rest are the far side or edge on.
        if (sign * along >= -kRayEpsilon) continue;

        const float distance =
            (sign * half - component(ray.origin, normal)) / along;
        if (distance <= kRayEpsilon) continue;

        const Vec3 point = ray.origin + ray.direction * distance;

        bool within_face = true;
        for (const cube::Axis axis : cube::other_axes(normal)) {
            if (std::abs(component(point, axis)) > half) {
                within_face = false;
                break;
            }
        }
        if (!within_face) continue;

        if (nearest && distance >= nearest_distance) continue;

        nearest_distance = distance;
        nearest = Pick{face, cell_index(point.x, size),
                       cell_index(point.y, size), cell_index(point.z, size),
                       point};
    }

    return nearest;
}

}  // namespace rubiks::interaction
