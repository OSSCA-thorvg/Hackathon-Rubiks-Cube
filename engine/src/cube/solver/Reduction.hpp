#pragma once

#include <memory>
#include <vector>

#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"
#include "cube/solver/Solver.hpp"

/**
 * Turning a big cube into a three by three, which is how a big cube is solved.
 *
 * A four by four and up have pieces a three by three does not: a block of
 * centres on each face instead of one fixed piece, and a row of pieces along
 * each edge instead of one. Reduction makes each of those blocks one colour
 * and each of those rows one pair of colours; after that the cube answers to
 * outer face turns exactly as a three by three does, and the stages that solve
 * one solve this.
 */
namespace rubiks::cube::solver {

/**
 * A solver for the cubes that have to be turned into a three by three first.
 *
 * The reduction above, and then the stages that finish a three by three --
 * which is the whole of the method, and the reason those stages were written
 * to read pieces as faces and colours rather than as coordinates.
 *
 * **Four up to the largest cube the application builds**, which is every size
 * it builds that is not already a three by three. Nothing in the method knows
 * a size: a workshop is derived from the cube it is asked about.
 *
 * The cost is not flat across that range. A forty-move scramble takes about
 * two and a half seconds to solve at nine and about two minutes at
 * twenty-eight, and the answer grows from a thousand moves to nearly eight
 * thousand. The first solve of a size pays for its workshop on top of that --
 * seventeen seconds at twenty-eight. Every one of them solves; what a caller
 * has to decide is whether to make somebody wait, because this does not return
 * until it is done.
 *
 * What it builds and holds is one workshop per size it has been asked about:
 * which cells a size has, which tools move which of them, where a setup leaves
 * each one. That is the state the interface exists to make room for, arriving
 * a little later than the constructor because the size of the cube is not
 * known until one turns up.
 */
class Reduction final : public Solver {
public:
    Reduction();
    ~Reduction() override;

    [[nodiscard]] bool supports(int size) const noexcept override;

    [[nodiscard]] std::vector<CubeMove> solve(
        const CubeState& state) const override;

private:
    struct Workshops;

    // Held by pointer so that a workshop can be built on the way through a
    // `solve()` that is const: what the interface promises is that solving
    // does not change the answer, not that nothing is written down.
    std::unique_ptr<Workshops> prepared_;
};

}  // namespace rubiks::cube::solver
