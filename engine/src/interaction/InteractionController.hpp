#pragma once

#include <optional>
#include <vector>

#include "cube/CubeMove.hpp"
#include "graphics/ActiveRotation.hpp"
#include "graphics/NetGeometry.hpp"
#include "graphics/RingsGeometry.hpp"
#include "graphics/Camera.hpp"
#include "graphics/Rect.hpp"
#include "interaction/DragResolver.hpp"
#include "interaction/NetPicking.hpp"
#include "interaction/RingsPicking.hpp"
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

/**
 * The same for a drag across the unfolded net, as a fraction of its width.
 *
 * One face out of the four the cross is wide, so a sticker dragged a quarter
 * turn travels about as far as the face it is heading for.
 */
inline constexpr float kNetQuarterTurnFraction = 0.25f;

/**
 * How long a moving piece takes to lift off the drawing, and to settle again.
 *
 * In milliseconds, and a time rather than an angle on purpose: the lift is one
 * height, held from the moment the turn has a direction until the moment it
 * commits. Sliding it in over a little time is only so that it does not appear
 * at a stroke; a lift that grew and shrank with the angle would have the piece
 * breathing under a finger that was only turning further.
 */
inline constexpr double kOpeningMs = 120.0;

/** Snap pacing: time for a full quarter turn, and a floor for short snaps. */
inline constexpr double kSnapMsPerQuarterTurn = 200.0;
inline constexpr double kMinSnapMs = 60.0;

/**
 * How far past a quarter turn a release commits the next one.
 *
 * The boundary between two snap targets sits here rather than halfway, and at
 * the same place between every pair, so "carry a face past this and it turns"
 * reads the same whichever multiple the finger is near. At the sensitivity
 * above this is a sixth of the viewport width, about a thumb flick.
 *
 * Fixed rather than tunable: the known-answer tests straddle this value, so
 * moving it means recomputing them, and both belong in one change.
 */
