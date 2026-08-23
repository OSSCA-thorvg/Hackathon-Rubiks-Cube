#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cube/CubeState.hpp"
#include "cube/PackedMove.hpp"
#include "cube/Scramble.hpp"
#include "cube/solver/LayerByLayer.hpp"
#include "cube/solver/Reduction.hpp"

/**
 * What a reduction costs, per stage and per size.
 *
 * Hidden from the ordinary run by the leading dot in the tag, and it asserts
 * almost nothing: what it produces is a table, and the table is the evidence
 * that a change made things faster. Run it with
 * `solver-test "[.bench]"` and paste the output into
 * `docs/tasks/16.6-clusterwise-reduction.md`.
 *
 * Three things this is careful about, because a benchmark that is not is worse
 * than none at all.
 *
 * **Preparation is measured and subtracted.** `solve_centres` and `reduce`
 * build their workshop on every call, but the application does not -- a
 * `Reduction` holds one per size and pays for it once. So each stage is also
 * run on a solved cube, where the workshop is built and no round is ever
 * entered, and that time is the preparation. What is reported for a stage is
 * the search alone, and preparation is reported beside it rather than hidden
 * inside it.
 *
 * **The whole-solve number is the warm one.** `Reduction::solve` is what the
 * application calls, so it is timed through a solver that has already been
 * asked about the size. That is the number a person waits through.
 *
 * **Several seeds, and the middle one.** A single scramble can walk into a
 * stall that costs more than the rest of the stage together. The median says
 * what a size usually costs and the maximum says what it can cost, and both
 * are printed because a solver that is usually quick is still a solver that
 * sometimes is not.
 */

