#include <cstdint>
#include <random>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cube/Assembly.hpp"
#include "cube/CubeState.hpp"
#include "cube/Scramble.hpp"
#include "cube/Surface.hpp"
#include "cube/solver/LayerByLayer.hpp"
#include "cube/solver/Reduction.hpp"

/**
 * The other direction of the gate: what it lets through, a solver can finish.
 *
 * `tests/cube` cannot ask this. That target links the domain alone, which is
 * what keeps graphics and the solver out of it, so the question "and does it
 * solve?" has to be asked from here -- where the solvers live.
 *
 * The two directions together are the whole claim. `AssemblyTest` shows that
 * every cube turning can reach is let through, so nobody's real cube is
 * refused; this shows that nothing else is, so no solver is ever handed a
 * position it will search for forever.
 */

namespace {

using rubiks::cube::assembled;
using rubiks::cube::CubeState;
using rubiks::cube::FaceColor;
using rubiks::cube::make_scramble;
using rubiks::cube::PaintFault;
using rubiks::cube::painting_of;
using rubiks::cube::read_painting;
using rubiks::cube::solver::LayerByLayer;
using rubiks::cube::solver::Reduction;
using rubiks::cube::solver::Solver;

CubeState scrambled(std::uint32_t seed, int size, std::size_t moves = 40)
{
    CubeState cube(size);
    cube.apply(make_scramble(size, seed, moves));
    return cube;
}

/**
 * A painting made by disturbing a real one, rather than by colouring at random.
 *
 * Colouring at random is no way to find paintings the gate accepts: the odds
 * of six colours falling into pieces that exist at all are near enough zero
 * that a run of millions would turn up none. So a cube that *is* one gets a
 * few stickers moved about instead, which lands near the boundary rather than
 * far outside it -- some of these are cubes and some are not, which is exactly
 * the mixture worth testing.
 */
std::vector<FaceColor> disturbed(const CubeState& cube, std::uint32_t seed,
                                 int swaps)
{
    auto painting = painting_of(cube);
    std::mt19937 noise(seed);
    std::uniform_int_distribution<std::size_t> pick(0, painting.size() - 1);
    for (int i = 0; i < swaps; ++i) {
        std::swap(painting[pick(noise)], painting[pick(noise)]);
    }
    return painting;
}

[[nodiscard]] const Solver& solver_for(int size, const LayerByLayer& small,
                                       const Reduction& big)
{
    return size <= 3 ? static_cast<const Solver&>(small)
                     : static_cast<const Solver&>(big);
}

}  // namespace

TEST_CASE("a painting the gate accepts is a painting a solver finishes")
{
    const LayerByLayer small;
    const Reduction big;

    int accepted = 0;
    int refused = 0;

    for (const int size : {2, 3, 4, 5}) {
        for (std::uint32_t seed = 0; seed < 40; ++seed) {
            // A mixture: some untouched cubes, and some with stickers moved
            // about until they may well not be cubes any more.
            const auto cube = scrambled(seed, size, 20);
            const int swaps = static_cast<int>(seed % 4);
            const auto painting =
                swaps == 0 ? painting_of(cube) : disturbed(cube, seed, swaps);

            const auto reading = read_painting(size, painting);
            const auto built = assembled(size, painting);
            REQUIRE(built.has_value() == (reading.fault == PaintFault::None));

            if (!built) {
                ++refused;
                continue;
            }
            ++accepted;

            INFO("size " << size << " seed " << seed << " swaps " << swaps);
            CHECK(painting_of(*built) == painting);

            auto working = *built;
            working.apply(solver_for(size, small, big).solve(working));
            CHECK(working.is_solved());
        }
    }

    // Both outcomes have to turn up, or this test is only checking one of
    // them. A run where nothing was refused would mean the disturbing was too
    // gentle; a run where nothing was accepted would mean it was too rough.
    CHECK(accepted > 0);
    CHECK(refused > 0);
}

TEST_CASE("every size the application builds is read and solved", "[.big]")
{
    // The whole range, painted from a real cube and read straight back. Hidden
    // because solving the large sizes is minutes rather than seconds.
    const Reduction big;
    for (const int size : {6, 7, 9, 12}) {
        const auto cube = scrambled(1, size);
        const auto built = assembled(size, painting_of(cube));

        INFO("size " << size);
        REQUIRE(built.has_value());

        // Paintings and not cubes: a cubie's hidden stickers are nothing a
        // painting could have mentioned, so the two are the same cube to look
        // at without being the same `CubeState`.
        REQUIRE(painting_of(*built) == painting_of(cube));

        auto working = *built;
        working.apply(big.solve(working));
        CHECK(working.is_solved());
    }
}
