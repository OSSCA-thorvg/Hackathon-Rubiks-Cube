#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cube/Permutation.hpp"

// The two permutation questions the assembly gate and the reduction solver
// share, answered by hand for permutations small enough to check on paper.

namespace {

using rubiks::cube::is_odd_permutation;
using rubiks::cube::orbits;

}  // namespace

TEST_CASE("a permutation is odd when it has an odd number of even cycles")
{
    REQUIRE_FALSE(is_odd_permutation({}));
    REQUIRE_FALSE(is_odd_permutation({0, 1, 2}));
    REQUIRE(is_odd_permutation({1, 0, 2}));
    REQUIRE_FALSE(is_odd_permutation({1, 2, 0}));
    REQUIRE_FALSE(is_odd_permutation({1, 0, 3, 2}));
    REQUIRE(is_odd_permutation({1, 2, 3, 0}));
}

TEST_CASE("items reached from one another by the steps are one orbit")
{
    // Two steps: one swaps 0 and 1, the other cycles 2, 3 and 4. Item 5 is
    // fixed by both.
    const std::vector<std::vector<int>> steps{{1, 0, 2, 3, 4, 5},
                                              {0, 1, 3, 4, 2, 5}};
    REQUIRE(orbits(steps, 6) == std::vector<int>{0, 0, 1, 1, 1, 2});

    // With no steps every item is a set of its own.
    REQUIRE(orbits({}, 3) == std::vector<int>{0, 1, 2});
}
