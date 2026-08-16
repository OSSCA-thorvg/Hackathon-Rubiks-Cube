#pragma once

#include <vector>

#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"

/**
 * Solving a cube, kept apart from the cube itself.
 *
 * The one part of the domain that reads a cube rather than being one. It is
 * its own target for the same reason `cube` has no dependencies at all: a
 * solver is the largest single body of code here and has nothing to do with
 * drawing or turning, so somebody reading the domain should be able to finish
 * reading it without passing through this.
 *
 * The arrow only points one way. This sees `cube`; `cube` does not see this.
 */
namespace rubiks::cube::solver {

/**
 * A way of turning some cube into a solved one.
 *
 * An object rather than a free function because a solver has properties of its
 * own: which sizes it solves differs between algorithms, and a search-based
 * one has to build its pruning tables once and hold them. A free function
 * would have to answer the first question somewhere else and rebuild the
 * second on every call.
 *
 * Every implementation solves every cube it says it supports. There is no
 * failure path, and that is a property of the domain rather than a promise:
 * `CubeState` exposes no way to reach a cube other than by turning one, so a
 * cube that cannot be solved cannot be built to hand in.
 */
class Solver {
public:
    Solver() = default;
    virtual ~Solver() = default;

    Solver(const Solver&) = delete;
    Solver& operator=(const Solver&) = delete;
    Solver(Solver&&) = delete;
    Solver& operator=(Solver&&) = delete;

    /** Whether this solver solves a cube of `size`. */
    [[nodiscard]] virtual bool supports(int size) const noexcept = 0;

    /**
     * The moves that take `state` to solved.
     *
     * Empty for a cube already solved, which is a fact about it rather than a
     * refusal -- the caller's "a command with nothing to do is refused" is the
     * same answer it gives a rewind with nothing to rewind.
     *
     * Calling this for a size `supports()` denies is a precondition violation.
     * Two questions with two answers is what keeps "no solver for this size"
     * from arriving as the same empty vector a solved cube does.
     */
    [[nodiscard]] virtual std::vector<CubeMove> solve(
        const CubeState& state) const = 0;
};

}  // namespace rubiks::cube::solver
