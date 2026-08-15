#include "app/Application.hpp"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "EngineLifecycle.hpp"
#include "cube/CubeMove.hpp"
#include "graphics/Layout.hpp"
#include "graphics/NetGeometry.hpp"
#include "math/Types.hpp"

// The record of what has happened to the cube, and the two commands that walk
// back along it. What the cube itself does under a rewind is checked through
// the drawing, which is the only thing here that can tell one cube from
// another without a second copy of the domain.

namespace {

using rubiks::test::arrive;
using rubiks::test::drawn_frame;
using rubiks::test::kFrameMs;
using rubiks::test::settle;
using rubiks::test::turn;

constexpr std::uint32_t kCanvas = 512;
constexpr int kCubeSize = 3;

/** Center of one net cell, which is the shortest way to hold a layer. */
[[nodiscard]] rubiks::math::Vec2 net_cell_point(rubiks::cube::Face face,
                                                int column, int row)
{
    const auto net = rubiks::graphics::layout(kCanvas, kCanvas).net;
    const float face_side =
        net.width / static_cast<float>(rubiks::graphics::kNetColumns);
    const float cell = face_side / static_cast<float>(kCubeSize);
    const auto block = rubiks::graphics::net_block(face);

    return rubiks::math::Vec2{
        net.x + static_cast<float>(block.column) * face_side +
            (static_cast<float>(column) + 0.5f) * cell,
        net.y + static_cast<float>(block.row) * face_side +
            (static_cast<float>(row) + 0.5f) * cell};
}

/**
 * Every value the cursor takes while the application settles, in order.
 *
 * What a rewind is checked against: the moves have to come off one at a time
 * and in one direction, and a plan that dropped, repeated or reordered one of
 * them would show up here as a step of the wrong size or a value seen twice.
 */
[[nodiscard]] std::vector<std::uint32_t> cursor_track()
{
    std::vector<std::uint32_t> track{rubiks::app::timeline_cursor()};

    int frames = 0;
    while (rubiks::app::advance(kFrameMs)) {
        const auto cursor = rubiks::app::timeline_cursor();
        if (cursor != track.back()) track.push_back(cursor);
        ++frames;
        REQUIRE(frames < 2000);
    }

    const auto cursor = rubiks::app::timeline_cursor();
    if (cursor != track.back()) track.push_back(cursor);
    return track;
}

/**
 * The values a sequence running from `from` to `to` has to pass through.
 *
 * One step at a time, in whichever direction the two ends put it: a scramble
 * arriving and a solve rewinding are the same walk read opposite ways.
 */
[[nodiscard]] std::vector<std::uint32_t> stepping(std::uint32_t from,
                                                  std::uint32_t to)
{
    std::vector<std::uint32_t> expected{from};
    while (expected.back() != to) {
        expected.push_back(expected.back() + (to > from ? 1U : -1U));
    }
    return expected;
}

}  // namespace

TEST_CASE("the record is empty and safe before initialization")
{
    REQUIRE_FALSE(rubiks::app::is_initialized());

    REQUIRE(rubiks::app::timeline_length() == 0);
    REQUIRE(rubiks::app::timeline_cursor() == 0);
    REQUIRE(rubiks::app::timeline_scramble_end() == 0);
    REQUIRE(rubiks::app::committed_move_count() == 0);

    REQUIRE_FALSE(rubiks::app::undo());
    REQUIRE_FALSE(rubiks::app::redo());
    REQUIRE_FALSE(rubiks::app::solve_rewind());
    rubiks::app::stop_playback();
}

TEST_CASE("a scramble is recorded whole and arrives one commit at a time")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    REQUIRE(rubiks::app::scramble(42U, 6U));
    REQUIRE(rubiks::app::timeline_length() == 6);
    REQUIRE(rubiks::app::timeline_scramble_end() == 6);

    // Written down in full before any of it has happened: the moves are turned
    // into the cube over the frames that follow.
    REQUIRE(rubiks::app::timeline_cursor() == 0);

    REQUIRE(cursor_track() == stepping(0, 6));
    REQUIRE(rubiks::app::committed_move_count() == 0);

    rubiks::app::reset_cube();
    REQUIRE(rubiks::app::timeline_length() == 0);
    REQUIRE(rubiks::app::timeline_cursor() == 0);
    REQUIRE(rubiks::app::timeline_scramble_end() == 0);
}

