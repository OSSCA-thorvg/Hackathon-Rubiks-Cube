#pragma once

#include <vector>

#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"
#include "cube/Cubie.hpp"
#include "cube/Surface.hpp"

/**
 * What the tests of every layer share, at the cube's own level.
 *
 * The tests keep the walls the engine has. A helper lives in the directory of
 * the lowest layer that can see everything it uses, and a test executable is
 * given only the directories its own library may see -- so a cube test that
 * reaches for a graphics helper fails to find the header, the same way cube
 * itself fails to find graphics.
 *
 * This one includes nothing but cube/ and the standard library, which is what
 * lets tests/cube and tests/solver use it as readily as the layers above them.
 *
 * No Catch2 in here: the benches link without it, and a helper that asserts
 * belongs beside the tests that assert.
 */
namespace rubiks::test {

/**
 * The six face turns of an N x N x N cube, by the letters they are written
 * with.
 *
 * Shorthand for a test that needs a move to turn a cube with and is not
 * asking how one is made: each is the engine's own `cube::face_move()` for
 * depth one of that face, turned once clockwise as seen from outside it. A
 * test of that conversion writes the axis, the layer and the sign out itself
 * rather than comparing the function with these.
 */
namespace moves {

[[nodiscard]] inline cube::CubeMove R(int size) noexcept
{
    return cube::face_move(cube::Face::Right, 1, 1, 1, size);
}

[[nodiscard]] inline cube::CubeMove L(int size) noexcept
{
    return cube::face_move(cube::Face::Left, 1, 1, 1, size);
}

[[nodiscard]] inline cube::CubeMove U(int size) noexcept
{
    return cube::face_move(cube::Face::Up, 1, 1, 1, size);
}

[[nodiscard]] inline cube::CubeMove D(int size) noexcept
{
    return cube::face_move(cube::Face::Down, 1, 1, 1, size);
}

[[nodiscard]] inline cube::CubeMove F(int size) noexcept
{
    return cube::face_move(cube::Face::Front, 1, 1, 1, size);
}

[[nodiscard]] inline cube::CubeMove B(int size) noexcept
{
    return cube::face_move(cube::Face::Back, 1, 1, 1, size);
}

}  // namespace moves

/** Every move of `sequence` applied to `cube`, in the order written. */
inline void apply_all(cube::CubeState& cube,
                      const std::vector<cube::CubeMove>& sequence) noexcept
{
    for (const auto& move : sequence) {
        cube.apply(move);
    }
}

/**
 * The sequence that undoes `sequence`: reversed, each move inverted.
 *
 * Written here rather than asked of the engine, so that what a rewind plan is
 * compared with was built by something other than the code that plans it.
 */
[[nodiscard]] inline std::vector<cube::CubeMove> inverse_of(
    const std::vector<cube::CubeMove>& sequence)
{
    std::vector<cube::CubeMove> reversed;
    reversed.reserve(sequence.size());

    for (auto move = sequence.rbegin(); move != sequence.rend(); ++move) {
        reversed.push_back(cube::inverse(*move));
    }
    return reversed;
}

}  // namespace rubiks::test