namespace {

using rubiks::cube::CubeState;
using rubiks::cube::kPackedMaxLayers;
using rubiks::cube::make_scramble;
using rubiks::cube::solver::reduce;
using rubiks::cube::solver::Reduction;
using rubiks::cube::solver::solve_as_three_layers;
using rubiks::cube::solver::solve_centres;

using Clock = std::chrono::steady_clock;

/** The seeds every size is measured at, fixed so two runs can be compared. */
constexpr std::uint32_t kSeeds[] = {0, 1, 7, 13, 42};

/** How long a scramble is. Forty is what the rest of the tests use. */
constexpr std::size_t kScrambleMoves = 40;

CubeState scrambled(std::uint32_t seed, int size)
{
    CubeState cube(size);
    cube.apply(make_scramble(size, seed, kScrambleMoves));
    return cube;
}

/** Seconds taken by `work`, which is called for its time rather than its value. */
template <typename Work>
double seconds(Work&& work)
{
    const auto started = Clock::now();
    work();
    const std::chrono::duration<double> took = Clock::now() - started;
    return took.count();
}

double median_of(std::vector<double> values)
{
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

/** One size, measured. Everything printed for a size comes from here. */
struct Measured {
    int size = 0;
    std::size_t seeds = 0;

    double prepare_centres = 0.0;
    double prepare_reduce = 0.0;

    double centres = 0.0;
    double centres_worst = 0.0;
    double reduction = 0.0;
    double reduction_worst = 0.0;
    double finish = 0.0;
    double solve = 0.0;
    double solve_worst = 0.0;

    std::size_t centre_moves = 0;
    std::size_t reduction_moves = 0;
    std::size_t finish_moves = 0;
    std::size_t moves = 0;
    std::size_t moves_raw = 0;
};

Measured measure(int size, std::size_t seed_count)
{
    Measured out;
    out.size = size;
    out.seeds = seed_count;

    // Preparation, which is what building the workshop costs. A solved cube
    // enters no round, so what is left is the tables.
    const CubeState solved(size);
    out.prepare_centres = seconds([&] { (void)solve_centres(solved); });
    out.prepare_reduce = seconds([&] { (void)reduce(solved); });

    // The solver is asked once before it is timed, so that the size's
    // workshops are built and the timings below are the search alone -- which
    // is what a second and later solve in the application costs.
    Reduction solver;
    (void)solver.solve(scrambled(kSeeds[0], size));

    std::vector<double> centres;
    std::vector<double> reductions;
    std::vector<double> finishes;
    std::vector<double> solves;

    for (std::size_t i = 0; i < seed_count; ++i) {
        const auto cube = scrambled(kSeeds[i], size);

        std::vector<rubiks::cube::CubeMove> centre_moves;
        centres.push_back(seconds([&] { centre_moves = solve_centres(cube); }));

        std::vector<rubiks::cube::CubeMove> reduction_moves;
        reductions.push_back(seconds([&] { reduction_moves = reduce(cube); }));

        auto reduced_cube = cube;
        reduced_cube.apply(reduction_moves);

        std::vector<rubiks::cube::CubeMove> finish_moves;
        finishes.push_back(
            seconds([&] { finish_moves = solve_as_three_layers(reduced_cube); }));

        std::vector<rubiks::cube::CubeMove> whole;
        solves.push_back(seconds([&] { whole = solver.solve(cube); }));

        auto check = cube;
        check.apply(whole);
        REQUIRE(check.is_solved());

        out.centre_moves += centre_moves.size();
        out.reduction_moves += reduction_moves.size();
        out.finish_moves += finish_moves.size();
        out.moves += whole.size();
        out.moves_raw += reduction_moves.size() + finish_moves.size();
    }

    out.centres = median_of(centres);
    out.centres_worst = *std::max_element(centres.begin(), centres.end());
    out.reduction = median_of(reductions);
    out.reduction_worst =
        *std::max_element(reductions.begin(), reductions.end());
    out.finish = median_of(finishes);
    out.solve = median_of(solves);
    out.solve_worst = *std::max_element(solves.begin(), solves.end());

    out.centre_moves /= seed_count;
    out.reduction_moves /= seed_count;
    out.finish_moves /= seed_count;
    out.moves /= seed_count;
    out.moves_raw /= seed_count;

    // Preparation is real time somebody waits through on the first solve of a
    // size, so it is subtracted from the stages rather than ignored.
    out.centres = std::max(0.0, out.centres - out.prepare_centres);
    out.centres_worst = std::max(0.0, out.centres_worst - out.prepare_centres);
    out.reduction = std::max(0.0, out.reduction - out.prepare_reduce);
    out.reduction_worst =
        std::max(0.0, out.reduction_worst - out.prepare_reduce);

    return out;
}

void write_header()
{
    std::cout << "\n"
              << "reduction bench -- " << kScrambleMoves << "-move scrambles, "
              << "seeds {0,1,7,13,42}, median (worst), workshop warm\n"
#ifdef NDEBUG
              << "build: asserts off. "
#else
              // The repository's own native build is debugoptimized with
              // asserts left on, so this is the ordinary line rather than a
              // warning. It is printed because a table compared against one
              // taken with asserts off would be comparing two things.
              << "build: -O2, asserts on (the repository default). "
#endif
              << "times in seconds, moves averaged over the seeds.\n"
              << "  prep   = building the size's tables, paid once per size\n"
              << "  centre = solve_centres, preparation subtracted\n"
              << "  reduce = reduce (centres included), preparation subtracted\n"
              << "  edge*  = reduce - centre, derived rather than measured:\n"
              << "           a reduction gathers the centres more than once\n"
              << "  finish = solve_as_three_layers on the reduced cube\n"
              << "  solve  = Reduction::solve, warm -- what a person waits for\n"
              << "  moves  = after compress(); raw = before it\n\n"
              << " size |   prep |  centre |   edge* |  reduce | finish |"
              << "   solve |  worst |  moves |    raw\n"
              << "------+--------+---------+---------+---------+--------+"
              << "---------+--------+--------+-------\n";
}

void write_row(const Measured& m)
{
    const double edge = std::max(0.0, m.reduction - m.centres);
    // `prepare_reduce` builds both workshops, `prepare_centres` only the
    // centre one, so the pair is not a sum -- what a size costs once is the
    // larger of the two, and the smaller is what gets subtracted from the
    // centre stage below.
    std::cout << std::fixed << std::setprecision(3) << std::setw(5) << m.size
              << " | " << std::setw(6) << m.prepare_reduce
              << " | " << std::setw(7) << m.centres << " | " << std::setw(7)
              << edge << " | " << std::setw(7) << m.reduction << " | "
              << std::setw(6) << m.finish << " | " << std::setw(7) << m.solve
              << " | " << std::setw(6) << m.solve_worst << " | " << std::setw(6)
              << m.moves << " | " << std::setw(6) << m.moves_raw << "\n"
              << std::flush;
}

}  // namespace

TEST_CASE("what a reduction costs, size by size", "[.bench]")
{
    write_header();
    for (const int size : {5, 7, 9, 12, 16}) {
        write_row(measure(size, std::size(kSeeds)));
    }
    std::cout << std::endl;
}

TEST_CASE("what a reduction costs at the sizes that take minutes", "[.bench]")
{
    // One seed rather than five: at twenty-eight a single solve is minutes, so
    // five of them would be an hour for a row whose point is its order of
    // magnitude. The worst column is the same number as the median here, and
    // says so by being equal.
    write_header();
    for (const int size : {20, kPackedMaxLayers}) {
        write_row(measure(size, 1));
    }
    std::cout << std::endl;
}
