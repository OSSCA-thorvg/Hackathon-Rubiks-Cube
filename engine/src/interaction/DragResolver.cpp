#include "interaction/DragResolver.hpp"

#include <array>
#include <cmath>

#include "graphics/CubeGeometry.hpp"
#include "math/Quaternion.hpp"

namespace rubiks::interaction {
namespace {

using cube::Axis;
using math::Vec2;
using math::Vec3;

constexpr std::array<Axis, 3> kAxes{Axis::X, Axis::Y, Axis::Z};

/** Screen distance below which a direction is noise rather than a direction. */
constexpr float kMinScreenDelta = 1e-4f;

}  // namespace

std::optional<Vec2> project_to_screen(const Vec3& point,
                                      const graphics::Camera& camera,
                                      const graphics::Rect& viewport) noexcept
{
    const math::Vec4 in_view = math::apply_point(camera.view(), point);
    if (in_view.z >= -camera.near_plane()) return std::nullopt;

    const math::Vec4 clip = math::apply(camera.projection(), in_view);
    if (clip.w <= 0.0f) return std::nullopt;

    const Vec2 ndc{clip.x / clip.w, clip.y / clip.w};
    return Vec2{viewport.x + (ndc.x + 1.0f) * 0.5f * viewport.width,
                viewport.y + (1.0f - ndc.y) * 0.5f * viewport.height};
}

std::optional<Vec2> turn_direction(Axis axis, const Vec3& point,
                                   const graphics::Camera& camera,
                                   const graphics::Rect& viewport) noexcept
{
    // Sampling the real rotation instead of deriving a direction analytically
    // means the answer cannot disagree with what the renderer will draw.
    const Vec3 turned = math::quaternion_rotate(
        graphics::layer_rotation(axis, kProbeDegrees), point);

    const auto from = project_to_screen(point, camera, viewport);
    const auto to = project_to_screen(turned, camera, viewport);
    if (!from || !to) return std::nullopt;

    const Vec2 delta{to->x - from->x, to->y - from->y};
    const float distance = math::length(delta);
    if (distance <= kMinScreenDelta) return std::nullopt;

    return delta / distance;
}

std::optional<AxisCandidate> resolve_axis(
    const Pick& pick, const Vec2& drag, const graphics::Camera& camera,
    const graphics::Rect& viewport) noexcept
{
    const Axis face_axis = axis_of(pick.face);

    std::optional<AxisCandidate> best;
    float best_score = 0.0f;

    for (const Axis axis : kAxes) {
        // Turning about the axis the grabbed face points along would spin the
        // sticker in place, which no drag can ask for.
        if (axis == face_axis) continue;

        const auto direction = turn_direction(axis, pick.point, camera,
                                              viewport);
        if (!direction) continue;

        const float score =
            std::abs(drag.x * direction->x + drag.y * direction->y);

        // Strictly greater, so an exact tie keeps the earlier axis.
        if (best && score <= best_score) continue;

        best_score = score;
        best = AxisCandidate{axis, *direction};
    }

    return best;
}

}  // namespace rubiks::interaction
