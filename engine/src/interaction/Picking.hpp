#pragma once

#include <optional>

#include "cube/CubeMove.hpp"
#include "cube/Cubie.hpp"
#include "graphics/Camera.hpp"
#include "graphics/Rect.hpp"
#include "math/Types.hpp"

/**
 * Turning a pointer position into the cubie face under it.
 *
 * This is a ray cast rather than a renderer hit test. ThorVG lives behind the
 * rendering boundary, and picking through it would pull that dependency into
 * interaction and make every test here need a renderer.
 */
namespace rubiks::interaction {

/** A world-space ray. `direction` is unit length. */
struct Ray {
    math::Vec3 origin;
    math::Vec3 direction;
};

/** The sticker a ray hit, in the domain's index coordinates. */
struct Pick {
    cube::Face face;
    int x;
    int y;
    int z;
    /** Where the ray met the cube, in world space. */
    math::Vec3 point;
};

/**
 * Distance below which an intersection counts as being at the ray origin.
 *
 * Also rejects planes the ray runs along, where the intersection distance
 * would otherwise be a division by a value near zero.
 */
inline constexpr float kRayEpsilon = 1e-4f;

/**
 * Builds the world-space ray through a point in the 3D viewport.
 *
 * The coordinates are drawing-buffer pixels. Points outside the viewport and
 * non-finite coordinates produce no ray, so an out-of-region press is a miss
 * rather than an extrapolated hit.
 */
[[nodiscard]] std::optional<Ray> pointer_ray(
    float x, float y, const graphics::Camera& camera,
    const graphics::Rect& viewport) noexcept;

/**
 * Intersects a ray with the resting cube of `size` cubies per edge.
 *
 * Only the outward-facing plane nearest the origin can be hit, so the result
 * is the sticker a viewer would point at. Cell boundaries are half-open
 * towards the larger index, and the far edge of the cube belongs to the last
 * cell rather than to a cell that does not exist.
 */
[[nodiscard]] std::optional<Pick> pick_cube(const Ray& ray, int size) noexcept;

/** The cubie's coordinate along `axis`, which is also its layer index. */
[[nodiscard]] int layer_of(const Pick& pick, cube::Axis axis) noexcept;

/** The axis a face points along. */
[[nodiscard]] cube::Axis axis_of(cube::Face face) noexcept;

}  // namespace rubiks::interaction
