#pragma once

#include <vector>

#include "cube/CubeMove.hpp"
#include "cube/Cube.hpp"

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

/**
 * The moves that give each centre block its canonical `solved_color(face)`.
 * Odd cubes must already have their six fixed centres in the canonical frame.
 */
[[nodiscard]] std::vector<CubeMove> solve_centres(const Cube& state);

/**
 * The moves that turn a big cube into a three by three.
 *
 * What comes back is always reduced in the canonical colour frame, and always
 * a cube the three-layer stages can finish. Odd cubes must already have their
 * fixed centres in that frame; `Reduction::solve()` normalizes them first.
 */
[[nodiscard]] std::vector<CubeMove> reduce(const Cube& state);

}  // namespace rubiks::cube::solver
