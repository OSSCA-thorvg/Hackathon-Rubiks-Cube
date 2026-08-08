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
 * @return one when the pointer grabbed the cube; otherwise zero.
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
 * Releases the engine. Safe to call regardless of the initialization state.
 */
EMSCRIPTEN_KEEPALIVE void thorvg_rubiks_shutdown() noexcept
{
    rubiks::app::shutdown();
}

}  // extern "C"
