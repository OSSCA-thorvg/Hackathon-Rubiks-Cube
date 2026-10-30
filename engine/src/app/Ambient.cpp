#include "app/Application.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "app/ApplicationState.hpp"
#include "cube/Cube.hpp"
#include "cube/CubeMove.hpp"
#include "cube/Face.hpp"
#include "cube/MoveTimeline.hpp"

namespace rubiks::app {

using namespace detail;

namespace {

/**
 * How much of the cube one move of a watched pattern takes with it.
 *
 * Said as a share of the cube rather than as a depth, because the table is
 * written once for every size: `Wide` is half the cube and `Slice` is the
 * layer at the bottom of that half, both measured with the same
 * `size / 2` the scramble picks its depths inside. On a 2x2 and a 3x3 that
 * half is one layer, so all three of these are the face turn and those two
 * cubes are watched exactly as they were before there were others.
 */
enum class AmbientDepth { Face, Wide, Slice };

/** One move of a watched pattern, before it is given a cube to turn. */
struct AmbientStep {
    cube::Face face;
    AmbientDepth depth;
    int face_turns;
};

/**
 * The patterns the watching mode repeats, one of which a choice picks out.
 *
 * Faces and shares rather than moves, because the cube they will turn is not
 * known here: the same four are offered at every size, and what a face and a
 * half of it are depends on the cube in hand. Which of them comes back round
 * soonest is worked out in the phase document and is what put these four here
 * rather than four others; carrying those numbers in the code would be
 * carrying values nothing reads.
 *
 * Four moves, and on a big cube four moves is not much of a cube to look at --
 * which is what the winding either side of them is for, below.
 *
 * The comments write each one as it reads on a 4x4, where the half is two
 * layers: `Rw` is the half, `2R` the slice behind the face, `R` the face.
 */
constexpr std::array<std::array<AmbientStep, 4>, kAmbientPatternCount>
    kAmbientPatterns{{
        // 2R Uw' Dw' 2F
        {{{cube::Face::Right, AmbientDepth::Slice, 1},
          {cube::Face::Up, AmbientDepth::Wide, -1},
          {cube::Face::Down, AmbientDepth::Wide, -1},
          {cube::Face::Front, AmbientDepth::Slice, 1}}},
        // 2R Uw 2F Dw
        {{{cube::Face::Right, AmbientDepth::Slice, 1},
          {cube::Face::Up, AmbientDepth::Wide, 1},
          {cube::Face::Front, AmbientDepth::Slice, 1},
          {cube::Face::Down, AmbientDepth::Wide, 1}}},
        // Rw 2B 2D' L2
        {{{cube::Face::Right, AmbientDepth::Wide, 1},
          {cube::Face::Back, AmbientDepth::Slice, 1},
          {cube::Face::Down, AmbientDepth::Slice, -1},
          {cube::Face::Left, AmbientDepth::Face, 2}}},
        // 2R 2L' Uw F2
        {{{cube::Face::Right, AmbientDepth::Slice, 1},
          {cube::Face::Left, AmbientDepth::Slice, -1},
          {cube::Face::Up, AmbientDepth::Wide, 1},
          {cube::Face::Front, AmbientDepth::Face, 2}}},
    }};

/**
 * How deep the walk below reaches on a cube of `size`.
 *
 * Half of it, rounded up, so an odd cube's middle layer is included: reached
 * from either face it is the same layer, and leaving it out would be the one
 * band of each axis that never moved.
 */
[[nodiscard]] int ambient_walk_depth(int size) noexcept
{
    return std::max(1, (size + 1) / 2);
}

/**
 * How many moves a cube of `size` is watched with.
 *
 * The written pattern for a cube with no layers inside; for one that has
 * them, a walk long enough to grip it by every face at every depth exactly
 * once -- six faces by however deep the walk reaches.
 *
 * Four moves on nine layers turn the same few bands over and over: most of
 * the cube sits still, the rest churns in place, and it stays that way
 * however long the watching runs.
 */
[[nodiscard]] int ambient_walk_length(int size) noexcept
{
    return size < 4 ? 0 : 6 * ambient_walk_depth(size);
}

/**
 * One move of the walk, worked out from where it falls rather than listed.
 *
 * Written as a rule rather than a table because what it has to do is cover
 * the cube, and covering is what a rule is good at. The face steps round all
 * six on every move and the depth only after all six have had a turn, so the
 * two counters do not fall into step and the walk grips every face at every
 * depth exactly once. Every other move takes the layers above its depth along
 * instead of turning one alone, and every third turns back the other way, so
 * what is on screen is a slice sliding, then a block swinging, rather than
 * one steady wind.
 *
 * Reaching every depth from every face is what reaches every band: a layer
 * this walk does not grip from one side it grips from the other.
 *
 * The cost is the one thing a written table bought, and it is paid knowingly.
 * A sequence that mixes the whole of a big cube is an element of a very large
 * group, so the number of rounds before it comes back to a solved cube is
 * astronomical -- where four hand-picked moves came back inside a couple of
 * hundred. The two cannot both be had: staying near the solved cube is the
 * same thing as leaving most of the cube alone, and that is what made a big
 * one not worth watching. Nothing rests on the return, because what puts the
 * cube back is the snapshot the watching took.
 *
 * Nothing for a move the cube cannot make, which the walk does not produce:
 * its deepest reach is half the cube and its widest is that same half.
 */
[[nodiscard]] std::optional<cube::CubeMove> ambient_walk_move(
    std::uint32_t choice, int index, int size) noexcept
{
    constexpr std::array<cube::Face, 6> kWalkFaces{
        cube::Face::Right, cube::Face::Up,   cube::Face::Front,
        cube::Face::Left,  cube::Face::Down, cube::Face::Back};

    const int deepest = ambient_walk_depth(size);
    const int pass = index / static_cast<int>(kWalkFaces.size());
    const auto turn = static_cast<int>(choice % kAmbientPatternCount);

    const auto face_index = static_cast<std::size_t>(index + turn);
    const cube::Face face = kWalkFaces[face_index % kWalkFaces.size()];

    // The depth moves on once the faces have come round, which is what keeps
    // the two from repeating together. Where it starts is the choice's, so
    // the four watchings do not open the same way.
    const int depth = 1 + (pass + turn) % deepest;

    // A slice, then a half, then a slice again: one carries a single layer
    // round and the other takes everything above it along, and having both on
    // screen is what a cube with layers inside has to show.
    const bool wide = (index + pass) % 2 != 0;

    int turns = 1;
    if (index % 3 == 2) turns = -turns;
    if (choice % 2 != 0) turns = -turns;

    return depth_move(face, wide ? 1 : depth, depth, turns, size);
}

/** The depth range one share of a cube of `size` stands for. */
[[nodiscard]] std::pair<int, int> ambient_depths(AmbientDepth depth,
                                                 int size) noexcept
{
    // The same half the scramble measures its depths inside, so "how deep a
    // move goes on this cube" has one answer in this file.
    const int deepest = size / 2;

    switch (depth) {
        case AmbientDepth::Wide:
            return {1, deepest};
        case AmbientDepth::Slice:
            return {deepest, deepest};
        case AmbientDepth::Face:
            break;
    }
    return {1, 1};
}

/**
 * Whether the sequence playing is a pattern being watched.
 *
 * Which is to say whether there is a cube waiting to be put back. Watching is
 * a playback with somewhere to return to and no end of its own, so having the
 * snapshot is not a symptom of it -- it is the whole of what it is.
 */
[[nodiscard]] bool ambient_running() noexcept
{
    return state->playback && state->playback->restore_to.has_value();
}

}  // namespace

std::vector<cube::CubeMove> ambient_pattern(std::uint32_t choice, int size)
{
    if (!valid_cube_size(size)) return {};

    // A cube with layers inside is walked over rather than played a written
    // pattern into: the pattern is four moves, and four moves of a 9x9 is not
    // a cube anybody would watch.
    const int walk = ambient_walk_length(size);
    if (walk > 0) {
        std::vector<cube::CubeMove> pattern;
        pattern.reserve(static_cast<std::size_t>(walk));
        for (int index = 0; index < walk; ++index) {
            const auto move = ambient_walk_move(choice, index, size);
            if (!move) return {};

            pattern.push_back(*move);
        }
        return pattern;
    }

    std::vector<cube::CubeMove> pattern;
    for (const AmbientStep& step :
         kAmbientPatterns[choice % kAmbientPatternCount]) {
        // Where the table meets the cube being watched: a face and a share of
        // it become the layers this size actually has. Every share is the
        // face itself at these two sizes, which is what they have always been
        // watched with.
        const auto [first, last] = ambient_depths(step.depth, size);
        const auto move =
            depth_move(step.face, first, last, step.face_turns, size);
        if (!move) return {};

        pattern.push_back(*move);
    }
    return pattern;
}

bool ambient_start(std::uint32_t choice) noexcept
{
    if (!state) return false;
    if (ambient_running()) return false;

    // A pattern turns the cube, and nothing may turn it behind a draft.
    if (state->paint) return false;

    // Whatever else was playing gives way, the way it does for a scramble.
    // Nothing is put back on the way in: the cube being watched is the cube
    // that was there, which is exactly what the snapshot below records.
    discard_playback();

    // Nothing of a watched pattern is written down: it is an interlude, and
    // the cube it borrowed is put back at the end of it. Told to advance, its
    // very first turn would carry the cursor past the end of the record.
    state->playback = Player{ambient_pattern(choice, size_of_cube()),
                             0,
                             kAmbientTempoMs,
                             cube::TimelineEffect::None,
                             true,
                             false,
                             state->cube_state};
    return true;
}

void ambient_stop() noexcept
{
    if (!state || !ambient_running()) return;

    // Taken out of the player before the player is thrown away, and put back
    // afterwards: the discard is what stops a turn already in flight, and a
    // cube restored ahead of it would have that turn land on top of it.
    cube::Cube origin = std::move(*state->playback->restore_to);
    discard_playback();
    state->cube_state = std::move(origin);
}

bool is_ambient() noexcept
{
    return state && ambient_running();
}

}  // namespace rubiks::app
