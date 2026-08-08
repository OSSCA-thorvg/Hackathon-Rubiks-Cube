#pragma once

#include <optional>

#include "cube/CubeMove.hpp"
#include "graphics/ActiveRotation.hpp"
#include "graphics/Camera.hpp"
#include "graphics/Rect.hpp"
#include "interaction/DragResolver.hpp"
#include "interaction/Picking.hpp"
#include "math/Types.hpp"

namespace rubiks::interaction {

/** Drag distance, as a fraction of the viewport width, before an axis locks. */
inline constexpr float kDeadZoneFraction = 0.01f;

/** Dragging across this fraction of the viewport width is one quarter turn. */
inline constexpr float kQuarterTurnFraction = 0.5f;

/**
 * The same for a drag that sweeps the viewpoint rather than a layer.
 *
 * A separate constant despite the matching value: the two are unrelated feels
 * that happen to start out equal, and tuning one should not move the other.
 */
inline constexpr float kOrbitQuarterTurnFraction = 0.5f;

/** Snap pacing: time for a full quarter turn, and a floor for short snaps. */
inline constexpr double kSnapMsPerQuarterTurn = 200.0;
inline constexpr double kMinSnapMs = 60.0;

/**
 * Upper bound on one injected frame delta.
 *
 * A tab returning from the background reports a delta covering the whole time
 * it was away. Clamping is not needed for correctness, since a long delta just
 * finishes the snap, but it keeps the behaviour the same as an ordinary frame.
 */
inline constexpr double kMaxFrameMs = 250.0;

/** How far a drag swept the viewpoint, in degrees. */
struct OrbitDelta {
    float yaw_degrees;
    float pitch_degrees;
};

/**
 * Turns pointer events into layer turns and viewpoint changes.
 *
 * Pressing the cube drags a layer at a continuous angle, which snaps to the
 * nearest quarter turn on release; pressing anywhere else sweeps the
 * viewpoint, which has nothing to snap to and commits nothing. Only a genuine
 * release of a layer drag can reach the snap, so an interrupted gesture can
 * never change the cube.
 *
 * The controller owns neither the CubeState nor the camera. A finished snap
 * hands back a CubeMove and an orbit hands back angles, which keeps every
 * state change visible at the call site instead of hidden behind a reference.
 */
class InteractionController {
public:
    explicit InteractionController(int size = 3) noexcept : size_(size) {}

    /**
     * Starts a gesture: a layer drag over the cube, an orbit anywhere else.
     *
     * The camera and viewport are captured for the rest of a layer drag; a
     * resize cancels the gesture rather than letting them go stale.
     *
     * @return true when a gesture began. False only before a gesture is
     *         possible at all: non-finite coordinates, or another gesture or
     *         a snap already running.
     */
    [[nodiscard]] bool pointer_down(float x, float y,
                                    const graphics::Camera& camera,
                                    const graphics::Rect& viewport) noexcept;

    /**
     * Starts an animated move without a pointer gesture.
     *
     * Used by keyboard and DOM controls so every input reaches the same snap
     * and commit path. Layers outside the cube, whole-turn no-ops and input
     * while busy are rejected; the turn keeps the direction it was asked for.
     *
     * @return true when the animation started.
     */
    [[nodiscard]] bool start_move(const cube::CubeMove& move) noexcept;

    /** Updates the turn angle, locking the axis once the drag is deliberate. */
    void pointer_move(float x, float y) noexcept;

    /** Releases the gesture, starting the snap when an axis was locked. */
    void pointer_up() noexcept;

    /**
     * Abandons a drag without changing the cube.
     *
     * For everything that ends a gesture without the user letting go: a
     * cancelled pointer, lost capture, a resize, teardown. A snap already in
     * flight is left alone, because the release that started it was real.
     */
    void cancel() noexcept;

    /**
     * Advances the snap animation by an elapsed time in milliseconds.
     *
     * Non-finite and negative values count as no time passing.
     *
     * @return true while a frame still has to be drawn.
     */
    [[nodiscard]] bool advance(double elapsed_ms) noexcept;

    /** Takes the move a finished snap produced, once. */
    [[nodiscard]] std::optional<cube::CubeMove> take_committed_move() noexcept;

    /**
     * Takes the viewpoint change accumulated since the last call, once.
     *
     * Pointer events arrive more often than frames, so moves accumulate here
     * rather than replacing one another. A delta outlives the gesture that
     * produced it: releasing or cancelling an orbit leaves the last movement
     * to be taken, because there is nothing about a viewpoint to undo.
     */
    [[nodiscard]] std::optional<OrbitDelta> take_orbit_delta() noexcept;

    /** The turn to draw, or nothing when the cube is at rest. */
    [[nodiscard]] std::optional<graphics::ActiveRotation> active_rotation()
        const noexcept;

    /** Returns true while a gesture, snap, or unconsumed commit exists. */
    [[nodiscard]] bool is_busy() const noexcept;

    /** Drops all interaction state, as after a shutdown. */
    void reset() noexcept;

private:
    /** A drag in progress. */
    struct Gesture {
        graphics::Camera camera;
        graphics::Rect viewport;
        Pick pick;
        math::Vec2 start;
        std::optional<AxisCandidate> lock;
        float angle_degrees = 0.0f;
    };

    /** A drag sweeping the viewpoint. */
    struct Orbit {
        graphics::Rect viewport;
        /** The last position seen, so a move is a step rather than a total. */
        math::Vec2 previous;
    };

    /** A release running down to the nearest quarter turn. */
    struct Snap {
        cube::Axis axis;
        cube::LayerMask layers;
        float start_degrees;
        float target_degrees;
        double elapsed_ms = 0.0;
        double duration_ms = 0.0;
    };

    [[nodiscard]] float snap_angle() const noexcept;
    void accumulate_orbit(float yaw_degrees, float pitch_degrees) noexcept;

    int size_;
    std::optional<Gesture> gesture_;
    std::optional<Orbit> orbit_;
    std::optional<Snap> snap_;
    std::optional<cube::CubeMove> committed_;
    std::optional<OrbitDelta> pending_orbit_;
};

}  // namespace rubiks::interaction
