#include <array>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"
#include "cube/Scramble.hpp"
#include "cube/solver/Projection.hpp"

// Reading a big cube as the three by three it has been reduced to, and the two
// positions an even cube can hold that no three by three can.

namespace {

using rubiks::cube::Axis;
using rubiks::cube::CubeMove;
using rubiks::cube::CubeState;
using rubiks::cube::layer;
using rubiks::cube::make_scramble;
using rubiks::cube::solver::parities;
using rubiks::cube::solver::Projection;
using rubiks::cube::solver::projected;
using rubiks::cube::solver::reachable;
using rubiks::cube::solver::reduced;

/** One outer face of a cube of any size, turned the same way at every size. */
[[nodiscard]] CubeMove outer(Axis axis, bool far_end, int turns, int size)
{
    return CubeMove{axis, far_end ? layer(size - 1) : layer(0), turns};
}

/**
 * A run of outer face turns, said in a way that fits any size.
 *
 * The application's own generator reaches inside a big cube, which is the one
 * thing that must not happen here: what is being checked is that two sizes
 * agree, and they can only agree about moves both of them have.
 */
struct Choice {
    Axis axis;
    bool far_end;
    int turns;
};

[[nodiscard]] std::vector<Choice> outer_run(std::uint32_t seed,
                                            std::size_t count)
{
    constexpr std::array<Axis, 3> kAxes{Axis::X, Axis::Y, Axis::Z};
    constexpr std::array<int, 3> kTurns{-1, 1, 2};

    std::vector<Choice> run;
    std::uint32_t state = seed == 0 ? 0x6d2b79f5U : seed;
    for (std::size_t i = 0; i < count; ++i) {
        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        run.push_back(Choice{kAxes[state % 3U], (state >> 3U) % 2U != 0U,
                             kTurns[(state >> 5U) % 3U]});
    }
    return run;
}

[[nodiscard]] CubeState turned_by(const std::vector<Choice>& run, int size)
{
    CubeState cube(size);
    for (const auto& choice : run) {
        cube.apply(outer(choice.axis, choice.far_end, choice.turns, size));
    }
    return cube;
}

[[nodiscard]] bool same(const Projection& a, const Projection& b)
{
    return a.corners == b.corners && a.edges == b.edges &&
           a.turned_round == b.turned_round;
}

}  // namespace

TEST_CASE("a solved cube is reduced and projects to itself")
{
    for (const int size : {2, 3, 4, 5, 6, 7, 8, 9}) {
        INFO("size " << size);
        REQUIRE(reduced(CubeState(size)));

        if (size < 3) continue;
        const auto projection = projected(CubeState(size));
        for (int i = 0; i < 8; ++i) CHECK(projection.corners[i] == i);
        for (int i = 0; i < 12; ++i) {
            CHECK(projection.edges[i] == i);
            CHECK(projection.turned_round[i] == 0);
        }
        CHECK(reachable(parities(projection)));
    }
}

TEST_CASE("outer faces alone leave a cube reduced and reachable")
{
    // The invariance the orientation convention is chosen for. Every face turn
    // is asked to leave both parities where they were, thousands of times over
    // at every size -- which is the only thing that makes reading them off a
    // static cube trustworthy.
    for (const int size : {3, 4, 5, 6, 7, 8, 9}) {
        for (std::uint32_t seed = 1; seed <= 20; ++seed) {
            CubeState cube(size);
            for (const auto& choice : outer_run(seed, 60)) {
                cube.apply(outer(choice.axis, choice.far_end, choice.turns,
                                 size));
                INFO("size " << size << " seed " << seed);
                REQUIRE(reduced(cube));
                REQUIRE(reachable(parities(projected(cube))));
            }
        }
    }
}

TEST_CASE("two sizes turned the same way project to the same three by three")
{
    for (std::uint32_t seed = 1; seed <= 40; ++seed) {
        const auto run = outer_run(seed, 30);
        const auto three = projected(turned_by(run, 3));

        for (const int size : {4, 5, 6, 7, 8, 9}) {
            INFO("size " << size << " seed " << seed);
            REQUIRE(same(projected(turned_by(run, size)), three));
        }
    }
}

TEST_CASE("a turn that reaches inside leaves the cube unreduced")
{
    for (const int size : {3, 4, 5, 6, 7, 8, 9}) {
        CubeState cube(size);
        cube.apply(CubeMove{Axis::X, layer(1), 1});
        INFO("size " << size);
        CHECK_FALSE(reduced(cube));
    }
}

TEST_CASE("a scramble that reaches inside is not a three by three either")
{
    for (const int size : {4, 5, 6, 7, 8, 9}) {
        CubeState cube(size);
        cube.apply(make_scramble(size, 11, 40));
        INFO("size " << size);
        CHECK_FALSE(reduced(cube));
    }
}

TEST_CASE("the two parities are read off a projection")
{
    const Projection solved = projected(CubeState(4));

    SECTION("a pair turned round")
    {
        Projection one = solved;
        one.turned_round[5] = 1;

        CHECK(parities(one).pair_turned_round);
        CHECK_FALSE(parities(one).pairs_swapped);
        CHECK_FALSE(reachable(parities(one)));
    }

    SECTION("two pairs swapped")
    {
        Projection two = solved;
        std::swap(two.edges[0], two.edges[1]);

        CHECK(parities(two).pairs_swapped);
        CHECK_FALSE(parities(two).pair_turned_round);
        CHECK_FALSE(reachable(parities(two)));
    }

    SECTION("both at once")
    {
        Projection both = solved;
        both.turned_round[2] = 1;
        std::swap(both.edges[0], both.edges[1]);

        CHECK(parities(both).pair_turned_round);
        CHECK(parities(both).pairs_swapped);
    }

    SECTION("two corners swapped with two edges is a three by three position")
    {
        // Odd against odd is even: this is a position a three by three can
        // reach, which is why the check is corners against edges rather than
        // either on its own.
        Projection legal = solved;
        std::swap(legal.corners[0], legal.corners[1]);
        std::swap(legal.edges[0], legal.edges[1]);

        CHECK(reachable(parities(legal)));
    }
}
