#pragma once

#include <array>
#include <cstddef>

#include "cube/CubeState.hpp"

/**
 * Reading a big cube as the three by three it has been turned into.
 *
 * A reduction ends when every centre block is one colour and every row along
 * an edge is one pair of colours. From then on the cube answers to outer face
 * turns exactly as a three by three does, and the stages that solve one solve
 * it -- which is the whole reason a big cube is reduced rather than solved
 * where it stands.
 *
 * "Exactly as a three by three" is not quite the whole truth on an even cube,
 * and this file is where the difference is read off. Four, six and eight can
 * finish a reduction in a state no three by three can be in: a pair turned
 * round, or two pairs swapped. Neither is a failure of the reduction -- both
 * are ordinary positions of the bigger cube -- but the stages that follow have
 * no move that reaches them, so they have to be seen and undone first.
 */
namespace rubiks::cube::solver {

inline constexpr std::size_t kCornerSlots = 8;
inline constexpr std::size_t kEdgeSlots = 12;

/**
 * Whether the cube is reduced: centres one colour, edge rows one pair.
 *
 * A row is asked only to agree with itself, not to be right. A row of the two
 * colours it should be wearing the other way round is still reduced -- the
 * pieces are together, which is what reduction means -- and it is exactly that
 * state the parities below are for.
 *
 * True for every two by two and for a three by three whose centres are home:
 * neither has a row longer than one piece, so neither has anything to pair.
 */
[[nodiscard]] bool reduced(const CubeState& cube);

/**
 * A reduced cube as the twenty pieces of a three by three.
 *
 * Slots are numbered by this file and pieces by the slot they belong in, so
 * `corners[i] == i` and `edges[i] == i` with nothing turned round is a solved
 * cube -- and two cubes of different sizes turned by the same outer faces
 * project to the same thing.
 *
 * Not a `CubeState`: that class deliberately offers no way to build a position
 * other than by turning one, which is what keeps unreachable cubes out of the
 * domain. This is a reading of a cube rather than a cube.
 */
struct Projection {
    /** Which corner piece sits in each corner slot. */
    std::array<int, kCornerSlots> corners{};

    /** Which edge piece sits in each edge slot. */
    std::array<int, kEdgeSlots> edges{};

    /** Whether the edge in each slot wears its two colours the other way. */
    std::array<int, kEdgeSlots> turned_round{};
};

/**
 * The projection of a reduced cube of three or more.
 *
 * A two by two has no edges and no centres, so there is nothing to project and
 * nothing that could go wrong with it; asking here is a precondition
 * violation, as is asking about a cube that is not reduced.
 */
[[nodiscard]] Projection projected(const CubeState& cube);

/**
 * The two positions a reduced even cube can hold that a three by three cannot.
 *
 * Read as sums rather than looked up in a table of pictures. Every face turn
 * turns four edges round at once and moves four corners and four edges, so
 * both of these are unchanged by every turn a three by three has -- which is
 * what makes them the right question to ask, and what makes a table of the
 * positions they arise in a second place for the same fact to be wrong.
 */
struct Parities {
    /** One pair is wearing its colours the other way round. */
    bool pair_turned_round = false;

    /** Two pairs have changed places with nothing else moving. */
    bool pairs_swapped = false;
};

[[nodiscard]] Parities parities(const Projection& projection);

/** Whether the stages that solve a three by three can finish this one. */
[[nodiscard]] inline bool reachable(const Parities& parities) noexcept
{
    return !parities.pair_turned_round && !parities.pairs_swapped;
}

}  // namespace rubiks::cube::solver
