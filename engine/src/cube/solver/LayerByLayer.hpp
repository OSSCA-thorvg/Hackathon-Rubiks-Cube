#pragma once

#include <vector>

#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"
#include "cube/solver/Solver.hpp"

namespace rubiks::cube::solver {

/**
 * The method a person is taught first: one layer, then the next, then the last.
 *
 * Six stages, each of which only ever uses sequences that put back what the
 * stages before it finished. There is no table of positions anywhere in it:
 * about a dozen sequences are written down, and everything else is working out
 * where to stand before using one -- either by reading the piece being looked
 * for, or, in the three places where a person would recognise a picture
 * instead, by trying the ways round and asking the cube which came out best.
 *
 * The two layers under the last one are built together rather than one after
 * the other, a corner and its edge carried in as a pair. That is the one thing
 * here taken from how the method is done rather than how it is taught, and it
 * is worth about a fifth of the whole solution.
 *
 * Three by three only. The interface says so out loud, so a bigger cube is
 * met with "no solver for this size" rather than with a wrong answer.
 *
 * What it gives up is length: a hundred moves or so where an optimal solution
 * is twenty. Shortening that is a different algorithm behind the same
 * interface rather than a change here.
 */
class LayerByLayer final : public Solver {
public:
    [[nodiscard]] bool supports(int size) const noexcept override;

    [[nodiscard]] std::vector<CubeMove> solve(
        const CubeState& state) const override;
};

/**
 * The moves that finish a cube whose pieces are already three layers deep.
 *
 * The six stages above, without the compression at the end, for a caller
 * that has more to add to the sequence.
 *
 * A cube of any size may be handed in, as long as it is *reduced*: every row
 * of pieces along an edge already one pair of colours, every centre already
 * one colour. Under outer face turns such a cube behaves exactly as a three by
 * three does -- which is why the last stage of solving a big cube is this
 * function rather than a second copy of it -- and asking it about an
 * unreduced one is a precondition violation.
 *
 * A two by two, which has neither edges nor centres to reduce, is always in
 * that state; so is a three by three.
 */
[[nodiscard]] std::vector<CubeMove> solve_as_three_layers(
    const CubeState& state);

/**
 * The same sequence with neighbouring turns of one layer run together.
 *
 * Stages are written to be read one at a time, so where two of them meet a
 * layer is often turned twice in a row -- and sometimes turned back. Joining
 * those is a fifth of the moves for a dozen lines, and every one of them is a
 * turn the user would otherwise sit through.
 *
 * A rewind deliberately does not do this, and the reason the two differ is the
 * reason each exists: watching your own moves come back off the cube is the
 * point of a rewind, and a solution has no such claim on anybody's time.
 *
 * Public because it is a property of a sequence rather than of this solver:
 * the next implementation will want it, and a test can check it on its own.
 */
[[nodiscard]] std::vector<CubeMove> compress(
    const std::vector<CubeMove>& moves);

}  // namespace rubiks::cube::solver
