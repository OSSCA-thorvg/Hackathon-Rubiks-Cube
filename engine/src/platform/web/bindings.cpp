#include <cstdint>

#include <emscripten/emscripten.h>

#include "app/Application.hpp"

// C ABI for browser clients. Every function uses primitive types only.
// Return contract: 1 on success, 0 on failure; pointer and byte length
// queries return 0 without a valid software buffer.

extern "C" {

/**
 * Initializes the engine with the initial drawing buffer size in pixels.
 *
 * @return one when initialization succeeds; otherwise zero.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_initialize(
    std::uint32_t width, std::uint32_t height) noexcept
{
    return rubiks::app::initialize(width, height) ? 1 : 0;
}

/**
 * Resizes the drawing buffer following the resize failure semantics.
 *
 * @return one when the buffer uses the new size afterwards; otherwise zero.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_resize(std::uint32_t width,
                                              std::uint32_t height) noexcept
{
    return rubiks::app::resize(width, height) ? 1 : 0;
}

/**
 * Renders one frame into the engine-owned pixel buffer.
 *
 * The buffer is safe to read only after this function returns one.
 *
 * @return one when the frame completed, including sync; otherwise zero.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_render() noexcept
{
    return rubiks::app::render() ? 1 : 0;
}

/**
 * Returns the current pixel buffer address in WASM linear memory.
 *
 * The address stays valid from a successful initialize or resize until the
 * next resize or shutdown.
 *
 * @return zero without a valid software buffer.
 */
EMSCRIPTEN_KEEPALIVE std::uintptr_t thorvg_rubiks_pixel_buffer() noexcept
{
    return rubiks::app::pixel_buffer();
}

/**
 * Returns the pixel buffer length in bytes (width * height * 4).
 *
 * @return zero without a valid software buffer.
 */
EMSCRIPTEN_KEEPALIVE std::uint32_t thorvg_rubiks_pixel_byte_length() noexcept
{
    return rubiks::app::pixel_byte_length();
}

/**
 * Begins a pointer gesture. Coordinates are drawing-buffer pixels.
 *
 * Pressing a cell of either the cube or the net drags a layer, pressing
 * elsewhere sweeps the viewpoint. A snap still animating is confirmed rather
 * than allowed to block the press.
 *
 * @return one when a gesture began; zero before initialization, for
 *         non-finite coordinates, and while another gesture is running.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_pointer_down(float x, float y) noexcept
{
    return rubiks::app::pointer_down(x, y) ? 1 : 0;
}

/**
 * Continues the active gesture. A no-op without one.
 */
EMSCRIPTEN_KEEPALIVE void thorvg_rubiks_pointer_move(float x, float y) noexcept
{
    rubiks::app::pointer_move(x, y);
}

/**
 * Releases the active gesture, the only path that can turn the cube.
 */
EMSCRIPTEN_KEEPALIVE void thorvg_rubiks_pointer_up() noexcept
{
    rubiks::app::pointer_up();
}

/**
 * Abandons the active gesture without turning the cube.
 */
EMSCRIPTEN_KEEPALIVE void thorvg_rubiks_pointer_cancel() noexcept
{
    rubiks::app::pointer_cancel();
}

/**
 * Advances animation by an elapsed time in milliseconds.
 *
 * The engine reads no clock of its own, so this is the only source of time.
 *
 * @return one while further frames still have to be drawn; otherwise zero.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_advance(double elapsed_ms) noexcept
{
    return rubiks::app::advance(elapsed_ms) ? 1 : 0;
}

/**
 * Restarts the cube and plays `move_count` scramble moves into it.
 *
 * The cube is still solved when this returns; it arrives at the scrambled
 * state once the sequence has been played, and is busy until then.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_scramble(
    std::uint32_t seed, std::uint32_t move_count) noexcept
{
    return rubiks::app::scramble(seed, move_count) ? 1 : 0;
}

/** Restores the solved cube while preserving camera and view mode. */
EMSCRIPTEN_KEEPALIVE void thorvg_rubiks_reset_cube() noexcept
{
    rubiks::app::reset_cube();
}

/**
 * Begins watching a repeating pattern, chosen by `choice` modulo the table.
 *
 * Every value is a valid choice, so this is where the arbitrariness comes in:
 * the browser has a random source and the engine has none.
 *
 * @return one when watching began; zero when it had already begun.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_ambient_start(
    std::uint32_t choice) noexcept
{
    return rubiks::app::ambient_start(choice) ? 1 : 0;
}

/**
 * Ends watching, putting back the cube from the moment it began.
 *
 * Immediate rather than animated, and a no-op when nothing is being watched.
 */
EMSCRIPTEN_KEEPALIVE void thorvg_rubiks_ambient_stop() noexcept
{
    rubiks::app::ambient_stop();
}

/** Returns one while a pattern is being watched. */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_is_ambient() noexcept
{
    return rubiks::app::is_ambient() ? 1 : 0;
}

/** Returns one when the committed logical cube is solved. */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_is_solved() noexcept
{
    return rubiks::app::is_solved() ? 1 : 0;
}

/**
 * Returns user moves committed since the latest scramble or reset.
 *
 * Read off the record rather than counted, so a rewind takes moves out of it
 * as surely as making them put them in.
 */
EMSCRIPTEN_KEEPALIVE std::uint32_t
thorvg_rubiks_committed_move_count() noexcept
{
    return rubiks::app::committed_move_count();
}

/**
 * Turns the user's last move back.
 *
 * @return one when a rewind began; zero with nothing of the user's own on the
 *         cube, or while anything else owns it.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_undo() noexcept
{
    return rubiks::app::undo() ? 1 : 0;
}

/**
 * Plays back the move a rewind took off.
 *
 * @return one when a replay began; zero when nothing has been rewound.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_redo() noexcept
{
    return rubiks::app::redo() ? 1 : 0;
}

/**
 * Rewinds every applied move, leaving a solved cube.
 *
 * @return one when a rewind began; zero with nothing applied.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_solve_rewind() noexcept
{
    return rubiks::app::solve_rewind() ? 1 : 0;
}

/**
 * Breaks off a rewind, keeping everything it has already turned.
 *
 * A no-op for a scramble or a watched pattern, which have to reach their end.
 */
EMSCRIPTEN_KEEPALIVE void thorvg_rubiks_stop_playback() noexcept
{
    rubiks::app::stop_playback();
}

/** How many moves the record holds, scramble and user moves together. */
EMSCRIPTEN_KEEPALIVE std::uint32_t thorvg_rubiks_timeline_length() noexcept
{
    return rubiks::app::timeline_length();
}

/**
 * How many of those moves are on the cube right now.
 *
 * Every commit moves this by exactly one, so a caller watching it change is
 * watching moves commit, and there is no counter beside it saying the same.
 */
EMSCRIPTEN_KEEPALIVE std::uint32_t thorvg_rubiks_timeline_cursor() noexcept
{
    return rubiks::app::timeline_cursor();
}

/** Where the scramble stops and the user's own moves begin. */
EMSCRIPTEN_KEEPALIVE std::uint32_t
thorvg_rubiks_timeline_scramble_end() noexcept
{
    return rubiks::app::timeline_scramble_end();
}

/**
 * Returns the recorded move at `index`, packed into a single word.
 *
 * Axis in bits 0-1, turns in bits 2-3 as 0 = -1, 1 = +1, 2 = +2, and the layer
 * mask from bit 4 up. Notation is assembled from these on the other side, so
 * no string crosses here and a change of notation never reaches the engine.
 *
 * @return zero for an index the record does not hold; a packed move is never
 *         zero, because a move always turns at least one layer.
 */
EMSCRIPTEN_KEEPALIVE std::uint32_t thorvg_rubiks_timeline_move(
    std::uint32_t index) noexcept
{
    return rubiks::app::timeline_move(index);
}

/**
 * Starts an animated face-relative turn.
 *
 * Face follows cube::Face order and turns are -1, 1, or 2.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_turn_face(int face,
                                                int face_turns) noexcept
{
    constexpr int kFirstFace = static_cast<int>(rubiks::cube::Face::Right);
    constexpr int kLastFace = static_cast<int>(rubiks::cube::Face::Back);
    if (face < kFirstFace || face > kLastFace) return 0;

    return rubiks::app::turn_face(static_cast<rubiks::cube::Face>(face),
                                  face_turns)
               ? 1
               : 0;
}

/** Changes the visible render regions; invalid integer values are rejected. */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_set_view_mode(int mode) noexcept
{
    constexpr int kFirstMode =
        static_cast<int>(rubiks::graphics::ViewMode::Cube3D);
    constexpr int kLastMode =
        static_cast<int>(rubiks::graphics::ViewMode::Flat);
    if (mode < kFirstMode || mode > kLastMode) return 0;

    return rubiks::app::set_view_mode(
               static_cast<rubiks::graphics::ViewMode>(mode))
               ? 1
               : 0;
}

/** Returns the current graphics::ViewMode integer. */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_view_mode() noexcept
{
    return static_cast<int>(rubiks::app::view_mode());
}

/** Chooses which drawing fills the flat region; invalid values are rejected. */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_set_flat_style(int style) noexcept
{
    constexpr int kFirstStyle =
        static_cast<int>(rubiks::graphics::FlatStyle::Net);
    constexpr int kLastStyle =
        static_cast<int>(rubiks::graphics::FlatStyle::Both);
    if (style < kFirstStyle || style > kLastStyle) return 0;

    return rubiks::app::set_flat_style(
               static_cast<rubiks::graphics::FlatStyle>(style))
               ? 1
               : 0;
}

/** Returns the current graphics::FlatStyle integer. */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_flat_style() noexcept
{
    return static_cast<int>(rubiks::app::flat_style());
}

/** Chooses which six shades the stickers take; invalid values are rejected. */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_set_palette(int palette) noexcept
{
    constexpr int kFirst = static_cast<int>(rubiks::graphics::Palette::Classic);
    constexpr int kLast =
        static_cast<int>(rubiks::graphics::Palette::HighContrast);
    if (palette < kFirst || palette > kLast) return 0;

    return rubiks::app::set_palette(
               static_cast<rubiks::graphics::Palette>(palette))
               ? 1
               : 0;
}

/** Returns the current graphics::Palette integer. */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_palette() noexcept
{
    return static_cast<int>(rubiks::app::palette());
}

/** Restores only the turntable camera. */
EMSCRIPTEN_KEEPALIVE void thorvg_rubiks_reset_view() noexcept
{
    rubiks::app::reset_view();
}

/** Returns one while a gesture, snap, or commit is active; orbit is not busy. */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_is_busy() noexcept
{
    return rubiks::app::is_busy() ? 1 : 0;
}

/**
 * Releases the engine. Safe to call regardless of the initialization state.
 */
EMSCRIPTEN_KEEPALIVE void thorvg_rubiks_shutdown() noexcept
{
    rubiks::app::shutdown();
}

}  // extern "C"
