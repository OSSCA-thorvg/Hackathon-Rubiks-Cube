#pragma once

#include <cstdint>

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
 * Converting CSS pixels and the device pixel ratio is the caller's job, the
 * same split the resize path uses.
 *
 * @return true when the pointer grabbed the cube; false for a miss, for
 *         non-finite coordinates, and before initialization.
 */
[[nodiscard]] bool pointer_down(float x, float y) noexcept;

/** Continues the active gesture. Ignored without one. */
void pointer_move(float x, float y) noexcept;

/** Releases the active gesture, which is the only path that turns the cube. */
void pointer_up() noexcept;

/**
 * Abandons the active gesture without turning the cube.
 *
 * For a cancelled pointer or lost capture: the user never let go, so nothing
 * should be committed.
 */
void pointer_cancel() noexcept;

/**
 * Advances animation by an elapsed time in milliseconds and applies a turn
 * that has finished.
 *
 * @return true while further frames still have to be drawn.
 */
[[nodiscard]] bool advance(double elapsed_ms) noexcept;

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
