#include "app/Application.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "app/ApplicationState.hpp"
#include "cube/CubeMove.hpp"
#include "cube/MoveTimeline.hpp"
#include "cube/PackedMove.hpp"

namespace rubiks::app {

using namespace detail;

namespace {

/** How many of the moves on the cube right now are the user's own. */
[[nodiscard]] std::size_t applied_user_moves() noexcept
{
    const auto& timeline = state->timeline;
    return timeline.cursor() > timeline.scramble_end()
               ? timeline.cursor() - timeline.scramble_end()
               : 0;
}

}  // namespace

namespace detail {

/**
 * Hands a rewind or a replay to the player, if there is anything in it.
 *
 * What the three commands share once each has worked out its own target: the
 * plan is made in full here and consumed by the ordinary playback loop, so
 * this phase adds no second way of playing moves into the cube. An empty plan
 * is a refusal, which is what a command with nothing to do amounts to.
 */
[[nodiscard]] bool play_rewind(std::vector<cube::CubeMove> plan,
                               cube::TimelineEffect effect) noexcept
{
    if (plan.empty()) return false;

    // Stoppable, unlike either of the other two producers: this is the one
    // sequence a user waits through, and a solve can be dozens of moves.
    state->playback = Player{std::move(plan), 0,    kRewindTempoMs, effect,
                             false,           true, std::nullopt};
    return true;
}

/** Whether a rewind may be started at all, whatever it would rewind. */
[[nodiscard]] bool can_rewind() noexcept
{
    return state && !cube_held();
}

}  // namespace detail

std::uint32_t committed_move_count() noexcept
{
    return state ? static_cast<std::uint32_t>(applied_user_moves()) : 0;
}

bool undo() noexcept
{
    if (!can_rewind()) return false;

    const auto& timeline = state->timeline;

    // The lower bound is the policy: what is below the end of the scramble is
    // not the user's to take back, so a cube that has only been scrambled and
    // one a solve has rewound into the scramble both refuse here without a
    // case of their own.
    if (timeline.cursor() <= timeline.scramble_end()) return false;

    return play_rewind(cube::rewind_plan(timeline, timeline.cursor() - 1),
                       cube::TimelineEffect::Rewind);
}

bool redo() noexcept
{
    if (!can_rewind()) return false;

    const auto& timeline = state->timeline;
    if (timeline.cursor() >= timeline.size()) return false;

    // Forward through the record, which is the same thing a scramble arriving
    // does -- so it carries the same effect.
    return play_rewind(cube::redo_plan(timeline, timeline.cursor() + 1),
                       cube::TimelineEffect::Advance);
}

bool solve_rewind() noexcept
{
    if (!can_rewind()) return false;

    // Undo with a different target and nothing else: the plan is longer, and
    // the length of it is the whole difference between the two commands.
    return play_rewind(cube::rewind_plan(state->timeline, 0),
                       cube::TimelineEffect::Rewind);
}

std::uint32_t timeline_length() noexcept
{
    return state ? static_cast<std::uint32_t>(state->timeline.size()) : 0;
}

std::uint32_t timeline_cursor() noexcept
{
    return state ? static_cast<std::uint32_t>(state->timeline.cursor()) : 0;
}

std::uint32_t timeline_scramble_end() noexcept
{
    return state ? static_cast<std::uint32_t>(state->timeline.scramble_end())
                 : 0;
}

std::uint32_t timeline_move(std::uint32_t index) noexcept
{
    if (!state || index >= state->timeline.size()) return 0;

    // The record hands over the move as it was played; normalizing the turns
    // happens in the packing, which is on nobody's replay path.
    return cube::pack(state->timeline.at(index));
}

}  // namespace rubiks::app
