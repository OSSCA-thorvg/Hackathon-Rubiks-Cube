#include "cube/MoveTimeline.hpp"

#include <algorithm>
#include <cassert>
#include <utility>

namespace rubiks::cube {

void MoveTimeline::begin_scramble(std::vector<CubeMove> moves)
{
    moves_ = std::move(moves);
    cursor_ = 0;
    scramble_end_ = moves_.size();
}

void MoveTimeline::record(const CubeMove& move)
{
    moves_.resize(cursor_);
    scramble_end_ = std::min(scramble_end_, cursor_);
    moves_.push_back(move);
    ++cursor_;
}

const CubeMove& MoveTimeline::at(std::size_t index) const noexcept
{
    assert(index < moves_.size());
    return moves_[index];
}

void MoveTimeline::step(TimelineEffect effect) noexcept
{
    switch (effect) {
        case TimelineEffect::Advance:
            assert(cursor_ < moves_.size());
            ++cursor_;
            return;
        case TimelineEffect::Rewind:
            assert(cursor_ > 0);
            --cursor_;
            return;
        case TimelineEffect::None:
            return;
    }
}

void MoveTimeline::clear() noexcept
{
    moves_.clear();
    cursor_ = 0;
    scramble_end_ = 0;
}

std::vector<CubeMove> rewind_plan(const MoveTimeline& timeline, std::size_t to)
{
    std::vector<CubeMove> plan;

    // Counting down to `to` rather than up to the cursor, so `at(i - 1)` is
    // only ever reached with `i > to >= 0` and the index cannot wrap.
    for (std::size_t i = timeline.cursor(); i > to; --i) {
        plan.push_back(inverse(timeline.at(i - 1)));
    }
    return plan;
}

std::vector<CubeMove> redo_plan(const MoveTimeline& timeline, std::size_t to)
{
    std::vector<CubeMove> plan;
    for (std::size_t i = timeline.cursor(); i < to; ++i) {
        plan.push_back(timeline.at(i));
    }
    return plan;
}

}  // namespace rubiks::cube
