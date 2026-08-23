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
using rubiks::cube::kPackedMaxLayers;
using rubiks::cube::solver::parities;
using rubiks::cube::solver::Reduction;
using rubiks::cube::solver::projected;
using rubiks::cube::solver::reachable;
using rubiks::cube::solver::reduce;
using rubiks::cube::solver::reduced;
using rubiks::cube::solver::solve_as_three_layers;
using rubiks::cube::solver::solve_centres;
using rubiks::cube::solver::CentreLocality;

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

TEST_CASE("the biggest cubes are reduced and solved too", "[.big]")
{
    // Hidden from the ordinary run by the leading dot in the tag: these check
    // what the sizes above check and take minutes to do it, because the board
    // the search reads grows with the square of the size. Run them with
    // `solver-test "[.big]"` after touching this file.
    for (const int size : {7, 8, 9, 12}) {
        reduces_and_finishes(size, 0);
    }
}

TEST_CASE("the widest cube that can be written down is solved as well",
          "[.big]")
{
    // The ceiling itself, which is the packing field rather than anything in
    // the method. Minutes on its own, and the reason it is here at all is
    // that "every size the application builds solves" is a claim about the
    // end of the range and not only about the middle of it.
    reduces_and_finishes(kPackedMaxLayers, 0);
}

TEST_CASE("every size that can be built has a reduction, and nothing wider")
{
    const Reduction solver;

    // Cheap enough to walk in full, unlike solving them. Four is where a cube
    // stops being one the three-layer stages take on their own.
    for (int size = 4; size <= kPackedMaxLayers; ++size) {
        INFO("size " << size);
        CHECK(solver.supports(size));
    }

    // Past the field a move's layers are written into, a solution could not
    // be recorded even if it were found.
    CHECK_FALSE(solver.supports(kPackedMaxLayers + 1));
    CHECK_FALSE(solver.supports(3));
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

TEST_CASE("both of the things a reduction can be left holding turn up")
{
    // The two are answered differently -- one by asking for the other target,
    // one by the odd move -- and a test that never meets them is a test of
    // neither. This one meets the first: a cube that comes out of a reduction
    // with the three by three's edges in an odd order is a cube whose corners
    // were, and it is the target that moved rather than the cube.
    int straight = 0;
    int swapped = 0;

    for (const int size : {4, 5}) {
        for (std::uint32_t seed = 0; seed < 4; ++seed) {
            auto cube = scrambled(seed, size);
            cube.apply(reduce(cube));
            REQUIRE(reduced(cube));

            const auto projection = projected(cube);
            bool moved = false;
            for (int i = 0; i < 12; ++i) {
                moved = moved || projection.edges[i] != i;
            }
            if (moved) {
                ++swapped;
            } else {
                ++straight;
            }
            REQUIRE(reachable(parities(projection)));
        }
    }

    INFO("straight " << straight << ", swapped " << swapped);
    CHECK(straight > 0);
    CHECK(swapped > 0);
}

TEST_CASE("an odd number of slices from solved is reduced all the same")
{
    // And this one meets the second. Every tool the edge stage has is an even
    // permutation of the edge pieces, and on an even cube so is every outer
    // face turn -- so a cube reached by one inner slice and any number of face
    // turns is in the half of the arrangements those tools cannot reach.
    // Without the odd move in the reduction, this stops rather than finishes.
    for (const int size : {4, 6}) {
        for (std::uint32_t seed = 0; seed < 2; ++seed) {
            CubeState cube(size);
            cube.apply(CubeMove{rubiks::cube::Axis::X, rubiks::cube::layer(1),
                                1});
            for (const auto& move : make_scramble(size, seed, 30)) {
                if (move.layers == rubiks::cube::layer(0) ||
                    move.layers == rubiks::cube::layer(size - 1)) {
                    cube.apply(move);
                }
            }

            cube.apply(reduce(cube));
            INFO("size " << size << " seed " << seed);
            REQUIRE(reduced(cube));
            REQUIRE(reachable(parities(projected(cube))));

            cube.apply(solve_as_three_layers(cube));
            REQUIRE(cube.is_solved());
        }
    }
}

TEST_CASE("the centre tools are read against the clusters they move in")
{
    // What solving one cluster at a time rests on: a tool whose cells all sit
    // in one cluster can be chosen by looking at that cluster alone. The
    // coarse family is not expected to pass -- it carries a strip from one
    // face to another on purpose -- so what is checked is the two families
    // that would do the work, and the shape of the clusters themselves.
    for (const int size : {4, 5, 6, 7, 9}) {
        const auto locality = rubiks::cube::solver::centre_locality(size);
        INFO("size " << size << ": " << locality.cells << " cells in "
                     << locality.clusters << " clusters, smallest "
                     << locality.smallest_cluster << ", largest "
                     << locality.largest_cluster << "; fine "
                     << locality.fine_local << "/" << locality.fine
                     << " local, narrow " << locality.narrow_local << "/"
                     << locality.narrow << " local, widest reach "
                     << locality.widest_fine_reach << "; local tools per "
                     << "cluster " << locality.fewest_local_tools << ".."
                     << locality.most_local_tools);

        // Every centre sticker is in exactly one cluster, and a cluster is
        // never wider than the twenty-four slots a piece can reach.
        CHECK(locality.cells == 6 * (size - 2) * (size - 2));
        CHECK(locality.clusters > 0);
        CHECK(locality.largest_cluster <= 24);
        CHECK(locality.smallest_cluster >= 1);

        CHECK(locality.fine > 0);
        CHECK(locality.narrow > 0);

        // Not every tool is local, and the counts above say by how much:
        // roughly half the fine family and a quarter of the narrow one reach
        // into two clusters or more. A commutator of a slice at depth a and a
        // slice at depth b moves cells at both (a, b) and (b, a), and those
        // are mirror positions that no turn carries into one another -- so
        // spanning is what the family does by construction, not a defect.
        //
        // What a cluster-at-a-time stage can use is the local part, and what
        // matters is that the part is not thin. So that is what is checked.
        CHECK(locality.fine_local > 0);
        CHECK(locality.narrow_local > 0);

        // Every cluster has local tools, and the poorest of them is the one
        // holding the six true centres of an odd cube -- six cells, and only
        // the middle slice reaches them. Forty-eight tools for six cells is
        // thin but not empty. Whether thin is enough is not a question this
        // can answer: it is answered by a cluster-at-a-time stage actually
        // finishing, which is the next step's to show.
        CHECK(locality.fewest_local_tools > 0);
    }
}
