#pragma once

#include "math/Types.hpp"

namespace rubiks::math {

/**
 * Rotation quaternion stored as (x, y, z, w).
 *
 * Layer rotations in later phases interpolate through quaternions, so the
 * conversion to a matrix lives here rather than in the transform code.
 */
using Quaternion = Vec4;

/** Rotation of zero radians. */
[[nodiscard]] inline Quaternion quaternion_identity() noexcept
{
    return Quaternion{0.0f, 0.0f, 0.0f, 1.0f};
}

/** Right-handed rotation of `radians` about `axis`, which must be unit length. */
[[nodiscard]] inline Quaternion quaternion_from_axis_angle(
    const Vec3& axis, float radians) noexcept
{
    return linalg::rotation_quat(axis, radians);
}

/** Composition: the result applies `b` first, then `a`. */
[[nodiscard]] inline Quaternion quaternion_multiply(const Quaternion& a,
                                                    const Quaternion& b) noexcept
{
    return linalg::qmul(a, b);
}

/** Unit-length quaternion; undefined for a zero quaternion. */
[[nodiscard]] inline Quaternion quaternion_normalize(const Quaternion& q) noexcept
{
    return linalg::normalize(q);
}

/** Rotates a vector directly, without building a matrix. */
[[nodiscard]] inline Vec3 quaternion_rotate(const Quaternion& q,
                                            const Vec3& v) noexcept
{
    return linalg::qrot(q, v);
}

/** Equivalent rotation matrix. */
[[nodiscard]] inline Mat4 quaternion_to_matrix(const Quaternion& q) noexcept
{
    return linalg::rotation_matrix(q);
}

}  // namespace rubiks::math