TEST_CASE("an undo turns one move back rather than making it again")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    arrive(7U, 4U);
    const auto scrambled = drawn_frame();

    turn(rubiks::cube::Face::Right, 1);
    REQUIRE(rubiks::app::committed_move_count() == 1);
    const auto turned = drawn_frame();
    REQUIRE(turned != scrambled);

    REQUIRE(rubiks::app::undo());
    REQUIRE(rubiks::app::is_busy());
    settle();

    // Exactly back, down to the pixel. A record that stored the inverse would
    // play R again here and land somewhere else entirely.
    REQUIRE(rubiks::app::timeline_cursor() == 4);
    REQUIRE(rubiks::app::committed_move_count() == 0);
    REQUIRE(drawn_frame() == scrambled);

    // The move is still on the record, waiting, and a redo replays it as it
    // was made -- so the two directions are exact reverses.
    REQUIRE(rubiks::app::timeline_length() == 5);
    REQUIRE(rubiks::app::redo());
    settle();
    REQUIRE(rubiks::app::timeline_cursor() == 5);
    REQUIRE(rubiks::app::committed_move_count() == 1);
    REQUIRE(drawn_frame() == turned);

    // Nothing left to replay.
    REQUIRE_FALSE(rubiks::app::redo());
}

TEST_CASE("an undo will not go below the end of the scramble")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    arrive(11U, 5U);

    // Nothing of the user's own on the cube, so there is nothing to take back
    // -- the same refusal a solve that has rewound into the scramble gets.
    REQUIRE(rubiks::app::timeline_cursor() ==
            rubiks::app::timeline_scramble_end());
    REQUIRE_FALSE(rubiks::app::undo());

    // A solve is not refused there, because what it takes back is not the
    // user's moves but everything.
    REQUIRE(rubiks::app::solve_rewind());
    settle();
    REQUIRE(rubiks::app::is_solved());
    REQUIRE_FALSE(rubiks::app::undo());
}

TEST_CASE("a solve is an undo with a further target")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    arrive(3U, 5U);
    turn(rubiks::cube::Face::Up, 1);
    turn(rubiks::cube::Face::Front, -1);
    REQUIRE(rubiks::app::committed_move_count() == 2);

    REQUIRE(rubiks::app::solve_rewind());

    // One move at a time, all the way down, straight through the boundary
    // between the user's moves and the scramble: there is one cursor, so
    // nothing has to decide which record the next move comes off.
    REQUIRE(cursor_track() == stepping(7, 0));

    REQUIRE(rubiks::app::is_solved());
    REQUIRE(rubiks::app::committed_move_count() == 0);

    // The record is whole; only the cursor moved.
    REQUIRE(rubiks::app::timeline_length() == 7);
    REQUIRE(rubiks::app::timeline_scramble_end() == 5);

    // And a cube with nothing left applied is one a rewind solved rather than
    // a person, which is what the two queries say together.
    REQUIRE(rubiks::app::timeline_cursor() == 0);
    REQUIRE(rubiks::app::timeline_scramble_end() > 0);

    // Nothing left to rewind.
    REQUIRE_FALSE(rubiks::app::solve_rewind());
}

TEST_CASE("a cube a solve has just finished can be taken up again")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    arrive(23U, 4U);
    turn(rubiks::cube::Face::Right, 1);

    REQUIRE(rubiks::app::solve_rewind());
    settle();
    REQUIRE(rubiks::app::timeline_cursor() == 0);

    // Accepted rather than refused: refusing would mean the cube that was just
    // solved is the one cube nobody may touch. The move lands at the cursor
    // and everything past it goes -- the user's replayable tail and the
    // scramble underneath it, cut by the one rule.
    turn(rubiks::cube::Face::Up, 1);
    REQUIRE(rubiks::app::timeline_length() == 1);
    REQUIRE(rubiks::app::timeline_cursor() == 1);
    REQUIRE(rubiks::app::timeline_scramble_end() == 0);
    REQUIRE(rubiks::app::committed_move_count() == 1);
}

