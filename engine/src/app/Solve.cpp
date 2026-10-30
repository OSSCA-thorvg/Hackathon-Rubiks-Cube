#include "app/Application.hpp"

#include <utility>

#include "app/ApplicationState.hpp"
#include "cube/MoveTimeline.hpp"
#include "cube/solver/Solver.hpp"

namespace rubiks::app {

using namespace detail;

namespace {

/** The solver that handles a cube of this size, or nothing. */
[[nodiscard]] const cube::solver::Solver* solver_for(int size) noexcept
{
    if (!state) return nullptr;

    for (const auto& solver : state->solvers) {
        if (solver->supports(size)) return solver.get();
    }
    return nullptr;
}

}  // namespace

bool can_solve() noexcept
{
    // The lifecycle first: `size_of_cube()` asks the state what it is, so
    // there has to be one before anything else is asked.
    return state && solver_for(size_of_cube()) != nullptr;
}

bool solve() noexcept
{
    if (!can_rewind() || !can_solve()) return false;

    // Worked out before anything is written, so that a cube already solved --
    // the one case with nothing to play -- refuses here with the record and
    // the cube untouched, the way every other empty command does.
    auto plan = solver_for(size_of_cube())->solve(state->cube_state);
    if (plan.empty()) return false;

    // Written down and then played forward, which is what a scramble does. So
    // there is no third way of getting moves into the cube, and a solve broken
    // off part way leaves the rest of it above the cursor for a redo.
    state->timeline.record_ahead(plan);
    return play_rewind(std::move(plan), cube::TimelineEffect::Advance);
}

}  // namespace rubiks::app
