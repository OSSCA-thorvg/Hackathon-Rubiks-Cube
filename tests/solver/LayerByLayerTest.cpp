#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"
#include "cube/Scramble.hpp"
#include "cube/Surface.hpp"
#include "cube/solver/LayerByLayer.hpp"

// The solver in isolation. This executable links the solver target, which
// carries the cube domain and nothing else -- so a solver that reached for
// graphics or the application would fail to build here.

namespace {

using rubiks::cube::Axis;
using rubiks::cube::CubeMove;
using rubiks::cube::CubeState;
using rubiks::cube::is_layer_run;
using rubiks::cube::layer;
using rubiks::cube::make_scramble;
using rubiks::cube::solver::compress;
using rubiks::cube::solver::LayerByLayer;
using rubiks::cube::solver::solve_as_three_layers;

constexpr int kSize = 3;

/** A cube scrambled with the application's own generator. */
CubeState scrambled(std::uint32_t seed, std::size_t moves = 20,
                    int size = kSize)
{
    CubeState cube(size);
    cube.apply(make_scramble(size, seed, moves));
    return cube;
}

/**
 * A big cube turned by its outer faces alone, which leaves it reduced.
 *
 * Every row along an edge and every centre began one colour and no move here
 * can break either up, so this is the state a reduction hands over -- and the
 * only way to reach one without a reduction to hand.
 */
CubeState outer_scrambled(std::uint32_t seed, int size,
                          std::size_t moves = 30)
{
    CubeState cube(size);

    // The generator reaches inside a big cube on purpose, so its moves are
    // filtered down here to the ones a three by three would have made.
    for (const auto& move : make_scramble(size, seed, moves)) {
        if (move.layers == layer(0) || move.layers == layer(size - 1)) {
            cube.apply(move);
        }
    }
    return cube;
}

/** Whether a move is an outer face of `size` turned on its own. */
bool is_outer_face_turn(const CubeMove& move, int size)
{
    return move.layers == layer(0) || move.layers == layer(size - 1);
}

}  // namespace

TEST_CASE("the solver says which cubes it solves")
{
    const LayerByLayer solver;

    CHECK(solver.supports(2));
    CHECK(solver.supports(3));
    for (const int size : {1, 4, 5, 7, 9}) {
        CHECK_FALSE(solver.supports(size));
    }
}

TEST_CASE("a two by two is solved by the corner stages alone")
{
    const LayerByLayer solver;

    for (std::uint32_t seed = 0; seed < 500; ++seed) {
        auto cube = scrambled(seed, 20, 2);
        const auto solution = solver.solve(cube);

        cube.apply(solution);
        INFO("seed " << seed);
        REQUIRE(cube.is_solved());
    }
}

TEST_CASE("a two by two solution is outer face turns as well")
{
    const LayerByLayer solver;

    for (std::uint32_t seed = 0; seed < 100; ++seed) {
        for (const auto& move : solver.solve(scrambled(seed, 20, 2))) {
            INFO("seed " << seed);
            REQUIRE(is_outer_face_turn(move, 2));
            REQUIRE(is_layer_run(move.layers, 2));
        }
    }
}

TEST_CASE("the three-layer stages finish a reduced cube of any size")
{
    // What a reduction will hand over, without a reduction yet to hand it.
    // The stages read pieces as faces and colours rather than as coordinates,
    // so a nine by nine whose rows are already paired is a three by three to
    // them.
    for (const int size : {4, 5, 6, 7, 8, 9}) {
        for (std::uint32_t seed = 0; seed < 40; ++seed) {
            auto cube = outer_scrambled(seed, size);
            cube.apply(solve_as_three_layers(cube));
            INFO("size " << size << " seed " << seed);
            REQUIRE(cube.is_solved());
        }
    }
}

TEST_CASE("a solved cube needs no moves")
{
    const LayerByLayer solver;
    CHECK(solver.solve(CubeState(kSize)).empty());
}

TEST_CASE("every scramble the application can make is solved")
{
    const LayerByLayer solver;

    for (std::uint32_t seed = 0; seed < 500; ++seed) {
        auto cube = scrambled(seed);
        const auto solution = solver.solve(cube);

        cube.apply(solution);
        INFO("seed " << seed);
        REQUIRE(cube.is_solved());
    }
}

