#include "interaction/InteractionController.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <utility>

#include "graphics/NetGeometry.hpp"
#include "graphics/RingsGeometry.hpp"

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

/** Degrees of turn per pixel of drag across the net. */
[[nodiscard]] float net_degrees_per_pixel(const graphics::Rect& rect) noexcept
{
    return kDegreesPerQuarterTurn / (kNetQuarterTurnFraction * rect.width);
}

/**
 * The angle a release settles on, in degrees.
 *
 * The boundary between two targets is `kCommitDegrees` past the lower one
 * rather than halfway, uniformly, so that under-rotating still turns and the
 * rule reads the same at every multiple. Nothing but the angle is consulted:
 * no time, no speed, no state the gesture had to carry.
 */
[[nodiscard]] float snap_target(float angle_degrees) noexcept
{
    const float magnitude = std::abs(angle_degrees);
    const float biased =
        magnitude + 0.5f * kDegreesPerQuarterTurn - kCommitDegrees;
    const float turns = std::round(biased / kDegreesPerQuarterTurn);

    return std::copysign(turns * kDegreesPerQuarterTurn, angle_degrees);
}

/**
 * How long a settle takes, at a pace given as milliseconds per quarter turn.
 *
 * The pace rather than the total, so a half turn takes twice as long as a
 * quarter and a small correction is over quickly.
 */
