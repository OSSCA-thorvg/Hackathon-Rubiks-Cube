#include "cube/MoveTimeline.hpp"

#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cube/CubeMove.hpp"

namespace {

constexpr int kSize = 3;

using rubiks::cube::CubeMove;
using rubiks::cube::MoveTimeline;
using rubiks::cube::TimelineEffect;

/** A scramble of two named moves, as the generator would hand one over. */
[[nodiscard]] std::vector<CubeMove> two_move_scramble()
{
    using namespace rubiks::cube::moves;
    return {R(kSize), U(kSize)};
}

/**
 * Plays a whole sequence in, the way the application does.
 *
 * A scramble is recorded before any of it has happened and the cursor follows
 * the commits, so "the scramble arrived" is `begin_scramble` and one `step`
 * per move rather than anything of its own.
 */
void arrive(MoveTimeline& timeline, const std::vector<CubeMove>& scramble)
{
    timeline.begin_scramble(scramble);
    for (std::size_t index = 0; index < scramble.size(); ++index) {
        timeline.step(TimelineEffect::Advance);
    }
}

/** A move told apart from the others only by which layer it names. */
[[nodiscard]] CubeMove numbered(int layer)
{
    return CubeMove{rubiks::cube::Axis::X, rubiks::cube::layer(layer), 1};
}

}  // namespace

// A two-record model would need a test here that the two cursors only ever
// hold a legal combination -- that the user's count is nothing until the
// scramble is complete. There is deliberately no such test in this file, and
// nothing in MoveTimeline for it to guard: one array and one cursor cannot
// express a state where a later move is applied and an earlier one is not, so
// the property is a fact about the shape rather than a rule under test.

TEST_CASE("a scramble is recorded whole and arrives one commit at a time")
{
    using namespace rubiks::cube::moves;

    MoveTimeline timeline;
    REQUIRE(timeline.size() == 0);
    REQUIRE(timeline.cursor() == 0);
    REQUIRE(timeline.scramble_end() == 0);

    timeline.begin_scramble(two_move_scramble());
    REQUIRE(timeline.size() == 2);
    REQUIRE(timeline.scramble_end() == 2);

    // Recorded but not yet on the cube: the moves are turned into it over the
    // frames that follow, and each commit is what moves the cursor.
    REQUIRE(timeline.cursor() == 0);

    timeline.step(TimelineEffect::Advance);
    REQUIRE(timeline.cursor() == 1);
    timeline.step(TimelineEffect::Advance);
    REQUIRE(timeline.cursor() == 2);

    // Kept as they were played, right way round.
    REQUIRE(timeline.at(0) == R(kSize));
    REQUIRE(timeline.at(1) == U(kSize));

    // A second scramble replaces the lot rather than being added to it.
    timeline.begin_scramble({F(kSize)});
    REQUIRE(timeline.size() == 1);
    REQUIRE(timeline.cursor() == 0);
    REQUIRE(timeline.scramble_end() == 1);
    REQUIRE(timeline.at(0) == F(kSize));

    timeline.clear();
    REQUIRE(timeline.size() == 0);
    REQUIRE(timeline.cursor() == 0);
    REQUIRE(timeline.scramble_end() == 0);
}

TEST_CASE("a recorded move lands at the cursor and takes the cursor with it")
{
    using namespace rubiks::cube::moves;

    MoveTimeline timeline;
    arrive(timeline, two_move_scramble());

    timeline.record(F(kSize));
    REQUIRE(timeline.size() == 3);
    REQUIRE(timeline.cursor() == 3);
    REQUIRE(timeline.scramble_end() == 2);
    REQUIRE(timeline.at(2) == F(kSize));

    // Stored as played rather than inverted: a redo replays this move, and an
    // inverted store would send it the other way.
    REQUIRE(timeline.at(2).quarter_turns == F(kSize).quarter_turns);
}

