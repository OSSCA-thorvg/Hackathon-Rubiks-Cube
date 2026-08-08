#include "interaction/InteractionController.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace rubiks::interaction {
namespace {

constexpr float kDegreesPerQuarterTurn = 90.0f;

/** Degrees of turn per pixel of drag along the locked direction. */
[[nodiscard]] float degrees_per_pixel(const graphics::Rect& viewport) noexcept
{
    return kDegreesPerQuarterTurn /
           (kQuarterTurnFraction * viewport.width);
}

[[nodiscard]] double snap_duration(float remaining_degrees) noexcept
{
    const float magnitude = std::abs(remaining_degrees);

    // Nothing left to travel: the next frame commits. Anything else takes at
    // least the floor, so a tiny correction is still visible as motion.
    if (magnitude <= 0.0f) return 0.0;

    return std::max(kMinSnapMs, kSnapMsPerQuarterTurn *
                                    static_cast<double>(magnitude) /
                                    kDegreesPerQuarterTurn);
}

[[nodiscard]] double sanitized_delta(double elapsed_ms) noexcept
{
    if (!std::isfinite(elapsed_ms) || elapsed_ms < 0.0) return 0.0;
    return std::min(elapsed_ms, kMaxFrameMs);
}

[[nodiscard]] bool finite_point(float x, float y) noexcept
{
    return std::isfinite(x) && std::isfinite(y);
}

}  // namespace

bool InteractionController::pointer_down(
    float x, float y, const graphics::Camera& camera,
    const graphics::Rect& viewport) noexcept
{
    // A snap owns the cube until it finishes, and a second pointer during a
    // drag is not a second gesture.
    if (snap_ || gesture_) return false;
    if (!finite_point(x, y)) return false;

    const auto ray = pointer_ray(x, y, camera, viewport);
    if (!ray) return false;

    const auto pick = pick_cube(*ray, size_);
    if (!pick) return false;

    gesture_ = Gesture{camera, viewport, *pick, math::Vec2{x, y},
                       std::nullopt, 0.0f};
    return true;
}

void InteractionController::pointer_move(float x, float y) noexcept
{
    if (!gesture_ || !finite_point(x, y)) return;

    const math::Vec2 drag{x - gesture_->start.x, y - gesture_->start.y};

    if (!gesture_->lock) {
        // Below the dead zone the drag has no direction worth trusting, and
        // guessing an axis there would make the cube jump under the finger.
        const float dead_zone = kDeadZoneFraction * gesture_->viewport.width;
        if (math::length(drag) < dead_zone) return;

        gesture_->lock = resolve_axis(gesture_->pick, drag, gesture_->camera,
                                      gesture_->viewport);
        if (!gesture_->lock) return;
    }

    // Always measured from where the finger went down, so the angle is one
    // formula rather than an accumulation that could drift.
    const auto& direction = gesture_->lock->direction;
    gesture_->angle_degrees = (drag.x * direction.x + drag.y * direction.y) *
                              degrees_per_pixel(gesture_->viewport);
}

void InteractionController::pointer_up() noexcept
{
    if (!gesture_) return;

    if (!gesture_->lock) {
        // Released inside the dead zone: a tap, not a turn.
        gesture_.reset();
        return;
    }

    const float angle = gesture_->angle_degrees;
    const float target =
        std::round(angle / kDegreesPerQuarterTurn) * kDegreesPerQuarterTurn;

    snap_ = Snap{gesture_->lock->axis,
                 cube::layer(layer_of(gesture_->pick, gesture_->lock->axis)),
                 angle, target, 0.0, snap_duration(target - angle)};
    gesture_.reset();
}

void InteractionController::cancel() noexcept
{
    gesture_.reset();
}

bool InteractionController::advance(double elapsed_ms) noexcept
{
    if (!snap_) return gesture_.has_value();

    snap_->elapsed_ms += sanitized_delta(elapsed_ms);
    if (snap_->elapsed_ms < snap_->duration_ms) return true;

    const auto quarter_turns = static_cast<int>(
        std::lround(snap_->target_degrees / kDegreesPerQuarterTurn));
    if (quarter_turns != 0) {
        committed_ = cube::CubeMove{snap_->axis, snap_->layers, quarter_turns};
    }
    snap_.reset();

    // The frame that lands on the quarter turn still has to be drawn.
    return true;
}

std::optional<cube::CubeMove>
InteractionController::take_committed_move() noexcept
{
    return std::exchange(committed_, std::nullopt);
}

float InteractionController::snap_angle() const noexcept
{
    if (snap_->duration_ms <= 0.0) return snap_->target_degrees;

    const auto progress = static_cast<float>(
        std::clamp(snap_->elapsed_ms / snap_->duration_ms, 0.0, 1.0));
    const float eased = progress * progress * (3.0f - 2.0f * progress);

    return snap_->start_degrees +
           (snap_->target_degrees - snap_->start_degrees) * eased;
}

std::optional<graphics::ActiveRotation> InteractionController::active_rotation()
    const noexcept
{
    if (snap_) {
        return graphics::ActiveRotation{snap_->axis, snap_->layers,
                                        snap_angle()};
    }

    // Before the axis locks there is no layer to turn, so the cube is still
    // at rest even though a pointer is down.
    if (gesture_ && gesture_->lock) {
        return graphics::ActiveRotation{
            gesture_->lock->axis,
            cube::layer(layer_of(gesture_->pick, gesture_->lock->axis)),
            gesture_->angle_degrees};
    }

    return std::nullopt;
}

void InteractionController::reset() noexcept
{
    gesture_.reset();
    snap_.reset();
    committed_.reset();
}

}  // namespace rubiks::interaction
