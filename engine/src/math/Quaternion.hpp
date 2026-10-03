#pragma once

#include "math/Types.hpp"

namespace rubiks::math {

/**
 * Rotation quaternion stored as (x, y, z, w).
 *
 * A turning layer is the one thing rotated this way: the quaternion is made
 * from the layer's axis and an angle and turns each point directly. Nothing
 * composes two rotations or needs one as a matrix, so neither is here.
 */
using Quaternion = Vec4;

/** Right-handed rotation of `radians` about `axis`, which must be unit length. */
[[nodiscard]] inline Quaternion quaternion_from_axis_angle(
    const Vec3& axis, float radians) noexcept
{
    return linalg::rotation_quat(axis, radians);
}

/** Rotates a vector directly, without building a matrix. */
[[nodiscard]] inline Vec3 quaternion_rotate(const Quaternion& q,
                                            const Vec3& v) noexcept
{
    return linalg::qrot(q, v);
}

}  // namespace rubiks::math