TEST_CASE("a new move discards everything after the cursor, in one rule")
{
    using namespace rubiks::cube::moves;

    SECTION("a redo tail the user rewound behind")
    {
        MoveTimeline timeline;
        arrive(timeline, two_move_scramble());
        timeline.record(F(kSize));
        timeline.record(D(kSize));
        REQUIRE(timeline.size() == 4);

        // Two undos, so two of the user's moves are waiting to be redone.
        timeline.step(TimelineEffect::Rewind);
        timeline.step(TimelineEffect::Rewind);
        REQUIRE(timeline.cursor() == 2);
        REQUIRE(timeline.size() == 4);

        timeline.record(L(kSize));
        REQUIRE(timeline.size() == 3);
        REQUIRE(timeline.cursor() == 3);
        REQUIRE(timeline.at(2) == L(kSize));

        // The scramble is still whole, because the cut never reached it.
        REQUIRE(timeline.scramble_end() == 2);
    }

    SECTION("a stretch of scramble a rewind took back off the cube")
    {
        MoveTimeline timeline;
        arrive(timeline, two_move_scramble());
        timeline.record(F(kSize));

        // Rewound past the user's move and into the scramble, which is where
        // a solve stopped part way through leaves the cursor.
        timeline.step(TimelineEffect::Rewind);
        timeline.step(TimelineEffect::Rewind);
        REQUIRE(timeline.cursor() == 1);
        REQUIRE(timeline.scramble_end() == 2);

        // The same rule cuts both: `scramble_end` comes down to the cursor
        // with everything past it, and this is the one `min` doing it.
        timeline.record(L(kSize));
        REQUIRE(timeline.size() == 2);
        REQUIRE(timeline.cursor() == 2);
        REQUIRE(timeline.scramble_end() == 1);
        REQUIRE(timeline.at(0) == R(kSize));
        REQUIRE(timeline.at(1) == L(kSize));
    }

    SECTION("a move on a cube a solve just finished")
    {
        MoveTimeline timeline;
        arrive(timeline, two_move_scramble());

        // All the way down, which is exactly where a solve leaves things.
        timeline.step(TimelineEffect::Rewind);
        timeline.step(TimelineEffect::Rewind);
        REQUIRE(timeline.cursor() == 0);
        REQUIRE(timeline.scramble_end() == 2);

        timeline.record(B(kSize));
        REQUIRE(timeline.size() == 1);
        REQUIRE(timeline.cursor() == 1);
        REQUIRE(timeline.scramble_end() == 0);
    }
}

TEST_CASE("watching leaves no trace on the record")
{
    MoveTimeline timeline;
    arrive(timeline, two_move_scramble());

    for (int turn = 0; turn < 50; ++turn) {
        timeline.step(TimelineEffect::None);
    }

    REQUIRE(timeline.size() == 2);
    REQUIRE(timeline.cursor() == 2);
    REQUIRE(timeline.scramble_end() == 2);
}

TEST_CASE("a rewind plan is the recorded moves backwards, each inverted")
{
    using namespace rubiks::cube::moves;
    using rubiks::cube::rewind_plan;

    MoveTimeline timeline;
    arrive(timeline, two_move_scramble());
    timeline.record(F(kSize));
    timeline.record(D(kSize));

    // S0 S1 U0 U1 -> U1' U0' S1' S0'
    const std::vector<CubeMove> expected{
        inverse(D(kSize)), inverse(F(kSize)), inverse(U(kSize)),
        inverse(R(kSize))};
    REQUIRE(rewind_plan(timeline, 0) == expected);

    // Undo and solve are the same function with a different argument, and the
    // length of a plan is the distance it covers.
    REQUIRE(rewind_plan(timeline, timeline.cursor() - 1).size() == 1);
    REQUIRE(rewind_plan(timeline, timeline.cursor() - 1)[0] ==
            inverse(D(kSize)));
    REQUIRE(rewind_plan(timeline, 0).size() == 4);
    REQUIRE(rewind_plan(timeline, timeline.cursor()).empty());
}

