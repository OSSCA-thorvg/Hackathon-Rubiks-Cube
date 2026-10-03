#pragma once

#include <vector>

#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"

/**
 * A reduction, stopped where one of its stages ends.
 *
 * Internal to this target, like `Turning.hpp`. The application asks a
 * `Reduction` for the whole answer and for nothing else, and the whole answer
 * runs straight through to a solved cube. These are the door the tests and
 * the bench come in by instead: each stops at a stage's own boundary, so the
 * cube can be looked at with its centres gathered or just reduced, and each
 * stage can be timed apart from the rest.
 */
namespace rubiks::cube::solver {

/** The moves that make every face's centre block one colour. */
[[nodiscard]] std::vector<CubeMove> solve_centres(const CubeState& state);

/**
 * The moves that turn a big cube into a three by three.
 *
 * What comes back is always reduced, and always a cube the three-layer stages
 * can finish.
 */
[[nodiscard]] std::vector<CubeMove> reduce(const CubeState& state);

}  // namespace rubiks::cube::solver
