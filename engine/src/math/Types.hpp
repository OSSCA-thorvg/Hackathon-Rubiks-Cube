#pragma once

#include <linalg.h>

/**
 * Vector and matrix types for the engine, aliased onto vendored linalg.h.
 *
 * This header and its siblings in math/ are the only places that include
 * linalg.h. Everything else goes through these aliases, so the conventions
 * below have a single home:
 *
 * - Right-handed world and view space, +Y up, camera looks down -Z.
 * - Mat4 is column-major with column vectors: apply as M * v, compose as
 *   P * V * M.
 * - Scalars are float, matching the ThorVG coordinate API.
 *
 * linalg spells the algebraic product mul() and reserves operator* for the
 * elementwise product, which is an easy mistake to make silently. Engine code
 * uses multiply() and apply() below instead of calling either directly.
 */
namespace rubiks::math {

using Vec2 = linalg::vec<float, 2>;
using Vec3 = linalg::vec<float, 3>;
using Vec4 = linalg::vec<float, 4>;
using Mat4 = linalg::mat<float, 4, 4>;

using linalg::cross;
using linalg::dot;
using linalg::length;

/** Identity matrix. */
[[nodiscard]] inline Mat4 identity() noexcept
{
    return Mat4{linalg::identity};
}

/**
 * Unit-length direction of a vector.
 *
 * Undefined for zero-length input, which yields NaN components rather than a
 * sentinel; callers pass directions that are known to be non-degenerate.
 */
[[nodiscard]] inline Vec3 normalize(const Vec3& v) noexcept
{
    return linalg::normalize(v);
}

/** Matrix product: multiply(a, b) applies b first, then a. */
[[nodiscard]] inline Mat4 multiply(const Mat4& a, const Mat4& b) noexcept
{
    return linalg::mul(a, b);
}

/**
 * Matrix inverse, used to turn a screen point back into a world-space ray.
 *
 * Undefined for a singular matrix; callers invert a projection times a view,
 * which is invertible for any camera the engine can build.
 */
[[nodiscard]] inline Mat4 inverse(const Mat4& m) noexcept
{
    return linalg::inverse(m);
}

/** Applies a matrix to a homogeneous vector. */
[[nodiscard]] inline Vec4 apply(const Mat4& m, const Vec4& v) noexcept
{
    return linalg::mul(m, v);
}

/** Applies a matrix to a point, treating it as w = 1 without dividing. */
[[nodiscard]] inline Vec4 apply_point(const Mat4& m, const Vec3& p) noexcept
{
    return linalg::mul(m, Vec4{p, 1.0f});
}

}  // namespace rubiks::math
