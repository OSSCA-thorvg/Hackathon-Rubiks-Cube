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
 * The moves that make every face's centre block one colour.
 *
 * The first half of a reduction, and the half that has no written sequences at
 * all. Two facts about a cube shape it. A face turn cannot move a centre piece
 * off its face -- only a slice can -- so face turns are free and slices are
 * the whole cost. And a commutator of two turns moves centre pieces in one of
 * exactly two sizes: a slice against a face it cuts exchanges a strip between
 * that face and one neighbour, leaving the other four untouched, while two
 * slices about different axes exchange one piece on each of the six faces
 * whatever the size of the cube.
 *
 * The coarse one cannot finish a face -- near the end it gives back more of
 * the colour than it fetches -- so the fine one is needed, and it touches
 * every face. That is why nothing here works face by face: it counts every
 * centre piece that is home, over the whole cube, and takes whichever
 * commutator raises that count the most.
 */
[[nodiscard]] std::vector<CubeMove> solve_centres(const CubeState& state);

/**
 * How the centre stage's tools sit against the cube's clusters, at one size.
 *
 * A cluster is the set of slots one piece can ever reach. No sequence of turns
 * moves a piece out of its own, so the centres of a big cube are not one
 * puzzle but a few dozen side by side -- the same fact the edge stage already
 * counts, under the name of worlds. A tool that stays inside a single cluster
 * can be chosen by looking at that cluster alone, which is what makes solving
 * them one at a time possible; a tool that spans several cannot.
 *
 * Whether our tools are of the first kind is not something to assume. The
 * families were written to be short and to keep the other faces still, not to
 * respect clusters, and the coarse one is not expected to: it carries a whole
 * strip between faces. So this counts, and a test reads the counts.
 */
struct CentreLocality {
    int cells = 0;             ///< Centre stickers there are at this size.
    int clusters = 0;          ///< Closed sets they fall into.
    int smallest_cluster = 0;  ///< Cells in the smallest of them.
    int largest_cluster = 0;   ///< Cells in the largest.

    int coarse = 0;        ///< Tools that are a slice against a face it cuts.
    int coarse_local = 0;  ///< How many of those stay inside one cluster.
    int fine = 0;          ///< Tools that are two slices about different axes.
    int fine_local = 0;
    int narrow = 0;  ///< Tools whose first half is a slice under a face turn.
    int narrow_local = 0;

    /// The most clusters any one fine or narrow tool reaches into.
    int widest_fine_reach = 0;

    /// Local fine and narrow tools the poorest cluster has to work with.
    int fewest_local_tools = 0;
    /// The same for the best-served cluster.
    int most_local_tools = 0;
};

/** The counts above, worked out for a cube of `size`. */
[[nodiscard]] CentreLocality centre_locality(int size);

/**
 * The moves that turn a big cube into a three by three: both halves, and the
 * one thing left over when they are done.
 *
 * A cube can come out of a reduction in a position no three by three can be
 * in, because pieces a three by three cannot tell apart -- the centres of one
 * face -- can hold a permutation the rest of the cube then has to answer for.
 * Two things follow from it, and neither is met with a written sequence.
 *
 * One is corners sitting in an odd order, which is seen by projecting the
 * reduced cube and asking. What answers it is a change of target: two rows are
 * told to want each other's colours, which is one swap of the three by three's
 * edges and puts the same oddness on the other side of the ledger.
 *
 * The other is the edge pieces' own arrangement, which the stage cannot reach
 * at all when it is an odd number of swaps from what is being asked -- every
 * tool that keeps the centres is an even permutation of them. What answers
 * that is the one odd move a big cube has, a quarter turn of a single inner
 * slice, after which the centres are gathered again.
 *
 * What comes back is always reduced, and always a cube the three-layer stages
 * can finish.
 */
[[nodiscard]] std::vector<CubeMove> reduce(const CubeState& state);

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
 * three and a half seconds to solve at nine and about three and a half minutes
 * at twenty-eight, and the answer grows from a thousand moves to nearly eight
 * thousand. Every one of them solves; what a caller has to decide is whether
 * to make somebody wait, because this does not return until it is done.
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