TEST_CASE("a solve stopped part way leaves the record where the cube is")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    arrive(31U, 8U);
    turn(rubiks::cube::Face::Right, 1);
    REQUIRE(rubiks::app::solve_rewind());

    // Far enough in for the rewind to have passed into the scramble.
    for (int frame = 0; frame < 40; ++frame) {
        static_cast<void>(rubiks::app::advance(kFrameMs));
    }

    rubiks::app::stop_playback();
    REQUIRE_FALSE(rubiks::app::is_busy());

    const auto stopped_at = rubiks::app::timeline_cursor();
    REQUIRE(stopped_at > 0);
    REQUIRE(stopped_at < 8);
    const auto stopped_frame = drawn_frame();

    // Broken off rather than abandoned: nothing else lands afterwards, so the
    // cube stays exactly where the last commit left it.
    settle();
    REQUIRE(rubiks::app::timeline_cursor() == stopped_at);
    REQUIRE(drawn_frame() == stopped_frame);

    // Below the scramble's end, so there is nothing of the user's to take
    // back, and a move made here cuts the record down to the cube.
    REQUIRE_FALSE(rubiks::app::undo());
    turn(rubiks::cube::Face::Down, 1);
    REQUIRE(rubiks::app::timeline_length() == stopped_at + 1);
    REQUIRE(rubiks::app::timeline_cursor() == stopped_at + 1);
    REQUIRE(rubiks::app::timeline_scramble_end() == stopped_at);
    REQUIRE(rubiks::app::committed_move_count() == 1);
}

TEST_CASE("a stopped rewind can be picked up again from where it stopped")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    arrive(5U, 6U);
    REQUIRE(rubiks::app::solve_rewind());

    for (int frame = 0; frame < 30; ++frame) {
        static_cast<void>(rubiks::app::advance(kFrameMs));
    }
    rubiks::app::stop_playback();

    const auto stopped_at = rubiks::app::timeline_cursor();
    REQUIRE(stopped_at > 0);
    REQUIRE(stopped_at < 6);

    // Both directions are still open, and both start from the cursor rather
    // than from wherever the abandoned plan had got to.
    REQUIRE(rubiks::app::redo());
    settle();
    REQUIRE(rubiks::app::timeline_cursor() == stopped_at + 1);

    REQUIRE(rubiks::app::solve_rewind());
    REQUIRE(cursor_track() == stepping(stopped_at + 1, 0));
    REQUIRE(rubiks::app::is_solved());
}

TEST_CASE("only a rewind can be stopped")
{
    SECTION("a scramble runs to its end")
    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

        REQUIRE(rubiks::app::scramble(9U, 6U));
        for (int frame = 0; frame < 10; ++frame) {
            static_cast<void>(rubiks::app::advance(kFrameMs));
        }

        // A scramble stopped half way is a cube nobody asked for, so the
        // default keeps this sequence out of reach of the command.
        rubiks::app::stop_playback();
        REQUIRE(rubiks::app::is_busy());

        settle();
        REQUIRE(rubiks::app::timeline_cursor() == 6);
    }

    SECTION("a watched pattern is stopped by its own command")
    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

        REQUIRE(rubiks::app::ambient_start(0));
        for (int frame = 0; frame < 40; ++frame) {
            static_cast<void>(rubiks::app::advance(kFrameMs));
        }

        // Stopping it this way would leave the borrowed cube where the pattern
        // had got to, with nothing to put it back.
        rubiks::app::stop_playback();
        REQUIRE(rubiks::app::is_ambient());

        rubiks::app::ambient_stop();
        REQUIRE(rubiks::app::is_solved());
    }
}