[[nodiscard]] double snap_duration(float remaining_degrees,
                                   double tempo_ms) noexcept
{
    const float magnitude = std::abs(remaining_degrees);

    // Nothing left to travel: the next frame commits. Anything else takes at
    // least the floor, so a tiny correction is still visible as motion -- but
    // never longer than a whole quarter turn would take, or at a brisk pace
    // the floor stops guarding the pacing and becomes it.
    if (magnitude <= 0.0f) return 0.0;

    return std::max(std::min(kMinSnapMs, tempo_ms),
                    tempo_ms * static_cast<double>(magnitude) /
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
    if (snap_ || gesture_running()) return false;
    if (!finite_point(x, y)) return false;
    if (viewport.width <= 0.0f || viewport.height <= 0.0f) return false;

    std::optional<Pick> pick;
    if (const auto ray = pointer_ray(x, y, camera, viewport)) {
        pick = pick_cube(*ray, size_);
    }

    // Missing the cube is not nothing: it is a request to look around it.
    // Where the press landed does not matter, only that it was not the cube.
    if (!pick) return start_orbit(x, y, viewport);

    gesture_ = Gesture{camera, viewport, *pick, math::Vec2{x, y},
                       std::nullopt, 0.0f};
    return true;
}

bool InteractionController::start_orbit(
    float x, float y, const graphics::Rect& viewport) noexcept
{
    // A running snap is not consulted, unlike everywhere else. Looking around
    // takes nothing away from a turn that is settling, and a caller playing a
    // sequence back has one running almost all of the time.
    if (gesture_running()) return false;
    if (!finite_point(x, y)) return false;
    if (viewport.width <= 0.0f || viewport.height <= 0.0f) return false;

    orbit_ = Orbit{viewport, math::Vec2{x, y}};
    return true;
}

bool InteractionController::net_pointer_down(float x, float y,
                                             const graphics::Rect& rect,
                                             const NetPick& pick) noexcept
{
    if (snap_ || gesture_running()) return false;
    if (!finite_point(x, y)) return false;
    if (rect.width <= 0.0f || rect.height <= 0.0f) return false;

    net_gesture_ = NetGesture{rect, pick, math::Vec2{x, y}, std::nullopt, 0.0f};
    return true;
}

bool InteractionController::rings_pointer_down(float x, float y,
                                               const graphics::Rect& rect,
                                               const RingsPick& pick) noexcept
{
    if (snap_ || gesture_running()) return false;
    if (!finite_point(x, y)) return false;
    if (rect.width <= 0.0f || rect.height <= 0.0f) return false;

    rings_gesture_ = RingsGesture{rect, pick, math::Vec2{x, y}, std::nullopt,
                                  0.0f, 0.0f, 0.0f};
    return true;
}

bool InteractionController::start_move(const cube::CubeMove& move,
                                       double tempo_ms) noexcept
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

    // The turn starts at rest, and so does the piece: nobody has hold of it,
    // so it has to be lifted rather than found already up. Without this a move
    // asked for while the previous one is still being cleared away -- which,
    // played back, is every move after the first -- starts at full height.
    opened_ms_ = 0.0;

    // The whole target is still to travel.
    snap_ = Snap{move.axis, move.layers, 0.0f, target,
                 0.0,       snap_duration(target, tempo_ms)};
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

    if (net_gesture_) {
        if (!finite_point(x, y)) return;
        advance_net_gesture(x, y);
        return;
    }

    if (rings_gesture_) {
        if (!finite_point(x, y)) return;
        advance_rings_gesture(x, y);
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

void InteractionController::advance_net_gesture(float x, float y) noexcept
{
    const math::Vec2 drag{x - net_gesture_->start.x, y - net_gesture_->start.y};

    if (!net_gesture_->lock) {
        const float dead_zone = kDeadZoneFraction * net_gesture_->rect.width;
        if (math::length(drag) < dead_zone) return;

        // The net's screen directions are fixed, so the lock is whichever of
        // the two the drag is more along; there is no candidate to project.
        const bool sideways = std::abs(drag.x) >= std::abs(drag.y);
        const int col_step = sideways ? (drag.x < 0.0f ? -1 : 1) : 0;
        const int row_step = sideways ? 0 : (drag.y < 0.0f ? -1 : 1);

        const auto turn = graphics::net_step_turn(
            net_gesture_->pick.face, net_gesture_->pick.col,
            net_gesture_->pick.row, col_step, row_step, size_);
        if (!turn) return;

        net_gesture_->lock =
            NetLock{turn->axis, turn->layer, cube::layer(turn->layer),
                    math::Vec2{static_cast<float>(col_step),
                               static_cast<float>(row_step)},
                    static_cast<float>(turn->sign)};
    }

    const auto& lock = *net_gesture_->lock;
    const float along = drag.x * lock.direction.x + drag.y * lock.direction.y;

    // Uncapped, as a drag on the cube is. The net used to stop at a quarter
    // because it only drew one, and a drag carried further would have settled
    // on a turn the drawing never showed; now the bands go round their loops
    // as far as they are taken, so the two views agree at any angle.
    net_gesture_->angle_degrees =
        lock.sign * along * net_degrees_per_pixel(net_gesture_->rect);
}

void InteractionController::advance_rings_gesture(float x, float y) noexcept
{
    const math::Vec2 at{x, y};
    const auto& rect = rings_gesture_->rect;

    if (!rings_gesture_->lock) {
        const math::Vec2 drag{x - rings_gesture_->start.x,
                              y - rings_gesture_->start.y};
        const float dead_zone = kDeadZoneFraction * rect.width;
        if (math::length(drag) < dead_zone) return;

        // The pressed sticker is on exactly two rings, so the drag is choosing
        // between two candidates rather than searching. Each ring's tangent
        // where the sticker sits is the way a positive turn carries it, so the
        // better match is the ring and the sign of the match is the direction
        // -- there is no table of faces anywhere in this.
        std::optional<RingsLock> best;
        float strongest = 0.0f;

        for (const auto& candidate : rings_through_pick()) {
            const auto tangent = graphics::rings_slot_tangent(
                candidate.axis, candidate.layer, rect, size_,
                rings_gesture_->pick.sticker);
            if (!tangent) continue;

            const float along = drag.x * tangent->x + drag.y * tangent->y;
            if (std::abs(along) <= std::abs(strongest)) continue;

            strongest = along;
            best = RingsLock{candidate.axis, candidate.layer,
                             cube::layer(candidate.layer)};
        }

        if (!best) return;
        rings_gesture_->lock = *best;

        // Counted from the press rather than from here, so the turn does not
        // jump by however far the drag had to travel to leave the dead zone.
        const auto from = graphics::rings_slot_at(
            best->axis, best->layer, rect, size_, rings_gesture_->start);
        if (!from) {
            rings_gesture_->lock.reset();
            return;
        }
        rings_gesture_->previous_slot = *from;
    }

    const auto& lock = *rings_gesture_->lock;
    const auto now =
        graphics::rings_slot_at(lock.axis, lock.layer, rect, size_, at);
    if (!now) return;

    // The step since the last position, brought back into half a loop either
    // way. A pointer cannot cross more than that between two events, so this
    // is what lets a drag go round and round without the angle folding over.
    const auto slots = static_cast<float>(4 * size_);
    float step = *now - rings_gesture_->previous_slot;
    while (step <= -0.5f * slots) step += slots;
    while (step > 0.5f * slots) step -= slots;

    rings_gesture_->previous_slot = *now;
    rings_gesture_->swept_slots += step;

    // N slots is a quarter turn, whichever ring it is, and slots run the way a
    // positive turn does -- so the angle needs no sign of its own.
    rings_gesture_->angle_degrees = rings_gesture_->swept_slots /
                                    static_cast<float>(size_) *
                                    kDegreesPerQuarterTurn;
}

void InteractionController::pointer_up(double tempo_ms) noexcept
{
    // Nothing to settle or commit, and the accumulated sweep stays behind to
    // be taken: a viewpoint is where the user left it, not something to undo.
    if (orbit_) {
        orbit_.reset();
        return;
    }

    // Past here the two kinds of layer drag are one: a turn that locked runs
    // down to its quarter, and one that never did was a tap inside the dead
    // zone. Only one of the two can be in flight, so both are let go.
    if (const auto turn = locked_turn()) {
        start_snap(turn->axis, turn->layers, turn->angle_degrees, tempo_ms);
    }

    gesture_.reset();
    net_gesture_.reset();
    rings_gesture_.reset();
}

void InteractionController::start_snap(cube::Axis axis, cube::LayerMask layers,
                                       float angle_degrees,
                                       double tempo_ms) noexcept
{
    const float target = snap_target(angle_degrees);

    snap_ = Snap{axis,          layers, angle_degrees, target, 0.0,
                 snap_duration(target - angle_degrees, tempo_ms)};
}

std::optional<cube::CubeMove> InteractionController::finish_snap() noexcept
{
    const auto move = settled_move();
    snap_.reset();
    return move;
}

void InteractionController::cancel() noexcept
{
    gesture_.reset();
    net_gesture_.reset();
    rings_gesture_.reset();

    // Same as a release for an orbit: there is no commit to withhold, and the
    // sweep already made stays pending rather than being thrown away.
    orbit_.reset();
}

bool InteractionController::advance(double elapsed_ms) noexcept
{
    const double delta = sanitized_delta(elapsed_ms);

    // A piece lifts for a turn that has a direction, whoever is making it, and
    // forgets it was ever up once the cube settles.
    const bool turning = snap_ || locked_turn().has_value();
    opened_ms_ = turning ? std::min(opened_ms_ + delta, kOpeningMs) : 0.0;

    if (!snap_) return gesture_running();

    snap_->elapsed_ms += delta;
    if (snap_->elapsed_ms < snap_->duration_ms) return true;

    if (const auto move = settled_move()) committed_ = *move;
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

std::optional<cube::CubeMove> InteractionController::settled_move()
    const noexcept
{
    if (!snap_) return std::nullopt;

    const auto quarter_turns = static_cast<int>(
        std::lround(snap_->target_degrees / kDegreesPerQuarterTurn));

    // A drag that came the whole way round leaves the cube exactly as it was.
    // The animation still runs to 360, because that is what the finger did,
    // but there is no move to report at the end of it.
    if (quarter_turns % 4 == 0) return std::nullopt;

    return cube::CubeMove{snap_->axis, snap_->layers, quarter_turns};
}

std::vector<graphics::NetGuide> InteractionController::net_guides() const
{
    std::vector<graphics::NetGuide> guides;
    if (!net_gesture_) return guides;

    const auto pressed =
        graphics::net_cell(net_gesture_->pick.face, net_gesture_->pick.col,
                           net_gesture_->pick.row, size_);

    if (net_gesture_->lock) {
        guides.push_back(graphics::NetGuide{
            net_gesture_->lock->axis, net_gesture_->lock->layer, pressed});
        return guides;
    }

    // Before the drag has said which way it is going, both rings through the
    // pressed cell are offered. The four drawn directions name each of them
    // twice, once each way round, so the second sighting is a repeat.
    static constexpr std::array<std::pair<int, int>, 4> kSteps{
        {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

    for (const auto& [col_step, row_step] : kSteps) {
        const auto turn = graphics::net_step_turn(
            net_gesture_->pick.face, net_gesture_->pick.col,
            net_gesture_->pick.row, col_step, row_step, size_);
        if (!turn) continue;

        const bool seen =
            std::any_of(guides.begin(), guides.end(),
                        [&](const graphics::NetGuide& guide) {
                            return guide.axis == turn->axis &&
                                   guide.layer == turn->layer;
                        });
        if (!seen) {
            guides.push_back(
                graphics::NetGuide{turn->axis, turn->layer, pressed});
        }
    }
    return guides;
}

float InteractionController::opening() const noexcept
{
    const auto opened = static_cast<float>(
        std::clamp(opened_ms_ / kOpeningMs, 0.0, 1.0));

    // The height the lift has reached is held for the whole turn: it is not a
    // function of how far round the turn has got. Only the last moments of the
    // settle put it down, so the piece is back on the drawing by the time the
    // commit lands and there is nothing for the commit to jump over -- and a
    // long turn stays up all the way to those last moments rather than sinking
    // across the whole animation.
    if (snap_) {
        const double left = snap_->duration_ms - snap_->elapsed_ms;
        return opened *
               static_cast<float>(std::clamp(left / kOpeningMs, 0.0, 1.0));
    }

    return locked_turn() ? opened : 0.0f;
}

std::optional<graphics::ActiveRotation> InteractionController::active_rotation()
    const noexcept
{
    // The settle owns the turn once the finger lets go, so it is asked first.
    if (snap_) {
        return graphics::ActiveRotation{snap_->axis, snap_->layers,
                                        snap_angle(), opening()};
    }

    // Before the axis locks there is no layer to turn, so the cube is still
    // at rest even though a pointer is down. Past that, a drag on the cube and
    // a drag on the net produce the same value, so both views show the same
    // turn whichever of them it was started in.
    if (const auto turn = locked_turn()) {
        return graphics::ActiveRotation{turn->axis, turn->layers,
                                        turn->angle_degrees, opening()};
    }

    return std::nullopt;
}

bool InteractionController::gesture_running() const noexcept
{
    return gesture_.has_value() || net_gesture_.has_value() ||
           rings_gesture_.has_value() || orbit_.has_value();
}

std::optional<InteractionController::LockedTurn>
InteractionController::locked_turn() const noexcept
{
    // A cube drag knows its layer only through the cubie it took hold of; a
    // net drag was told which layer when its direction resolved. That is the
    // whole difference, and it ends here.
    if (gesture_ && gesture_->lock) {
        return LockedTurn{
            gesture_->lock->axis,
            cube::layer(layer_of(gesture_->pick, gesture_->lock->axis)),
            gesture_->angle_degrees};
    }

    if (net_gesture_ && net_gesture_->lock) {
        return LockedTurn{net_gesture_->lock->axis, net_gesture_->lock->layers,
                          net_gesture_->angle_degrees};
    }

    if (rings_gesture_ && rings_gesture_->lock) {
        return LockedTurn{rings_gesture_->lock->axis,
                          rings_gesture_->lock->layers,
                          rings_gesture_->angle_degrees};
    }

    return std::nullopt;
}

std::vector<graphics::RingsGuide> InteractionController::rings_through_pick()
    const
{
    std::vector<graphics::RingsGuide> rings;
    if (!rings_gesture_) return rings;

    // Every axis but the one the sticker's own face points along: a turn about
    // that one spins it where it is rather than carrying it anywhere.
    const auto& sticker = rings_gesture_->pick.sticker;
    for (const auto axis : {cube::Axis::X, cube::Axis::Y, cube::Axis::Z}) {
        if (axis == cube::axis_of(sticker.face)) continue;
        rings.push_back(
            graphics::RingsGuide{axis, cube::coordinate_on(axis, sticker)});
    }
    return rings;
}

std::vector<graphics::RingsGuide> InteractionController::rings_guides() const
{
    if (!rings_gesture_) return {};

    if (const auto& lock = rings_gesture_->lock) {
        return {graphics::RingsGuide{lock->axis, lock->layer}};
    }
    return rings_through_pick();
}

bool InteractionController::is_busy() const noexcept
{
    // An orbit is deliberately absent: it has no commit to protect, so a
    // sweep in progress blocks neither moves nor, later, queued playback.
    return gesture_.has_value() || net_gesture_.has_value() ||
           rings_gesture_.has_value() || snap_.has_value() ||
           committed_.has_value();
}

void InteractionController::reset() noexcept
{
    gesture_.reset();
    net_gesture_.reset();
    rings_gesture_.reset();
    orbit_.reset();
    snap_.reset();
    committed_.reset();
    opened_ms_ = 0.0;
    pending_orbit_.reset();
}

}  // namespace rubiks::interaction
