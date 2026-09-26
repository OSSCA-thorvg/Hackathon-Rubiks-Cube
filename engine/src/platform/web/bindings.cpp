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
 * Resizes surface `id`, or puts it away with zero by zero.
 *
 * Surface 0 is the one initialize() made; the others start with no size.
 * A surface put away holds no buffer until it is given a size again.
 *
 * @return one when the surface has the new size afterwards; otherwise zero.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_resize_surface(
    std::uint32_t id, std::uint32_t width, std::uint32_t height) noexcept
{
    return rubiks::app::resize_surface(id, width, height) ? 1 : 0;
}

/**
 * Sets which scenes surface `id` shows, as a set of bits: cube 1, net 2,
 * rings 4, axes 8.
 *
 * @return one when accepted; zero for an unknown surface or unknown bits.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_set_surface_scenes(
    std::uint32_t id, std::uint32_t scenes) noexcept
{
    return rubiks::app::set_surface_scenes(id, scenes) ? 1 : 0;
}

/** Returns the scenes surface `id` holds; zero for an unknown surface. */
EMSCRIPTEN_KEEPALIVE std::uint32_t thorvg_rubiks_surface_scenes(
    std::uint32_t id) noexcept
{
    return rubiks::app::surface_scenes(id);
}

/**
 * Returns surface `id`'s pixel buffer address in WASM linear memory.
 *
 * Valid until that surface is next resized, or until shutdown.
 *
 * @return zero for a surface with no buffer.
 */
EMSCRIPTEN_KEEPALIVE std::uintptr_t thorvg_rubiks_surface_pixel_buffer(
    std::uint32_t id) noexcept
{
    return rubiks::app::surface_pixel_buffer(id);
}

/** Returns surface `id`'s pixel buffer length in bytes; zero without one. */
EMSCRIPTEN_KEEPALIVE std::uint32_t thorvg_rubiks_surface_pixel_byte_length(
    std::uint32_t id) noexcept
{
    return rubiks::app::surface_pixel_byte_length(id);
}

/**
 * Returns how many frames surface `id` has had drawn into it.
 *
 * render() skips a surface whose picture would not change, so a client
 * copies a surface out only when this has moved since it last did.
 */
EMSCRIPTEN_KEEPALIVE std::uint32_t thorvg_rubiks_surface_frame(
    std::uint32_t id) noexcept
{
    return rubiks::app::surface_frame(id);
}

/**
 * Begins a pointer gesture on surface `id`, in that surface's pixels.
 *
 * @return one when a gesture began; zero as thorvg_rubiks_pointer_down
 *         returns it, and for a surface with no buffer.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_pointer_down_on(std::uint32_t id,
                                                       float x,
                                                       float y) noexcept
{
    return rubiks::app::pointer_down_on(id, x, y) ? 1 : 0;
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
 * Takes the buffer a shared record is written into, and returns its address.
 *
 * The pixel buffer's arrangement, for a record instead of a frame: the engine
 * owns the memory and the caller fills it through a typed view, so a session
 * of any length crosses in one call. Words are packed as
 * thorvg_rubiks_timeline_move() hands them back, the scramble first.
 *
 * Nothing else may be called between this and thorvg_rubiks_restore_apply():
 * another call may grow the heap and leave the view pointing at nothing.
 *
 * @return zero when the count is nothing or past the engine's bound.
 */
EMSCRIPTEN_KEEPALIVE std::uintptr_t thorvg_rubiks_restore_buffer(
    std::uint32_t total_count) noexcept
{
    return rubiks::app::restore_buffer(total_count);
}

