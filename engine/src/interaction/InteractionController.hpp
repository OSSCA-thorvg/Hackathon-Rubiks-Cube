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

/**
 * Turns pointer events into layer turns.
 *
 * Three states: idle, dragging a layer at a continuous angle, and snapping
 * that angle to the nearest quarter turn. Only a genuine release can reach the
 * snap, so an interrupted gesture can never change the cube.
 *
 * The controller does not own the CubeState. A finished snap hands back a
 * CubeMove for the caller to apply, which keeps every state change visible at
 * the call site instead of hidden behind a reference.
 */
class InteractionController {
public:
    explicit InteractionController(int size = 3) noexcept : size_(size) {}

    /**
     * Starts a gesture if the pointer is over the cube.
     *
     * The camera and viewport are captured for the rest of the gesture; a
     * resize cancels the gesture rather than letting them go stale.
     *
     * @return true when the cube was grabbed, and only then.
     */
    [[nodiscard]] bool pointer_down(float x, float y,
                                    const graphics::Camera& camera,
                                    const graphics::Rect& viewport) noexcept;

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

    /** The turn to draw, or nothing when the cube is at rest. */
    [[nodiscard]] std::optional<graphics::ActiveRotation> active_rotation()
        const noexcept;

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

    int size_;
    std::optional<Gesture> gesture_;
    std::optional<Snap> snap_;
    std::optional<cube::CubeMove> committed_;
};

}  // namespace rubiks::interaction
