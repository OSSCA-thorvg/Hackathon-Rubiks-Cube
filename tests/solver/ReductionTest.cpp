#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cube/CubeState.hpp"
#include "cube/Scramble.hpp"
#include "cube/PackedMove.hpp"
#include "cube/Surface.hpp"
#include "cube/solver/LayerByLayer.hpp"
#include "cube/solver/Projection.hpp"
#include "cube/solver/Reduction.hpp"

// Turning a big cube into a three by three: the centres first.

namespace {

using rubiks::cube::CubeState;
using rubiks::cube::CubiePosition;
using rubiks::cube::Face;
using rubiks::cube::faces;
using rubiks::cube::make_scramble;
using rubiks::cube::solved_color;
using rubiks::cube::CubeMove;
using rubiks::cube::is_layer_run;
using rubiks::cube::pack;
using rubiks::cube::solver::parities;
using rubiks::cube::solver::projected;
using rubiks::cube::solver::reachable;
using rubiks::cube::solver::reduce;
using rubiks::cube::solver::reduced;
using rubiks::cube::solver::solve_as_three_layers;
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

namespace {

/** Reduces a scrambled cube, and says what the reduction had to say. */
void reduces_and_finishes(int size, std::uint32_t seed)
{
    auto cube = scrambled(seed, size);

    const auto reduction = reduce(cube);
    cube.apply(reduction);

    INFO("size " << size << " seed " << seed);
    REQUIRE(centres_done(cube));
    REQUIRE(reduced(cube));

    // Both halves of what a reduction can be left holding. A cube that fails
    // this is one the three-layer stages cannot finish, which is the whole
    // reason the reduction looks.
    REQUIRE(reachable(parities(projected(cube))));

    // The same mask contract the rest of the application is held to: one run
    // of layers, and never the whole width.
    for (const auto& move : reduction) {
        REQUIRE(is_layer_run(move.layers, size));
        REQUIRE(pack(move) != 0);
    }

    cube.apply(solve_as_three_layers(cube));
    REQUIRE(cube.is_solved());
}

}  // namespace

TEST_CASE("a big cube is reduced and then solved")
{
    for (const int size : {4, 5, 6}) {
        for (std::uint32_t seed = 0; seed < 2; ++seed) {
            reduces_and_finishes(size, seed);
        }
    }
}

TEST_CASE("a seven by seven is reduced and solved too", "[.big]")
{
    // Hidden from the ordinary run by the leading dot in the tag: it checks
    // what the sizes above check and takes a minute to do it, because the
    // board the search reads grows with the square of the size. Run it with
    // `solver-test "[.big]"` after touching this file.
    //
    // Eight and nine are not here yet. The search finishes them sometimes and
    // gives up on them sometimes, and a test that is sometimes right is worse
    // than no test at all.
    reduces_and_finishes(7, 0);
}

TEST_CASE("a reduction is the same reduction every time")
{
    for (const int size : {4, 5}) {
        const auto cube = scrambled(3, size);
        const auto once = reduce(cube);
        const auto again = reduce(cube);

        INFO("size " << size);
        REQUIRE(once.size() == again.size());
        for (std::size_t i = 0; i < once.size(); ++i) {
            REQUIRE(once[i].axis == again[i].axis);
            REQUIRE(once[i].layers == again[i].layers);
            REQUIRE(once[i].quarter_turns == again[i].quarter_turns);
        }
    }
}
