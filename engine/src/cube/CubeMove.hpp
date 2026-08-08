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