inline constexpr float kCommitDegrees = 30.0f;

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
 * Pressing the cube drags a layer at a continuous angle, which snaps on
 * release to the last quarter turn the drag carried it `kCommitDegrees` past;
 * pressing anywhere else sweeps the viewpoint, which has nothing to snap to
 * and commits nothing. Only a genuine release of a layer drag can reach the
 * snap, so an interrupted gesture can never change the cube.
 *
 * A drag on the unfolded net is the third way in and the shortest: it reaches
 * the same snap, so past the release nothing downstream can tell which of the
 * two views a turn was asked for in. One gesture at a time, whichever it was.
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
     * Starts a layer drag on the unfolded net instead of on the cube.
     *
     * The cell is passed in rather than found here, because the caller has to
     * know whether the press landed on a face before it can decide that this
     * is a net gesture at all, and picking it twice would let the two answers
     * disagree.
     *
     * The angle a net drag reaches is capped at one quarter turn: the net
     * shows a cycle one cell at a time, so a drag carried further would settle
     * on a turn the drawing never showed.
     *
     * @return true when a gesture began; false only when another gesture or a
     *         snap is already running, or the coordinates are not finite.
     */
    [[nodiscard]] bool net_pointer_down(float x, float y,
                                        const graphics::Rect& rect,
                                        const NetPick& pick) noexcept;

    /**
     * Starts a layer drag on the ring diagram instead.
     *
     * The sticker is passed in for the same reason the net's cell is: the
     * caller has to know whether the press landed on one before it can decide
     * this is a ring gesture at all, and picking it twice would let the two
     * answers disagree.
     *
     * @return true when a gesture began; false only when another gesture or a
     *         snap is already running, or the coordinates are not finite.
     */
    [[nodiscard]] bool rings_pointer_down(float x, float y,
                                          const graphics::Rect& rect,
                                          const RingsPick& pick) noexcept;

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
     * Ends a snap at its target, handing back the move it had decided.
     *
     * For a press arriving while the previous release is still animating: the
     * turn is already decided, so it is applied at once rather than blocking
     * the new gesture. The move is returned rather than stored, so the caller
     * applies it before the next gesture starts and no ordering question
     * between the two can arise.
     *
     * @return the settled move, or nothing when no snap was running or its
     *         target leaves the cube unchanged.
     */
    [[nodiscard]] std::optional<cube::CubeMove> finish_snap() noexcept;

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

    /**
     * The rings a net press is offering, or the one it has locked onto.
     *
     * Empty at every other moment, which is what gives the guide lines the
     * lifetime of the gesture and saves keeping a selection of their own.
     */
    [[nodiscard]] std::vector<graphics::NetGuide> net_guides() const;

    /**
     * The rings a press on the diagram is offering, or the one it has locked.
     *
     * Empty at every other moment, the same as the net's guides -- except that
     * these are not lines to draw but loops already drawn, so what a caller
     * does with them is pick them out rather than add to the picture.
     */
    [[nodiscard]] std::vector<graphics::RingsGuide> rings_guides() const;

    /** The turn to draw, or nothing when the cube is at rest. */
    [[nodiscard]] std::optional<graphics::ActiveRotation> active_rotation()
        const noexcept;

    /**
     * Returns true while a gesture, snap, or unconsumed commit exists.
     *
     * An orbit sweep is not busy: it has no commit to protect, so it blocks
     * neither moves nor queued playback.
     */
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

    /** The turn a net drag locked onto, with the direction driving it. */
    struct NetLock {
        cube::Axis axis;
        int layer;
        cube::LayerMask layers;
        /** Unit screen direction, one of the four the net's grid allows. */
        math::Vec2 direction;
        /** +1 when moving along `direction` turns the axis positively. */
        float sign;
    };

    /** A drag in progress across the net. */
    struct NetGesture {
        graphics::Rect rect;
        NetPick pick;
        math::Vec2 start;
        std::optional<NetLock> lock;
        float angle_degrees = 0.0f;
    };

    /** The turn a ring drag locked onto. */
    struct RingsLock {
        cube::Axis axis;
        int layer;
        cube::LayerMask layers;
    };

    /** A drag in progress around the ring diagram. */
    struct RingsGesture {
        graphics::Rect rect;
        RingsPick pick;
        math::Vec2 start;
        std::optional<RingsLock> lock;
        /** Where round the locked ring the pointer was, as a slot index. */
        float previous_slot = 0.0f;
        /**
         * Slots travelled since the press, unwrapped.
         *
         * Accumulated a step at a time rather than measured from the press,
         * because a ring closes: a drag carried right round would otherwise
         * read as having come back to where it started.
         */
        float swept_slots = 0.0f;
        float angle_degrees = 0.0f;
    };

    /** A drag sweeping the viewpoint. */
    struct Orbit {
        graphics::Rect viewport;
        /** The last position seen, so a move is a step rather than a total. */
        math::Vec2 previous;
    };

    /**
     * The turn a locked gesture is making, whichever kind of gesture it is.
     *
     * A drag on the cube and a drag on the net decide an axis, a layer and an
     * angle by quite different means and then have nothing left to say apart
     * from those three. Naming the three is what keeps the rest of this class
     * from asking "cube gesture, or net gesture?" once per consumer -- and a
     * third way in would be one more case here rather than one more pair of
     * branches everywhere.
     */
    struct LockedTurn {
        cube::Axis axis;
        cube::LayerMask layers;
        float angle_degrees;
    };

    /** A release running down to the quarter turn it settled on. */
    struct Snap {
        cube::Axis axis;
        cube::LayerMask layers;
        float start_degrees;
        float target_degrees;
        double elapsed_ms = 0.0;
        double duration_ms = 0.0;
    };

    /** True while a pointer owns a gesture of any of the three kinds. */
    [[nodiscard]] bool gesture_running() const noexcept;

    /** The turn a finger is holding, or nothing when none has an axis yet. */
    [[nodiscard]] std::optional<LockedTurn> locked_turn() const noexcept;

    /** Updates a net drag's lock and angle from the pointer's position. */
    void advance_net_gesture(float x, float y) noexcept;

    /** The same for a drag round the ring diagram. */
    void advance_rings_gesture(float x, float y) noexcept;

    /** The two rings the pressed sticker sits on, in axis order. */
    [[nodiscard]] std::vector<graphics::RingsGuide> rings_through_pick() const;

    /** Begins the run-down from a released drag's angle to its quarter turn. */
    void start_snap(cube::Axis axis, cube::LayerMask layers,
                    float angle_degrees) noexcept;

    [[nodiscard]] float snap_angle() const noexcept;

    /** How far a moving piece is held up, for whatever is happening now. */
    [[nodiscard]] float opening() const noexcept;

    /**
     * How long a turn has been on, capped at the time it takes to open.
     *
     * Counted while a turn is being made, whether by a finger or by a snap
     * animating a move nobody is holding, and dropped when the cube settles.
     */
    double opened_ms_ = 0.0;

    /**
     * The move the running snap has decided on, if it changes the cube.
     *
     * Whole circles are dropped here: a drag long enough to come back round
     * animates all the way, but a turn of four is not a turn.
     */
    [[nodiscard]] std::optional<cube::CubeMove> settled_move() const noexcept;

    void accumulate_orbit(float yaw_degrees, float pitch_degrees) noexcept;

    int size_;
    std::optional<Gesture> gesture_;
    std::optional<NetGesture> net_gesture_;
    std::optional<RingsGesture> rings_gesture_;
    std::optional<Orbit> orbit_;
    std::optional<Snap> snap_;
    std::optional<cube::CubeMove> committed_;
    std::optional<OrbitDelta> pending_orbit_;
};

}  // namespace rubiks::interaction