/**
 * Puts the written record on the cube, after checking all of it at once.
 *
 * Applied without animation, onto a cube of `size` -- the record and the cube
 * it belongs to arrive together, so a refusal leaves both untouched and the
 * caller starts a fresh session rather than retrying.
 *
 * @return one when the whole record was accepted and applied; otherwise zero.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_restore_apply(
    int size, std::uint32_t scramble_count, std::uint32_t user_count) noexcept
{
    return rubiks::app::restore_apply(size, scramble_count, user_count) ? 1 : 0;
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
 * Whether a solver here handles the size of cube in hand.
 *
 * The size alone; whether a solve may begin right now is is_busy(), the same
 * as for every other command.
 *
 * @return one when this cube can be solved.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_can_solve() noexcept
{
    return rubiks::app::can_solve() ? 1 : 0;
}

/**
 * Solves the cube as it stands, forwards, and plays the solution.
 *
 * @return one when a solve began; zero for a cube already solved, a size with
 *         no solver, and while something else owns the cube.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_solve() noexcept
{
    return rubiks::app::solve() ? 1 : 0;
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
 * Starts an animated turn of the layers a face names.
 *
 * Face follows cube::Face order and turns are -1, 1, or 2. Depth 1 is the
 * face itself and depths count inwards, so `(face, 1, 1)` is the face turn,
 * `(face, 1, 2)` is the wide move, and `(face, 2, 2)` is the slice behind it.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_turn_face(int face, int first_depth,
                                                 int last_depth,
                                                 int face_turns) noexcept
{
    constexpr int kFirstFace = static_cast<int>(rubiks::cube::Face::Right);
    constexpr int kLastFace = static_cast<int>(rubiks::cube::Face::Back);
    if (face < kFirstFace || face > kLastFace) return 0;

    return rubiks::app::turn_face(static_cast<rubiks::cube::Face>(face),
                                  first_depth, last_depth, face_turns)
               ? 1
               : 0;
}

/**
 * Builds a cube of a different size, starting the session over.
 *
 * How the cube is being looked at survives; the record and anything playing
 * do not.
 *
 * @return one when the size is one the engine builds; otherwise zero, and
 *         nothing has changed.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_set_cube_size(int size) noexcept
{
    return rubiks::app::set_cube_size(size) ? 1 : 0;
}

/** How many layers the cube has along an axis; zero before initialization. */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_cube_size() noexcept
{
    return rubiks::app::cube_size();
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

/**
 * Colouring a real cube onto the net.
 *
 * Ten small calls rather than one that carries a buffer, because a person
 * colours one square at a time and the browser has nothing to hand over in
 * bulk -- the draft lives in the engine and is drawn from there, so what
 * crosses this boundary is a press and a colour.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_paint_begin() noexcept
{
    return rubiks::app::begin_painting() ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE void thorvg_rubiks_paint_cancel() noexcept
{
    rubiks::app::cancel_painting();
}

EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_is_painting() noexcept
{
    return rubiks::app::is_painting() ? 1 : 0;
}

/** Chooses the colour a press lays down; invalid values are rejected. */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_set_paint_brush(int colour) noexcept
{
    constexpr int kFirst = static_cast<int>(rubiks::cube::FaceColor::Red);
    constexpr int kLast = static_cast<int>(rubiks::cube::FaceColor::Blue);
    if (colour < kFirst || colour > kLast) return 0;

    return rubiks::app::set_brush(
               static_cast<rubiks::cube::FaceColor>(colour))
               ? 1
               : 0;
}

EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_paint_brush() noexcept
{
    return static_cast<int>(rubiks::app::brush());
}

EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_paint_at(float x, float y) noexcept
{
    return rubiks::app::paint_at(x, y) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_paint_fill(float x, float y) noexcept
{
    return rubiks::app::fill_face_at(x, y) ? 1 : 0;
}

/** Whether a press covers the whole face it lands on. */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_set_paint_filling(int whole_face) noexcept
{
    return rubiks::app::set_filling(whole_face != 0) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_is_paint_filling() noexcept
{
    return rubiks::app::is_filling() ? 1 : 0;
}

/** How many of a colour the draft carries; zero for an invalid colour. */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_painted_count(int colour) noexcept
{
    constexpr int kFirst = static_cast<int>(rubiks::cube::FaceColor::Red);
    constexpr int kLast = static_cast<int>(rubiks::cube::FaceColor::Blue);
    if (colour < kFirst || colour > kLast) return 0;

    return rubiks::app::painted_count(
        static_cast<rubiks::cube::FaceColor>(colour));
}

EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_paint_apply() noexcept
{
    return rubiks::app::apply_painting() ? 1 : 0;
}

/** How many colours the session's starting colouring has; zero when none. */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_origin_painting_count() noexcept
{
    return static_cast<int>(rubiks::app::origin_painting_count());
}

/** One of those colours, or -1 past the end. */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_origin_painting_at(int index) noexcept
{
    if (index < 0) return -1;
    return rubiks::app::origin_painting_at(static_cast<std::uint32_t>(index));
}

/** Hands out the buffer a colouring from a link is written into. */
EMSCRIPTEN_KEEPALIVE std::uintptr_t thorvg_rubiks_painting_buffer(
    std::uint32_t count) noexcept
{
    return rubiks::app::painting_buffer(count);
}

/** Reads both buffers back as a painted session. */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_restore_painting(
    int size, std::uint32_t user_count) noexcept
{
    return rubiks::app::restore_painting(size, user_count) ? 1 : 0;
}

/** The last refusal as a cube::PaintFault integer; zero is None. */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_paint_fault() noexcept
{
    return static_cast<int>(rubiks::app::painting_fault());
}

EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_paint_blamed_count() noexcept
{
    return rubiks::app::painting_blamed_count();
}

/** One blamed sticker's number, or -1 past the end. */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_paint_blamed_at(int index) noexcept
{
    return rubiks::app::painting_blamed_at(index);
}

/** Chooses the ground the cube is drawn against; invalid values are rejected. */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_set_canvas_theme(int theme) noexcept
{
    constexpr int kFirst =
        static_cast<int>(rubiks::graphics::CanvasTheme::Light);
    constexpr int kLast = static_cast<int>(rubiks::graphics::CanvasTheme::Dark);
    if (theme < kFirst || theme > kLast) return 0;

    return rubiks::app::set_canvas_theme(
               static_cast<rubiks::graphics::CanvasTheme>(theme))
               ? 1
               : 0;
}

/** Returns the current graphics::CanvasTheme integer. */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_canvas_theme() noexcept
{
    return static_cast<int>(rubiks::app::canvas_theme());
}

/**
 * Sets how much faster than the written tempos every animation runs.
 *
 * Out-of-range values are clamped rather than refused -- a slider that ran
 * past its end still meant the end -- so a zero comes back only from a value
 * that is not a number or from there being no engine.
 */
/**
 * Takes the buffer a lighting setup is written into, and its address.
 *
 * The same arrangement the shared record uses: the engine owns the memory and
 * the caller writes floats into it through a view, then set_lighting() reads
 * them. The order is the one Lighting::from_values documents: ambient,
 * attenuation, saturation, then x, y, z, diffuse, specular, shininess per
 * lamp, for one to four lamps.
 *
 * @return zero for a count the engine does not accept.
 */
EMSCRIPTEN_KEEPALIVE std::uintptr_t thorvg_rubiks_lighting_buffer(
    std::uint32_t count) noexcept
{
    return rubiks::app::lighting_buffer(count);
}

/**
 * Reads the buffer back as the lighting every following frame is drawn under.
 *
 * @return one when accepted; zero for a wrong count or a value that is not a
 *         number, in which case the lighting is unchanged.
 */
EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_set_lighting(std::uint32_t count) noexcept
{
    return rubiks::app::set_lighting(count) ? 1 : 0;
}

/** How many values describe the current lighting; zero outside a lifecycle. */
EMSCRIPTEN_KEEPALIVE std::uint32_t thorvg_rubiks_lighting_count() noexcept
{
    return rubiks::app::lighting_count();
}

/**
 * Writes the current lighting into the buffer, in set_lighting()'s order, and
 * returns its address.
 *
 * @return zero for a count other than thorvg_rubiks_lighting_count().
 */
EMSCRIPTEN_KEEPALIVE std::uintptr_t thorvg_rubiks_lighting_values(
    std::uint32_t count) noexcept
{
    return rubiks::app::lighting_values(count);
}

EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_set_speed_scale(float scale) noexcept
{
    return rubiks::app::set_speed_scale(scale) ? 1 : 0;
}

/** Returns the current multiplier, or 1 outside a lifecycle. */
EMSCRIPTEN_KEEPALIVE float thorvg_rubiks_speed_scale() noexcept
{
    return rubiks::app::speed_scale();
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