TEST_CASE("longer and shorter scrambles are solved as well")
{
    const LayerByLayer solver;

    for (const std::size_t length : {1, 2, 3, 7, 40, 100}) {
        for (std::uint32_t seed = 0; seed < 60; ++seed) {
            auto cube = scrambled(seed, length);
            cube.apply(solver.solve(cube));
            INFO("seed " << seed << " length " << length);
            REQUIRE(cube.is_solved());
        }
    }
}

TEST_CASE("a solution is made of outer face turns and nothing else")
{
    const LayerByLayer solver;

    for (std::uint32_t seed = 0; seed < 200; ++seed) {
        const auto cube = scrambled(seed);
        for (const auto& move : solver.solve(cube)) {
            INFO("seed " << seed);

            // What the record, the notation and a shared link all require.
            REQUIRE(is_layer_run(move.layers, kSize));
            REQUIRE(is_outer_face_turn(move, kSize));
            REQUIRE(move.quarter_turns != 0);
            REQUIRE(move.quarter_turns >= -1);
            REQUIRE(move.quarter_turns <= 2);
        }
    }
}

TEST_CASE("the same cube is always solved the same way")
{
    const LayerByLayer solver;

    for (std::uint32_t seed = 0; seed < 50; ++seed) {
        const auto cube = scrambled(seed);
        const auto first = solver.solve(cube);
        const auto second = solver.solve(cube);
        REQUIRE(first == second);
    }
}

TEST_CASE("solutions stay within a length worth watching")
{
    const LayerByLayer solver;

    std::size_t longest = 0;
    std::size_t total = 0;
    constexpr std::uint32_t kSeeds = 500;

    for (std::uint32_t seed = 0; seed < kSeeds; ++seed) {
        const auto length = solver.solve(scrambled(seed)).size();
        longest = length > longest ? length : longest;
        total += length;
    }

    // A regression bound rather than a promise about the method: these are the
    // numbers this implementation gives today, and a change that doubles them
    // is a change somebody should have meant.
    CHECK(longest <= 150);
    CHECK(total / kSeeds <= 110);
}

TEST_CASE("neighbouring turns of one layer are run together")
{
    const auto axis = Axis::X;
    const auto layers = layer(2);

    SECTION("two quarters become a half")
    {
        const std::vector<CubeMove> moves{{axis, layers, 1}, {axis, layers, 1}};
        const auto joined = compress(moves);
        REQUIRE(joined.size() == 1);
        CHECK(joined[0].quarter_turns == 2);
    }

    SECTION("opposite quarters cancel")
    {
        CHECK(compress({{axis, layers, 1}, {axis, layers, -1}}).empty());
    }

    SECTION("cancelling brings the two either side together")
    {
        const std::vector<CubeMove> moves{{axis, layers, 1},
                                          {Axis::Y, layer(0), 2},
                                          {Axis::Y, layer(0), 2},
                                          {axis, layers, 1}};
        const auto joined = compress(moves);
        REQUIRE(joined.size() == 1);
        CHECK(joined[0].axis == axis);
        CHECK(joined[0].quarter_turns == 2);
    }

    SECTION("three quarters are written as one the other way")
    {
        const std::vector<CubeMove> moves{{axis, layers, 2}, {axis, layers, 1}};
        const auto joined = compress(moves);
        REQUIRE(joined.size() == 1);
        CHECK(joined[0].quarter_turns == -1);
    }

    SECTION("different layers of one axis are left alone")
    {
        const std::vector<CubeMove> moves{{axis, layer(0), 1},
                                          {axis, layer(2), 1}};
        CHECK(compress(moves).size() == 2);
    }

    SECTION("compressing does not change what a sequence does")
    {
        const LayerByLayer solver;
        for (std::uint32_t seed = 0; seed < 50; ++seed) {
            auto cube = scrambled(seed);
            const auto solution = solver.solve(cube);
            auto again = cube;

            cube.apply(solution);
            again.apply(compress(solution));
            REQUIRE(cube == again);
        }
    }
}