TEST_CASE("a rewind is refused while anything else owns the cube")
{
    SECTION("a scramble playing")
    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

        arrive(13U, 4U);
        turn(rubiks::cube::Face::Right, 1);
        REQUIRE(rubiks::app::scramble(14U, 4U));

        REQUIRE_FALSE(rubiks::app::undo());
        REQUIRE_FALSE(rubiks::app::redo());
        REQUIRE_FALSE(rubiks::app::solve_rewind());
        settle();
    }

    SECTION("a turn still settling")
    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

        arrive(13U, 4U);
        turn(rubiks::cube::Face::Right, 1);

        REQUIRE(rubiks::app::turn_face(rubiks::cube::Face::Up, 1));
        REQUIRE_FALSE(rubiks::app::undo());
        REQUIRE_FALSE(rubiks::app::solve_rewind());
        settle();
        REQUIRE(rubiks::app::committed_move_count() == 2);
    }

    SECTION("a layer held under the pointer")
    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

        arrive(13U, 4U);
        turn(rubiks::cube::Face::Right, 1);

        const auto grip = net_cell_point(rubiks::cube::Face::Front, 1, 0);
        REQUIRE(rubiks::app::pointer_down(grip.x, grip.y));
        REQUIRE_FALSE(rubiks::app::undo());
        REQUIRE_FALSE(rubiks::app::solve_rewind());

        rubiks::app::pointer_cancel();
        REQUIRE(rubiks::app::undo());
        settle();
        REQUIRE(rubiks::app::committed_move_count() == 0);
    }

    SECTION("a pattern being watched")
    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

        arrive(13U, 4U);
        turn(rubiks::cube::Face::Right, 1);
        REQUIRE(rubiks::app::ambient_start(1));

        REQUIRE_FALSE(rubiks::app::undo());
        REQUIRE_FALSE(rubiks::app::redo());
        REQUIRE_FALSE(rubiks::app::solve_rewind());

        rubiks::app::ambient_stop();
        REQUIRE(rubiks::app::undo());
    }
}

TEST_CASE("the user's count is read off the record rather than counted")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    arrive(17U, 3U);
    for (int move = 0; move < 5; ++move) {
        turn(rubiks::cube::Face::Right, 1);
        turn(rubiks::cube::Face::Up, 1);
    }
    REQUIRE(rubiks::app::committed_move_count() == 10);

    // A rewind is played back, so a counter kept at the commit would never
    // hear about these two and would still be saying ten.
    REQUIRE(rubiks::app::undo());
    settle();
    REQUIRE(rubiks::app::undo());
    settle();

    REQUIRE(rubiks::app::committed_move_count() == 8);
    REQUIRE(rubiks::app::timeline_length() == 13);
    REQUIRE(rubiks::app::timeline_cursor() == 11);

    // And a new move here cuts the two replayable ones away, which a counter
    // would need a second correction for.
    turn(rubiks::cube::Face::Front, 1);
    REQUIRE(rubiks::app::committed_move_count() == 9);
    REQUIRE(rubiks::app::timeline_length() == 12);
}

TEST_CASE("a turn confirmed by the next press still reaches the record")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    REQUIRE(rubiks::app::turn_face(rubiks::cube::Face::Right, 1));

    // One frame, so the turn is genuinely still settling when the press lands.
    REQUIRE(rubiks::app::advance(kFrameMs));
    REQUIRE(rubiks::app::timeline_cursor() == 0);

    // The other way a move commits, and it goes through the same function:
    // a record that only watched the frame path would lose this one.
    const auto corner = net_cell_point(rubiks::cube::Face::Front, 1, 0);
    REQUIRE(rubiks::app::pointer_down(corner.x, corner.y));
    REQUIRE(rubiks::app::timeline_cursor() == 1);
    REQUIRE(rubiks::app::timeline_length() == 1);
    REQUIRE(rubiks::app::committed_move_count() == 1);

    rubiks::app::pointer_cancel();
    settle();
    REQUIRE(rubiks::app::timeline_cursor() == 1);
}

TEST_CASE("watching leaves no trace on the record")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    arrive(19U, 5U);
    turn(rubiks::cube::Face::Right, 1);

    REQUIRE(rubiks::app::ambient_start(0));
    for (int frame = 0; frame < 600; ++frame) {
        static_cast<void>(rubiks::app::advance(kFrameMs));
    }
    rubiks::app::ambient_stop();

    // Long past the end of a four-move pattern, so it has plainly gone round
    // several times, and the record is where watching found it. Told to
    // advance, the cursor would have run off the end of the record instead.
    REQUIRE(rubiks::app::timeline_length() == 6);
    REQUIRE(rubiks::app::timeline_cursor() == 6);
    REQUIRE(rubiks::app::timeline_scramble_end() == 5);
    REQUIRE(rubiks::app::committed_move_count() == 1);

    // And the record still describes the cube: taking the one move back leaves
    // the scramble it was made on.
    REQUIRE(rubiks::app::undo());
    settle();
    REQUIRE(rubiks::app::timeline_cursor() == 5);
}

