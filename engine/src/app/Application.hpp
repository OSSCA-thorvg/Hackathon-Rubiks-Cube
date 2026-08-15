#pragma once

#include <cstdint>
#include <vector>

#include "cube/CubeMove.hpp"
#include "cube/Cubie.hpp"
#include "graphics/Layout.hpp"
#include "graphics/Palette.hpp"

namespace rubiks::app {

/**
 * Initializes the ThorVG runtime and the software renderer.
 *
 * Dimensions are validated first and invalid ones fail even when already
 * initialized. Repeated calls with valid dimensions succeed without
 * reconfiguring the renderer; the size is then changed through resize().
 *
 * @return true when the runtime and renderer are ready for rendering.
 */
[[nodiscard]] bool initialize(std::uint32_t width,
                              std::uint32_t height) noexcept;

/**
 * Resizes the render target following the resize failure semantics of the
 * Phase 1 contract.
 *
 * @return true when the target uses the new size afterwards.
 */
[[nodiscard]] bool resize(std::uint32_t width, std::uint32_t height) noexcept;

/**
 * Renders one frame into the renderer-owned target.
 *
 * @return true when update, draw, and sync all succeed.
 */
[[nodiscard]] bool render() noexcept;

/**
 * Returns the address of the software pixel buffer.
 *
 * @return zero without a valid software buffer.
 */
[[nodiscard]] std::uintptr_t pixel_buffer() noexcept;

/**
 * Returns the byte length of the software pixel buffer.
 *
 * @return zero without a valid software buffer.
 */
[[nodiscard]] std::uint32_t pixel_byte_length() noexcept;

/**
 * Begins a pointer gesture at a point in drawing-buffer pixels.
 *
 * Pressing a cell of either view drags a layer; pressing anywhere else sweeps
 * the viewpoint around the cube. Which view a press belongs to comes from the
 * layout, so the two never contend for a pixel. Converting CSS pixels and the
 * device pixel ratio is the caller's job, the same split the resize path uses.
 *
 * A snap left running by the previous release is confirmed here rather than
 * blocking the press, so drags in quick succession all reach the cube.
 *
 * While a sequence is playing, a press may only look around the cube: it never
 * reaches a pick, and it is refused outright when the 3D cube is not on screen,
 * for the same reason a press off the net is refused in the flat-only view.
 *
 * A watched pattern is a sequence like any other here, so it too is looked
 * around rather than interrupted; watching ends at a command, and a drag is
 * not one.
 *
 * @return true when a gesture began. False only when none could: before
 *         initialization, for non-finite coordinates, while another gesture is
 *         already running, and for a press with nothing on screen it could
 *         start -- off the net in the flat-only view, or anywhere in that view
 *         while a sequence plays.
 */
[[nodiscard]] bool pointer_down(float x, float y) noexcept;

/** Continues the active gesture. Ignored without one. */
void pointer_move(float x, float y) noexcept;

/** Releases the active gesture, the only path that can turn a layer. */
void pointer_up() noexcept;

/**
 * Abandons the active gesture without turning the cube.
 *
 * For a cancelled pointer or lost capture: the user never let go, so nothing
 * should be committed. A viewpoint sweep has nothing to withhold, so for it
 * this is the same as a release.
 */
void pointer_cancel() noexcept;

/**
 * Advances animation by an elapsed time in milliseconds and applies a turn
 * that has finished.
 *
 * @return true while further frames still have to be drawn.
 */
[[nodiscard]] bool advance(double elapsed_ms) noexcept;

/** The largest scramble that can be asked for. */
inline constexpr std::uint32_t kMaxScrambleMoves = 100;

/**
 * Restarts the cube and plays the deterministic scramble for `seed` into it.
 *
 * The moves are turned rather than applied, so the cube is still solved when
 * this returns and reaches the scrambled state some frames later. Until then
 * the application is busy and the played moves are nobody's.
 *
 * @return true when the sequence was accepted. False for a count outside
 *         1..kMaxScrambleMoves, which leaves the cube exactly as it was.
 */
[[nodiscard]] bool scramble(std::uint32_t seed,
                            std::uint32_t move_count) noexcept;

/** Restores the logical cube and clears the current solve session. */
void reset_cube() noexcept;

/** How many repeating patterns the watching mode chooses between. */
inline constexpr std::uint32_t kAmbientPatternCount = 4;

/**
 * How many times a pattern may repeat before the cube is back where it began.
 *
 * A bound rather than the exact orders, which are worked out in the phase
 * document and live nowhere else. Nothing in the engine reads an order -- the
 * repeat just keeps taking the next move -- so writing them down here would be
 * a set of numbers that only the tests guarding them ever looked at. What the
 * watching actually promises is that it comes back round within a while, and
 * that is a bound; it goes on holding when the table is edited, which is when
 * the promise is easiest to break.
 */
inline constexpr int kAmbientMaxPeriod = 200;

/**
 * The pattern one choice selects, taken modulo the size of the table.
 *
 * Every value is a valid choice, so there is no rejection path and no size to
 * ask for before choosing. The table is fixed and public because the property
 * that matters about it -- that each of its patterns brings the cube back
 * round within kAmbientMaxPeriod -- is arithmetic, and checking arithmetic
 * needs the numbers rather than an application to play them into.
 */
[[nodiscard]] const std::vector<cube::CubeMove>& ambient_pattern(
    std::uint32_t choice) noexcept;

/**
 * Begins watching: the cube is put away and a pattern repeats until stopped.
 *
 * The pattern is played through the same path a scramble takes, so the moves
 * are turned rather than applied and none of them is anybody's. It never runs
 * out, so the application stays busy and asking for frames until it is stopped.
 *
 * @return true when watching began; false before initialization and when it
 *         had already begun.
 */
[[nodiscard]] bool ambient_start(std::uint32_t choice) noexcept;

/**
 * Ends watching and puts back the cube from the moment it began.
 *
 * Without animation: this is not a move but the undoing of an interlude, and
 * there is nothing about it to watch. The move count and the viewpoint are
 * untouched, so nothing that happened while watching is left behind.
 *
 * A no-op when nothing is being watched.
 */
void ambient_stop() noexcept;

/** Returns whether a pattern is being watched right now. */
[[nodiscard]] bool is_ambient() noexcept;

/** Returns whether the committed logical cube is solved. */
[[nodiscard]] bool is_solved() noexcept;

/**
 * Returns the number of user moves committed since the latest scramble.
 *
 * Derived from the record rather than counted: it is the part of the timeline
 * past the scramble that is currently on the cube. A stored counter would need
 * a correction of its own at every rewind, every discarded redo tail and every
 * solve, and a derived one is right by construction at all of them.
 */
[[nodiscard]] std::uint32_t committed_move_count() noexcept;

/**
 * Takes back the user's last move, by turning it back rather than undoing it.
 *
 * The move is played as a sequence of one, so it settles on screen the way
 * every other turn does and the cursor follows the commit.
 *
 * Refused while anything else owns the cube, and refused once the cursor has
 * reached the end of the scramble: what is below that is not the user's to
 * take back. Use stop_playback() to break off a sequence rather than waiting
 * for one to end.
 *
 * @return true when a rewind began.
 */
[[nodiscard]] bool undo() noexcept;

/**
 * Plays back the move a rewind took off, exactly as it was made.
 *
 * @return true when a replay began; false when nothing has been rewound or
 *         something else owns the cube.
 */
[[nodiscard]] bool redo() noexcept;

/**
 * Rewinds every applied move, leaving a solved cube.
 *
 * The same function undo uses with a different target, so a solve is a longer
 * plan and nothing else. It can be broken off part way; where it stops, the
 * record and the cube agree.
 *
 * @return true when a rewind began; false with nothing applied, or while
 *         something else owns the cube.
 */
[[nodiscard]] bool solve_rewind() noexcept;

/**
 * Breaks off a rewind, keeping everything it has already turned.
 *
 * The turn in flight is confirmed rather than dropped -- winding a rotation
 * back mid-way looks worse than letting it land, and confirming it goes
 * through the same commit as every other move, so the record follows it.
 *
 * A no-op for anything else being played: a scramble broken off half way is a
 * cube nobody asked for, and a watched pattern stopped this way would never
 * put back the cube it borrowed.
 */
void stop_playback() noexcept;

/** How many moves the record holds, scramble and user moves together. */
[[nodiscard]] std::uint32_t timeline_length() noexcept;

/** How many of those moves are on the cube right now. */
[[nodiscard]] std::uint32_t timeline_cursor() noexcept;

/**
 * Where the scramble stops and the user's own moves begin.
 *
 * With the two above, everything the screen and a shared link need: the
 * scramble is `[0, scramble_end)`, the user's moves are `[scramble_end,
 * length)`, and a cursor of nothing on a scramble that exists is a cube a
 * rewind solved rather than a person.
 */
[[nodiscard]] std::uint32_t timeline_scramble_end() noexcept;

/**
 * The recorded move at an index, packed as cube::pack writes it.
 *
 * The record is one list, so reading it is one query: the scramble and the
 * user's own moves come back through the same index and are told apart by
 * timeline_scramble_end() rather than by asking twice.
 *
 * @return zero for an index the record does not hold, which is the same value
 *         a packed move can never take.
 */
[[nodiscard]] std::uint32_t timeline_move(std::uint32_t index) noexcept;

/**
 * Starts one animated face-relative turn.
 *
 * `face_turns` is 1 clockwise, -1 counter-clockwise, or 2 for a half turn as
 * seen from outside `face`.
 *
 * @return true when the command was accepted.
 */
[[nodiscard]] bool turn_face(cube::Face face, int face_turns) noexcept;

/** Changes the rendered views while preserving cube, camera, and game state. */
[[nodiscard]] bool set_view_mode(graphics::ViewMode mode) noexcept;

/** Returns the current view mode, defaulting to Both outside a lifecycle. */
[[nodiscard]] graphics::ViewMode view_mode() noexcept;

/**
 * Chooses which drawing fills the flat region, wherever that region is.
 *
 * Independent of the view mode, so it can be changed while the flat view is
 * not on screen and be waiting when it comes back.
 */
[[nodiscard]] bool set_flat_style(graphics::FlatStyle style) noexcept;

/** Returns the current flat style, defaulting to Net outside a lifecycle. */
[[nodiscard]] graphics::FlatStyle flat_style() noexcept;

/**
 * Chooses which six shades the stickers are drawn in.
 *
 * Nothing but the next frame reads it -- not the cube, not the record, not the
 * clock, not a turn in progress -- so unlike the view commands this one
 * neither cancels a gesture nor relays out the surface, and is accepted while
 * the engine is busy.
 */
[[nodiscard]] bool set_palette(graphics::Palette palette) noexcept;

/** Returns the current palette, defaulting to Classic outside a lifecycle. */
[[nodiscard]] graphics::Palette palette() noexcept;

/**
 * Sets how much faster than the written tempos every animation runs.
 *
 * `duration = base / scale`, so 2 is twice as fast and 0.5 is half. One value
 * covers a user's own release, a scramble, a rewind and a watched pattern,
 * because a speed is a property of the cube rather than of who turned it.
 *
 * Out-of-range values are clamped and accepted; only a value that is not a
 * number at all is refused. A turn already running keeps the duration it
 * began with.
 *
 * @return true when the engine was there to take it.
 */
[[nodiscard]] bool set_speed_scale(float scale) noexcept;

/** Returns the current multiplier, defaulting to 1 outside a lifecycle. */
[[nodiscard]] float speed_scale() noexcept;

/** Restores the home camera without changing cube or view mode. */
void reset_view() noexcept;

/**
 * Returns whether a gesture, animation, unconsumed commit, or playback is on.
 *
 * An orbit sweep is not busy; the viewpoint can move while the cube cannot.
 * A sequence being played is, from the moment it is accepted until its last
 * turn has settled, so the value never falls away between two of its moves --
 * and a pattern being watched, which has no last turn, for as long as it runs.
 */
[[nodiscard]] bool is_busy() noexcept;

/**
 * Releases the renderer and the ThorVG runtime.
 *
 * Safe to call regardless of the initialization state.
 */
void shutdown() noexcept;

/**
 * Reports whether this application owns an active ThorVG initialization.
 *
 * @return true when initialize() has completed successfully.
 */
[[nodiscard]] bool is_initialized() noexcept;

}  // namespace rubiks::app
