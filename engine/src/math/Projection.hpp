#pragma once

#include <cmath>

#include "math/Types.hpp"

/**
 * View and projection matrices.
 *
 * linalg ships comparable helpers, but these are written out so the
 * conventions pinned in docs/tasks/03-math-and-graphics-foundation.md are
 * defined by this engine and asserted against hand-computed values.
 */
namespace rubiks::math {

/**
 * World-to-view matrix for a camera at `eye` looking at `target`.
 *
 * View space is right-handed with the camera at the origin looking down -Z,
 * so visible geometry has a negative z. `up` must not be parallel to the
 * view direction.
 */
[[nodiscard]] inline Mat4 look_at(const Vec3& eye, const Vec3& target,
                                  const Vec3& up) noexcept
{
    const Vec3 forward = normalize(target - eye);
    const Vec3 right = normalize(cross(forward, up));
    const Vec3 true_up = cross(right, forward);

    return Mat4{
        Vec4{right.x, true_up.x, -forward.x, 0.0f},
        Vec4{right.y, true_up.y, -forward.y, 0.0f},
        Vec4{right.z, true_up.z, -forward.z, 0.0f},
        Vec4{-dot(right, eye), -dot(true_up, eye), dot(forward, eye), 1.0f},
    };
}

/**
 * View-to-clip matrix mapping the frustum onto the OpenGL-style NDC cube,
 * with x and y in [-1, 1] and +Y up.
 *
 * The clip w equals the positive view-space depth, so the perspective divide
 * is only defined for geometry in front of the near plane.
 *
 * @param vertical_fov Vertical field of view in radians.
 */
[[nodiscard]] inline Mat4 perspective(float vertical_fov, float aspect,
                                      float near_plane,
                                      float far_plane) noexcept
{
    const float focal = 1.0f / std::tan(vertical_fov * 0.5f);
    const float depth = near_plane - far_plane;

    return Mat4{
        Vec4{focal / aspect, 0.0f, 0.0f, 0.0f},
        Vec4{0.0f, focal, 0.0f, 0.0f},
        Vec4{0.0f, 0.0f, (far_plane + near_plane) / depth, -1.0f},
        Vec4{0.0f, 0.0f, 2.0f * far_plane * near_plane / depth, 0.0f},
    };
}

}  // namespace rubiks::math
