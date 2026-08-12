#include "interaction/InteractionController.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
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

/** Degrees of viewpoint sweep per pixel of drag. */
[[nodiscard]] float orbit_degrees_per_pixel(
    const graphics::Rect& viewport) noexcept
{
    return kDegreesPerQuarterTurn /
           (kOrbitQuarterTurnFraction * viewport.width);
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
    // gesture is not a second gesture.
    if (snap_ || gesture_ || orbit_) return false;
    if (!finite_point(x, y)) return false;
    if (viewport.width <= 0.0f || viewport.height <= 0.0f) return false;

    std::optional<Pick> pick;
    if (const auto ray = pointer_ray(x, y, camera, viewport)) {
        pick = pick_cube(*ray, size_);
    }

    // Missing the cube is not nothing: it is a request to look around it.
    // Where the press landed does not matter, only that it was not the cube.
    if (!pick) {
        orbit_ = Orbit{viewport, math::Vec2{x, y}};
        return true;
    }

    gesture_ = Gesture{camera, viewport, *pick, math::Vec2{x, y},
                       std::nullopt, 0.0f};
    return true;
}

bool InteractionController::start_move(const cube::CubeMove& move) noexcept
{
    if (is_busy()) return false;
    if (move.layers == 0 || move.layers >= cube::layer(size_)) return false;

    // Only the magnitude wraps at a full turn; the direction stays the
    // caller's, so a half turn asked for as -2 still animates the way the
    // face it was named after turns.
    const int magnitude = std::abs(move.quarter_turns) % 4;
    if (magnitude == 0) return false;

    const int quarter_turns =
        move.quarter_turns < 0 ? -magnitude : magnitude;
    const float target =
        static_cast<float>(quarter_turns) * kDegreesPerQuarterTurn;

    // The turn starts at rest, so the whole target is still to travel.
    snap_ = Snap{move.axis, move.layers, 0.0f, target, 0.0,
                 snap_duration(target)};
    return true;
}

void InteractionController::pointer_move(float x, float y) noexcept
{
    if (orbit_) {
        if (!finite_point(x, y)) return;

        // A step since the last position, not a total from the press. The
        // viewpoint integrates these against a clamped store, so a pitch
        // resting on its limit moves away on the first opposite step instead
        // of waiting for a total displacement to come back into range.
        const math::Vec2 step{x - orbit_->previous.x, y - orbit_->previous.y};
        orbit_->previous = math::Vec2{x, y};

        const float degrees = orbit_degrees_per_pixel(orbit_->viewport);
        accumulate_orbit(-step.x * degrees, step.y * degrees);
        return;
    }

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
    // Nothing to settle or commit, and the accumulated sweep stays behind to
    // be taken: a viewpoint is where the user left it, not something to undo.
    if (orbit_) {
        orbit_.reset();
        return;
    }

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

    // Same as a release for an orbit: there is no commit to withhold, and the
    // sweep already made stays pending rather than being thrown away.
    orbit_.reset();
}

bool InteractionController::advance(double elapsed_ms) noexcept
{
    if (!snap_) return gesture_.has_value() || orbit_.has_value();

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

std::optional<OrbitDelta> InteractionController::take_orbit_delta() noexcept
{
    return std::exchange(pending_orbit_, std::nullopt);
}

void InteractionController::accumulate_orbit(float yaw_degrees,
                                             float pitch_degrees) noexcept
{
    if (!pending_orbit_) {
        pending_orbit_ = OrbitDelta{yaw_degrees, pitch_degrees};
        return;
    }

    // Several moves can land between two frames; they add up rather than
    // replacing one another, or the movement between them would be lost.
    pending_orbit_->yaw_degrees += yaw_degrees;
    pending_orbit_->pitch_degrees += pitch_degrees;
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

bool InteractionController::is_busy() const noexcept
{
    // An orbit is deliberately absent: it has no commit to protect, so a
    // sweep in progress blocks neither moves nor, later, queued playback.
    return gesture_.has_value() || snap_.has_value() || committed_.has_value();
}

void InteractionController::reset() noexcept
{
    gesture_.reset();
    orbit_.reset();
    snap_.reset();
    committed_.reset();
    pending_orbit_.reset();
}

}  // namespace rubiks::interaction
