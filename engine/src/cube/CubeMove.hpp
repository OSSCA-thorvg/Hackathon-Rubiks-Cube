#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace rubiks::cube {

enum class Axis { X, Y, Z };

inline constexpr std::size_t kAxisCount = 3;

/** The three axes, in the order the enum declares them. */
[[nodiscard]] constexpr std::array<Axis, kAxisCount> axes() noexcept
{
    return {Axis::X, Axis::Y, Axis::Z};
}

/**
 * The two axes that are not this one, in the order the enum declares them.
 *
 * The order is relied on. A face's stickers are walked across the first of
 * the two and down the second, which is the order a painting is indexed in,
 * and the solver builds its tools in the order this gives.
 */
[[nodiscard]] constexpr std::array<Axis, 2> other_axes(Axis axis) noexcept
{
    switch (axis) {
        case Axis::X:
            return {Axis::Y, Axis::Z};
        case Axis::Y:
            return {Axis::X, Axis::Z};
        case Axis::Z:
            break;
    }
    return {Axis::X, Axis::Y};
}

[[nodiscard]] constexpr std::size_t axis_index(Axis axis) noexcept
{
    return static_cast<std::size_t>(axis);
}

/** Bitmask over layer indices 0 ... N-1 along a move's axis. */
using LayerMask = std::uint32_t;

/**
 * How many layers a mask has a bit for.
 *
 * A cube has to be narrower than this to be turned at all. Whether a mask
 * stays inside a cube is asked by shifting it down by the cube's size, and a
 * shift by the whole width of the type is not defined, so the widest cube the
 * domain turns is one layer short of the mask and every function that is
 * handed a size refuses one this wide.
 */
inline constexpr int kLayerMaskBits = std::numeric_limits<LayerMask>::digits;

/** The smallest cube there is: a single layer has nothing to turn against. */
inline constexpr int kMinCubeSize = 2;

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

/** Whether a mask selects the layer at `index`. */
[[nodiscard]] constexpr bool in_layers(LayerMask layers, int index) noexcept
{
    return (layers & layer(index)) != 0;
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
    if (layers == 0 || size <= 0 || size >= kLayerMaskBits) return false;
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

[[nodiscard]] constexpr CubeMove inverse(const CubeMove& move) noexcept
{
    return CubeMove{move.axis, move.layers, -move.quarter_turns};
}

/** Turn count in 0 ... 3; a move of any integer size is well defined. */
[[nodiscard]] constexpr int normalized_turns(int quarter_turns) noexcept
{
    return ((quarter_turns % 4) + 4) % 4;
}

}  // namespace rubiks::cube
