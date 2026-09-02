#pragma once

#include <optional>

#include "cube/CubeState.hpp"
#include "cube/Cubie.hpp"
#include "graphics/ActiveRotation.hpp"
#include "graphics/Color.hpp"
#include "graphics/Palette.hpp"
#include "graphics/Scene.hpp"
#include "math/Quaternion.hpp"
#include "math/Types.hpp"

namespace rubiks::graphics {

/** Half the edge length of the whole cube, whatever N is. */
inline constexpr float kCubeHalfExtent = 1.0f;

/**
 * How much of its grid cell a sticker fills.
 *
 * The remainder is the seam. Because only surface stickers are emitted and
 * cubies have no body faces, a seam always shows the background color, which
 * is what makes seam pixels a decisive check rather than an approximate one.
 */
inline constexpr float kStickerScale = 0.92f;

/** Distance from a cubie's center to its sticker plane. */
[[nodiscard]] float sticker_half_extent(int size) noexcept;

/** Center of the cubie at index `i` along one axis, for an N of `size`. */
[[nodiscard]] float cubie_center(int i, int size) noexcept;

/**
 * Appends one sticker quad, wound counter-clockwise as seen from outside.
 *
 * Back-face culling depends on that winding, so this function is the single
 * place the six corner orders are written down.
 */
void append_sticker(WorldScene& scene, const math::Vec3& center,
                    float half_extent, cube::Face face, const Color& color);

/**
 * Color of a cut surface exposed while a layer is turning.
 *
 * A cubie has no body faces at rest, so without this the inside of the cube
 * would show through as background during a turn. Kept clearly apart from the
 * background and from all six sticker colors so a test can tell them apart.
 */
inline constexpr Color kBodyColor{70, 74, 82, 255};

/**
 * Color of the cubie body where it shows between the stickers.
 *
 * The renderer paints each slab's silhouette in this under the stickers, so
 * a seam shows dark plastic rather than whatever is behind the cube. Darker
 * than kBodyColor as a groove in shadow is -- and deliberately far from it:
 * a lit cut face is kBodyColor at some brightness, and the anti-aliased edge
 * of a seam against a sticker must not be mistakable for one. Checked against
 * every sticker colour and both grounds: no blend of this with any of them
 * comes within a unit of kBodyColor at any brightness a cut can have.
 */
inline constexpr Color kSeamColor{34, 36, 40, 255};

/**
 * Rotation of a turning layer, in the domain's sign convention.
 *
 * CubeMove counts a positive turn clockwise seen from the positive end of the
 * axis, which is a negative right-handed rotation. That conversion happens
 * here and nowhere else, so picking, drag resolution and rendering cannot
 * disagree about which way a positive angle turns.
 */
[[nodiscard]] math::Quaternion layer_rotation(cube::Axis axis,
                                              float degrees) noexcept;

/**
 * Builds the world-space scene for a cube state at rest.
 *
 * Only stickers on the outside of the cube are emitted: a cubie's face is
 * visible exactly when that cubie sits in the outermost layer along the
 * face's axis. At N = 3 that is 54 quads, of which the fixed camera keeps 27.
 *
 * Beside them goes one shadow caster, the box of the whole cube, for the
 * shadow pass to project; it is not drawn.
 */
[[nodiscard]] WorldScene build_cube_scene(const cube::CubeState& state,
                                          Palette palette = Palette::Classic);

/**
 * Builds the scene for a cube state with a layer turn in progress.
 *
 * Without a turn this calls the function above verbatim, so the resting
 * output stays the one the rendered scene contract was derived from rather
 * than whatever a zero-angle rotation happens to produce.
 *
 * With a turn, the stickers of the selected layers rotate about the cube
 * center, and the cut surfaces revealed at the boundary between turning and
 * still layers are filled with kBodyColor.
 *
 * The casters are cut along the turning axis into one box per unbroken run of
 * layers, the turning runs rotated with their stickers. Their union is the
 * cube at this angle, which is what makes the shadow of the union theirs; at
 * an angle of zero it is the same solid as the resting cube in more pieces.
 */
[[nodiscard]] WorldScene build_cube_scene(
    const cube::CubeState& state,
    const std::optional<ActiveRotation>& active,
    Palette palette = Palette::Classic);

}  // namespace rubiks::graphics
