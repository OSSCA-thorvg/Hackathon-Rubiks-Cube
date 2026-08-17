#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cube/CubeState.hpp"
#include "cube/Scramble.hpp"
#include "cube/Surface.hpp"
#include "cube/solver/Reduction.hpp"

// Turning a big cube into a three by three: the centres first.

namespace {

using rubiks::cube::CubeState;
using rubiks::cube::CubiePosition;
using rubiks::cube::Face;
using rubiks::cube::faces;
using rubiks::cube::make_scramble;
using rubiks::cube::solved_color;
using rubiks::cube::solver::solve_centres;

CubeState scrambled(std::uint32_t seed, int size, std::size_t moves = 40)
{
    CubeState cube(size);
    cube.apply(make_scramble(size, seed, moves));
    return cube;
}

/** Whether every face's centre block is one colour, its own. */
bool centres_done(const CubeState& cube)
{
    const int size = cube.size();
    for (int x = 0; x < size; ++x) {
        for (int y = 0; y < size; ++y) {
            for (int z = 0; z < size; ++z) {
                int showing = 0;
                Face only = Face::Up;
                for (const Face face : faces()) {
                    if (rubiks::cube::coordinate_on(rubiks::cube::axis_of(face),
                                                    CubiePosition{x, y, z}) ==
                        rubiks::cube::outer_layer(face, size)) {
                        ++showing;
                        only = face;
                    }
                }
                if (showing != 1) continue;
                if (cube.at(x, y, z).sticker(only) != solved_color(only)) {
                    return false;
                }
            }
        }
    }
    return true;
}

}  // namespace

TEST_CASE("centres are brought together at every size")
{
    for (const int size : {4, 5, 6, 7, 8, 9}) {
        for (std::uint32_t seed = 0; seed < 20; ++seed) {
            auto cube = scrambled(seed, size);
            cube.apply(solve_centres(cube));
            INFO("size " << size << " seed " << seed);
            REQUIRE(centres_done(cube));
        }
    }
}