TEST_CASE("a scramble with nothing on top of it is rewound whole")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    arrive(27U, 6U);
    const auto scrambled = drawn_frame();

    REQUIRE(rubiks::app::solve_rewind());
    REQUIRE(cursor_track() == stepping(6, 0));

    REQUIRE(rubiks::app::is_solved());
    REQUIRE(drawn_frame() != scrambled);

    // Every one of them is still there to be replayed, in the order it was
    // made, which lands back on the same cube.
    REQUIRE(rubiks::app::redo());
    settle();
    REQUIRE(rubiks::app::timeline_cursor() == 1);
}

TEST_CASE("a command that makes a new cube throws the rewind away with it")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    arrive(29U, 8U);
    REQUIRE(rubiks::app::solve_rewind());

    for (int frame = 0; frame < 20; ++frame) {
        static_cast<void>(rubiks::app::advance(kFrameMs));
    }

    // The same discard a scramble uses, with nothing added to its list: what a
    // rewind leaves behind is a player, and that is already on it.
    rubiks::app::reset_cube();
    REQUIRE_FALSE(rubiks::app::is_busy());
    REQUIRE(rubiks::app::is_solved());
    REQUIRE(rubiks::app::timeline_length() == 0);
    REQUIRE(rubiks::app::timeline_cursor() == 0);

    // A turn of the abandoned rewind that was still in flight does not land on
    // the fresh cube some frames later.
    settle();
    REQUIRE(rubiks::app::is_solved());
    REQUIRE(rubiks::app::timeline_cursor() == 0);
    REQUIRE(rubiks::app::committed_move_count() == 0);
}

TEST_CASE("the record is read back one packed move at a time")
{
    // Nothing has been initialized here, so there is no record: the same word
    // an index past the end gives, because there is one answer for "no move at
    // that index" and no engine at all is a case of it.
    REQUIRE(rubiks::app::timeline_move(0) == 0);

    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);
    REQUIRE(rubiks::app::timeline_move(0) == 0);

    turn(rubiks::cube::Face::Right, 1);

    // Axis X, one clockwise quarter, the outer layer -- the same word the
    // reader on the other side is fixed to.
    REQUIRE(rubiks::app::timeline_move(0) == 0x44);
    REQUIRE(rubiks::app::timeline_move(1) == 0);
    REQUIRE(rubiks::app::timeline_move(0xffffffffU) == 0);

    // A double on a negative face is made by turning the negative way round,
    // and comes back written as a half turn with no direction to it. What the
    // record kept is the move as it was played; that is the timeline's own
    // contract, and it is checked where the timeline is.
    turn(rubiks::cube::Face::Left, 2);
    REQUIRE(rubiks::app::timeline_move(1) == 0x18);
}

TEST_CASE("a scramble is readable from the record the moment it is accepted")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    constexpr std::uint32_t kMoves = 6;
    REQUIRE(rubiks::app::scramble(31U, kMoves));

    // Every move of it is there before any of it has been turned: the record
    // holds the whole sequence and the cursor is what has arrived.
    REQUIRE(rubiks::app::timeline_cursor() == 0);
    for (std::uint32_t index = 0; index < kMoves; ++index) {
        REQUIRE(rubiks::app::timeline_move(index) != 0);
    }
    REQUIRE(rubiks::app::timeline_move(kMoves) == 0);

    settle();

    // A user move goes on the end and is read through the same query; which
    // stretch an index belongs to is the scramble boundary's to say.
    turn(rubiks::cube::Face::Up, 1);
    REQUIRE(rubiks::app::timeline_scramble_end() == kMoves);
    REQUIRE(rubiks::app::timeline_move(kMoves) == 0x45);
    REQUIRE(rubiks::app::timeline_move(kMoves + 1) == 0);
}
