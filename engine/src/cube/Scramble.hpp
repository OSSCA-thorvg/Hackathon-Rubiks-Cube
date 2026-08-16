#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "cube/CubeMove.hpp"

namespace rubiks::cube {

/** Number of moves in a normal 3x3 scramble. */
inline constexpr std::size_t kScrambleMoveCount = 20;

/**
 * Builds a reproducible scramble for an N x N x N cube.
 *
 * Consecutive moves never share an axis, so the sequence contains neither a
 * repeated face nor adjacent commuting turns of opposite faces. The generator
 * is owned by this repository to keep native and WebAssembly results identical
 * for the same seed.
 *
 * Each move takes a face and a depth of 1 to N/2, and turns that many layers
 * from the face at once -- the way a big cube is scrambled everywhere, and the
 * only way the layers inside get scrambled at all: turning nothing but the six
 * outer faces of a 5x5 leaves its inner slices exactly as they were, which is
 * not a scrambled cube. A 3x3 has one depth to choose from, so its scrambles
 * are the outer-face sequences they have always been, seed for seed.
 *
 * Empty for a size below 2, which has no move that changes anything.
 */
[[nodiscard]] std::vector<CubeMove> make_scramble(
    int size, std::uint32_t seed,
    std::size_t move_count = kScrambleMoveCount);

}  // namespace rubiks::cube