TEST_CASE("a redo plan replays the recorded moves as they were")
{
    using namespace rubiks::cube::moves;
    using rubiks::cube::redo_plan;
    using rubiks::cube::rewind_plan;

    MoveTimeline timeline;
    arrive(timeline, two_move_scramble());
    timeline.record(F(kSize));
    timeline.record(D(kSize));

    // Rewound all the way, as a solve leaves it.
    for (int step = 0; step < 4; ++step) timeline.step(TimelineEffect::Rewind);
    REQUIRE(timeline.cursor() == 0);

    const std::vector<CubeMove> expected{R(kSize), U(kSize), F(kSize),
                                         D(kSize)};
    REQUIRE(redo_plan(timeline, timeline.size()) == expected);

    // One step forward is one move, the mirror of one step back.
    REQUIRE(redo_plan(timeline, timeline.cursor() + 1).size() == 1);
    REQUIRE(redo_plan(timeline, timeline.cursor() + 1)[0] == R(kSize));
    REQUIRE(redo_plan(timeline, timeline.cursor()).empty());

    // And the two directions are exact reverses of one another: undoing a
    // sequence and redoing it are the same moves read the other way.
    MoveTimeline played;
    arrive(played, two_move_scramble());
    played.record(F(kSize));
    played.record(D(kSize));
    REQUIRE(rewind_plan(played, 0) == rubiks::cube::inverse(expected));
}

TEST_CASE("a scramble on its own rewinds without running off the start")
{
    using namespace rubiks::cube::moves;
    using rubiks::cube::rewind_plan;

    MoveTimeline timeline;
    arrive(timeline, two_move_scramble());

    // Nobody has made a move, so the whole plan is scramble. The loop stops at
    // the target rather than counting down from it, which is what keeps the
    // unsigned index off zero.
    const std::vector<CubeMove> expected{inverse(U(kSize)), inverse(R(kSize))};
    REQUIRE(rewind_plan(timeline, 0) == expected);
}

TEST_CASE("a plan written in above the cursor waits there")
{
    MoveTimeline timeline;
    timeline.begin_scramble({numbered(0), numbered(1), numbered(2)});
    timeline.step(TimelineEffect::Advance);
    timeline.step(TimelineEffect::Advance);
    timeline.step(TimelineEffect::Advance);

    timeline.record_ahead({numbered(3), numbered(4)});

    // Written down, not applied: the cursor is where it was, and the two new
    // moves are the stretch a playback is about to walk up.
    CHECK(timeline.size() == 5);
    CHECK(timeline.cursor() == 3);
    CHECK(timeline.scramble_end() == 3);
    CHECK(timeline.at(3) == numbered(3));
    CHECK(timeline.at(4) == numbered(4));
}

TEST_CASE("a plan cuts whatever was above the cursor first")
{
    MoveTimeline timeline;
    timeline.begin_scramble({numbered(0), numbered(1), numbered(2)});
    timeline.step(TimelineEffect::Advance);

    // One move on the cube and two of the scramble still waiting. The plan
    // replaces them, and the scramble ends where the cursor is -- the same
    // cut record() makes, because it is the same stretch nobody went through.
    timeline.record_ahead({numbered(3)});

    CHECK(timeline.size() == 2);
    CHECK(timeline.cursor() == 1);
    CHECK(timeline.scramble_end() == 1);
    CHECK(timeline.at(1) == numbered(3));
}

TEST_CASE("an empty plan is a plan that changes nothing")
{
    MoveTimeline timeline;
    timeline.begin_scramble({numbered(0), numbered(1)});
    timeline.step(TimelineEffect::Advance);
    timeline.step(TimelineEffect::Advance);

    timeline.record_ahead({});

    CHECK(timeline.size() == 2);
    CHECK(timeline.cursor() == 2);
    CHECK(timeline.scramble_end() == 2);
}
