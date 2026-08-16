#pragma once

#include <cstddef>
#include <vector>

#include "cube/CubeMove.hpp"

namespace rubiks::cube {

/**
 * What one committed move does to the timeline cursor.
 *
 * Named values rather than a delta of -1, 0 or +1, because what a wrong one
 * costs is the whole record: a watched pattern handed `Advance` would carry
 * the cursor past the end of the timeline on its very first turn. Each name
 * says what happens rather than by how much, so there is no arithmetic left
 * to get backwards.
 */
enum class TimelineEffect { Advance, Rewind, None };

/**
 * One linear record of the moves a cube session is made of.
 *
 * A scramble and the moves made on top of it are one array, and two indices
 * point into it: `cursor()` is how many of them are on the cube right now, and
 * `scramble_end()` is where the scramble stops and the user's own moves begin.
 * What happens to a cube is always one sequence, so the record of it is one
 * too, and the two stretches the screen and a shared link want are read off
 * `scramble_end()` rather than stored apart.
 *
 * The shape is what holds the invariant rather than a rule that has to be
 * kept: with a single cursor there is no way to write down a state where a
 * later move is applied and an earlier one is not. "The user's moves exist
 * only on top of a finished scramble" is checked nowhere here, because it
 * cannot be expressed.
 *
 * `cursor() < scramble_end()` is not a violation either -- it is the middle of
 * a rewind, with a stretch of scramble waiting to be put back.
 *
 * Moves are kept exactly as they were played. Inverting is the rewind's job
 * and happens where a plan is made; a timeline that stored the inverse would
 * turn the wrong way when the same move was played forward again.
 */
class MoveTimeline {
public:
    /**
     * Replaces everything with a new, not-yet-applied scramble sequence.
     *
     * The cursor starts at nothing, because a scramble is turned into the cube
     * rather than applied to it: the moves are all recorded and none of them
     * has happened yet.
     */
    void begin_scramble(std::vector<CubeMove> moves);

    /**
     * Records a user move at the cursor, discarding everything after it.
     *
     * One rule covers both things that can sit past the cursor: a redo tail
     * the user has rewound behind, and the stretch of scramble a rewind has
     * taken back off the cube. Pulling `scramble_end` down to the cursor when
     * it sits above it is that second cut, and it is one `min` rather than a
     * case of its own.
     */
    void record(const CubeMove& move);

    /**
     * Writes a sequence in above the cursor, leaving the cursor where it is.
     *
     * What `record()` does for one move that has already happened, for a
     * stretch of moves that has not: the redo tail and any scramble left above
     * the cursor are cut in exactly the same way, and then the whole plan goes
     * on the end.
     *
     * This is how a solution reaches the cube. A solver's moves are new -- no
     * part of the record is waiting to be replayed into them -- so they are
     * written down first and then played with `Advance`, which is the path a
     * scramble already takes. The alternative was a fourth `TimelineEffect`
     * that recorded as it went, and it would have cost this class its one
     * plain sentence: what is being played is already written down.
     *
     * Stopping part way therefore leaves the rest of the solution above the
     * cursor, where a redo can take it up again a move at a time.
     */
    void record_ahead(const std::vector<CubeMove>& moves);

    /**
     * The recorded move at an index, as it was played.
     *
     * The only accessor, and it does not invert. A "the move at the cursor"
     * accessor would hand back the same move over and over while a plan was
     * being built, since the cursor only moves on a commit.
     */
    [[nodiscard]] const CubeMove& at(std::size_t index) const noexcept;

    /**
     * Moves the cursor after a planned move was actually committed.
     *
     * Staying inside `[0, size()]` is fixed by a debug assertion rather than a
     * refusal: the one caller is the application's commit path, so a violation
     * is a bug to be caught while it is being written, and a runtime recovery
     * would be a branch nothing could ever exercise.
     */
    void step(TimelineEffect effect) noexcept;

    [[nodiscard]] std::size_t size() const noexcept { return moves_.size(); }

    [[nodiscard]] std::size_t cursor() const noexcept { return cursor_; }

    [[nodiscard]] std::size_t scramble_end() const noexcept
    {
        return scramble_end_;
    }

    void clear() noexcept;

private:
    std::vector<CubeMove> moves_;
    std::size_t cursor_ = 0;
    std::size_t scramble_end_ = 0;
};

/**
 * The moves that take a timeline's cursor down to `to`.
 *
 * The recorded moves from the cursor back to `to`, each inverted -- which is
 * an undo when `to` is one below the cursor and a solve when it is nothing.
 * The two commands differ by this argument and by nothing else, so there is no
 * direction to derive and no target to hold on to while a sequence plays.
 *
 * The plan is always exactly `cursor() - to` moves long, and cancelling
 * opposite pairs is deliberately not done: watching your own moves come back
 * off the cube in the order you made them is the point of it.
 */
[[nodiscard]] std::vector<CubeMove> rewind_plan(const MoveTimeline& timeline,
                                                std::size_t to);

/**
 * The moves that take a timeline's cursor back up to `to`.
 *
 * The recorded moves as they were played, so a redo is the exact reverse of
 * the rewind that came before it.
 *
 * Neither of these is a member: a timeline is what happened, and these are
 * questions asked about it. They read nothing but the public accessors, and
 * leaving them outside is what keeps the container from acquiring a second
 * idea of what its cursor is for.
 */
[[nodiscard]] std::vector<CubeMove> redo_plan(const MoveTimeline& timeline,
                                              std::size_t to);

}  // namespace rubiks::cube
