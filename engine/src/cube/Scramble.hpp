#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "cube/CubeMove.hpp"

namespace rubiks::cube {

/** Number of moves in a normal 3x3 scramble. */
inline constexpr std::size_t kScrambleMoveCount = 20;

/**
 * Builds a reproducible outer-face scramble for an N x N x N cube.
 *
 * Consecutive moves never share an axis, so the sequence contains neither a
 * repeated face nor adjacent commuting turns of opposite faces. The generator
 * is owned by this repository to keep native and WebAssembly results identical
 * for the same seed.
 */
[[nodiscard]] std::vector<CubeMove> make_scramble(
    int size, std::uint32_t seed,
    std::size_t move_count = kScrambleMoveCount);

}  // namespace rubiks::cube
