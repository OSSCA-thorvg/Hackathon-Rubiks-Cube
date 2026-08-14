#pragma once

#include <cstdint>

#include "cube/Cubie.hpp"
#include "graphics/Layout.hpp"

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
 * @return true when a gesture began. False only when none could: before
 *         initialization, for non-finite coordinates, while another gesture is
 *         already running, and in net-only view for a press off the net, where
 *         there is no viewpoint on screen to sweep instead.
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

/** Replaces the cube with the deterministic scramble for `seed`. */
[[nodiscard]] bool scramble(std::uint32_t seed) noexcept;

/** Restores the logical cube and clears the current solve session. */
void reset_cube() noexcept;

/** Returns whether the committed logical cube is solved. */
[[nodiscard]] bool is_solved() noexcept;

/** Returns the number of user moves committed since the latest scramble. */
[[nodiscard]] std::uint32_t committed_move_count() noexcept;

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

/** Restores the home camera without changing cube or view mode. */
void reset_view() noexcept;

/**
 * Returns whether a gesture, animation, or unconsumed commit is active.
 *
 * An orbit sweep is not busy; the viewpoint can move while the cube cannot.
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
