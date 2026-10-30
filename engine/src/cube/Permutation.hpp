#pragma once

#include <cstddef>
#include <vector>

/**
 * Two questions asked of permutations of a cube's stickers, by both the
 * assembly gate and the reduction solver.
 *
 * A permutation here is a vector whose entry `i` says where item `i` goes.
 * Nothing in this header knows what the items are: the gate's are the slots
 * of a painting, the solver's the cells of one of its boards.
 */
namespace rubiks::cube {

/**
 * Which closed set each of `count` items belongs to.
 *
 * Each step is a permutation, and two items are in one set when some run of
 * steps carries one to the other. Sets are numbered from zero in the order of
 * their first item, so the numbering is a fact about the steps alone.
 */
[[nodiscard]] std::vector<int> orbits(
    const std::vector<std::vector<int>>& steps, std::size_t count);

/** Whether a permutation takes an odd number of swaps to undo. */
[[nodiscard]] bool is_odd_permutation(const std::vector<int>& permutation);

}  // namespace rubiks::cube
