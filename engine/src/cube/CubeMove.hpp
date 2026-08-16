#pragma once

#include <cstdint>
#include <vector>

namespace rubiks::cube {

enum class Axis { X, Y, Z };

/** Bitmask over layer indices 0 ... N-1 along a move's axis. */
using LayerMask = std::uint32_t;

/** The mask selecting a single layer. */
[[nodiscard]] constexpr LayerMask layer(int index) noexcept
{
    return LayerMask{1} << index;
}

/** The mask selecting every layer from `first` to `last`, both included. */
[[nodiscard]] constexpr LayerMask layers_through(int first, int last) noexcept
{
    LayerMask mask = 0;
    for (int index = first; index <= last; ++index) {
        mask |= layer(index);
    }
    return mask;
}

/**
 * Whether a mask is one unbroken run of layers, all of them inside the cube.
 *
 * The shape of every move this application can make and every move it can be
 * handed: a single layer, a wide move from a face, or a block of slices
 * between two of them. A mask with a gap in it is not one of those -- nothing
 * turns two layers with a still one between them -- and it has no notation to
 * be written in either, so the same answer serves both questions.
 *
 * A size wide enough to shift a mask off the end is refused rather than
 * wrapped: `layer()` is a shift, and this is the function that would have to
 * have thought about it.
 */
[[nodiscard]] constexpr bool is_layer_run(LayerMask layers, int size) noexcept
{
    if (layers == 0 || size <= 0 || size >= 32) return false;
    if ((layers >> size) != 0) return false;

    // Slid down until the run starts at the bottom, a run and only a run is
    // all ones -- and a value of all ones is the one that carries on adding.
    LayerMask run = layers;
    while ((run & 1U) == 0) run >>= 1U;
    return (run & (run + 1U)) == 0;
}

/**
 * A rotation of one or more layers about an axis, in quarter turns.
 *
 * This struct is the general form, which is why there is no string notation
 * parser: "R" and "3Rw" are serializations of a layer set, and naming the
 * layers directly says the same thing without a failure path. Wide moves,
 * slices and whole-cube rotations are built by widening `layers`.
 *
 * `quarter_turns` is positive for a clockwise turn seen from the positive end
 * of the axis looking back at the origin. It is taken modulo 4 when applied,
 * so any integer is valid and 0 is a no-op.
 */
struct CubeMove {
    Axis axis;
    LayerMask layers;
    int quarter_turns;
};

[[nodiscard]] constexpr bool operator==(const CubeMove& a,
                                        const CubeMove& b) noexcept
{
    return a.axis == b.axis && a.layers == b.layers &&
           a.quarter_turns == b.quarter_turns;
}

[[nodiscard]] constexpr bool operator!=(const CubeMove& a,
                                        const CubeMove& b) noexcept
{
    return !(a == b);
}

[[nodiscard]] constexpr CubeMove inverse(const CubeMove& move) noexcept
{
    return CubeMove{move.axis, move.layers, -move.quarter_turns};
}

/** The sequence that undoes `sequence`: reversed, each move inverted. */
[[nodiscard]] std::vector<CubeMove> inverse(
    const std::vector<CubeMove>& sequence);

/**
 * The six standard face turns of an N x N x N cube.
 *
 * These are constructors, not parsers, so they cannot fail. The negative
 * faces take -1 quarter turns because clockwise as seen from outside that
 * face is counter-clockwise about the positive axis direction.
 */
namespace moves {

[[nodiscard]] CubeMove R(int size) noexcept;
[[nodiscard]] CubeMove L(int size) noexcept;
[[nodiscard]] CubeMove U(int size) noexcept;
[[nodiscard]] CubeMove D(int size) noexcept;
[[nodiscard]] CubeMove F(int size) noexcept;
[[nodiscard]] CubeMove B(int size) noexcept;

}  // namespace moves

}  // namespace rubiks::cube
