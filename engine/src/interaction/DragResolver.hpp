#pragma once

#include <optional>

#include "cube/CubeMove.hpp"
#include "graphics/Camera.hpp"
#include "graphics/Rect.hpp"
#include "interaction/Picking.hpp"
#include "math/Types.hpp"

/**
 * Deciding which layer a drag turns, and by how much.
 *
 * Grabbing a sticker leaves two possible axes: the two that are not the axis
 * the grabbed face points along. Each is scored by where it would carry the
 * grabbed point on screen, so the choice follows the picture the user sees
 * rather than a table of screen directions per face.
 */
namespace rubiks::interaction {

/** A candidate axis with the screen direction a positive turn moves along. */
struct AxisCandidate {
    cube::Axis axis;
    /** Unit screen-space direction; dragging this way turns positively. */
    math::Vec2 direction;
};

/** Small turn used to sample where a rotation carries a point. */
inline constexpr float kProbeDegrees = 1.0f;

/**
 * Projects a world-space point into viewport pixels.
 *
 * Empty for points at or behind the near plane, where the perspective divide
 * has no meaning.
 */
[[nodiscard]] std::optional<math::Vec2> project_to_screen(
    const math::Vec3& point, const graphics::Camera& camera,
    const graphics::Rect& viewport) noexcept;

/**
 * The screen direction along which a positive turn about `axis` carries
 * `point`.
 *
 * Empty when the point lies on the axis, or when the turned point cannot be
 * projected.
 */
[[nodiscard]] std::optional<math::Vec2> turn_direction(
    cube::Axis axis, const math::Vec3& point, const graphics::Camera& camera,
    const graphics::Rect& viewport) noexcept;

/**
 * Chooses the axis a drag is asking for.
 *
 * The candidate whose screen direction is most parallel to the drag wins,
 * either sign counting equally: dragging backwards along an axis turns it
 * backwards rather than selecting the other one. Ties go to the lower Axis
 * value, which matters only for determinism.
 */
[[nodiscard]] std::optional<AxisCandidate> resolve_axis(
    const Pick& pick, const math::Vec2& drag, const graphics::Camera& camera,
    const graphics::Rect& viewport) noexcept;

}  // namespace rubiks::interaction
