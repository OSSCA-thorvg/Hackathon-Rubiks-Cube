#include "app/Application.hpp"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "EngineLifecycle.hpp"
#include "cube/Cubie.hpp"

// The solve command: what it writes into the record, what it refuses, and
// what is left behind when it is broken off part way. What the solver itself
// does to a cube is checked in tests/solver, which links it without the
// application around it.

namespace {

using rubiks::test::arrive;
using rubiks::test::EngineLifecycle;
using rubiks::test::kFrameMs;
using rubiks::test::settle;
using rubiks::test::turn;

constexpr std::uint32_t kCanvas = 512;

/** The record as one list of packed words, for comparing two moments. */
[[nodiscard]] std::vector<std::uint32_t> record()
{
    std::vector<std::uint32_t> words;
    for (std::uint32_t i = 0; i < rubiks::app::timeline_length(); ++i) {
        words.push_back(rubiks::app::timeline_move(i));
    }
    return words;
}

/** Runs frames until `count` more moves have landed on the cube. */
void play(std::uint32_t count)
{
    const auto target = rubiks::app::timeline_cursor() + count;
    int frames = 0;
    while (rubiks::app::timeline_cursor() < target) {
        REQUIRE(rubiks::app::advance(kFrameMs));
        REQUIRE(++frames < 2000);
    }
}

}  // namespace

TEST_CASE("a solver is there for the cube it was built for")
{
    const EngineLifecycle engine(kCanvas, kCanvas);

    CHECK(rubiks::app::can_solve());

    REQUIRE(rubiks::app::set_cube_size(2));
    CHECK(rubiks::app::can_solve());

    // Every size this application builds, which is what it took two solvers
    // to say: the first handles the two smallest and the second turns the
    // rest into the second smallest.
    for (const int size : {4, 5, 6, 7, 8, 9}) {
        REQUIRE(rubiks::app::set_cube_size(size));
        INFO("size " << size);
        CHECK(rubiks::app::can_solve());
    }

    REQUIRE(rubiks::app::set_cube_size(3));
    CHECK(rubiks::app::can_solve());
}

TEST_CASE("outside a lifecycle nothing can be solved")
{
    CHECK_FALSE(rubiks::app::can_solve());
    CHECK_FALSE(rubiks::app::solve());
}

TEST_CASE("a solve leaves a solved cube and a record that agrees with it")
{
    const EngineLifecycle engine(kCanvas, kCanvas);
    arrive(7, 20);

    REQUIRE(rubiks::app::solve());

    // The whole solution is written down before any of it is played: the
    // record grows at once and the cursor walks up it a move at a time.
    const auto length = rubiks::app::timeline_length();
    REQUIRE(length > 20);
    CHECK(rubiks::app::timeline_cursor() == 20);

    settle();

    CHECK(rubiks::app::is_solved());
    CHECK(rubiks::app::timeline_length() == length);
    CHECK(rubiks::app::timeline_cursor() == length);
    CHECK(rubiks::app::timeline_scramble_end() == 20);
    CHECK(rubiks::app::committed_move_count() == length - 20);
}

TEST_CASE("a solve is refused when there is nothing for it to do")
{
    const EngineLifecycle engine(kCanvas, kCanvas);

    SECTION("a cube already solved")
    {
        const auto before = record();
        CHECK_FALSE(rubiks::app::solve());
        CHECK(record() == before);
        CHECK(rubiks::app::timeline_cursor() == 0);
    }

    SECTION("a cube something else is turning")
    {
        REQUIRE(rubiks::app::scramble(3, 20));

        const auto before = record();
        CHECK_FALSE(rubiks::app::solve());
        CHECK(record() == before);
    }

    SECTION("a cube being watched")
    {
        REQUIRE(rubiks::app::ambient_start(0));
        CHECK_FALSE(rubiks::app::solve());
    }
}

TEST_CASE("a solve broken off leaves the rest of it to redo")
{
    const EngineLifecycle engine(kCanvas, kCanvas);
    arrive(11, 20);

    REQUIRE(rubiks::app::solve());
    const auto length = rubiks::app::timeline_length();

    play(5);
    rubiks::app::stop_playback();

    // Where it stopped, the record and the cube say the same thing: the
    // cursor is what has been turned, and the rest is still written above it.
    const auto stopped = rubiks::app::timeline_cursor();
    CHECK(stopped >= 25);
    CHECK(stopped < length);
    CHECK(rubiks::app::timeline_length() == length);
    CHECK_FALSE(rubiks::app::is_solved());
    CHECK_FALSE(rubiks::app::is_busy());

    // A redo takes the solution up again exactly one move at a time.
    REQUIRE(rubiks::app::redo());
    settle();
    CHECK(rubiks::app::timeline_cursor() == stopped + 1);

    // And the rest of it plays out to a solved cube.
    while (rubiks::app::timeline_cursor() < length) {
        REQUIRE(rubiks::app::redo());
        settle();
    }
    CHECK(rubiks::app::is_solved());
}

TEST_CASE("a move of the user's own throws away the rest of a stopped solve")
{
    const EngineLifecycle engine(kCanvas, kCanvas);
    arrive(5, 20);

    REQUIRE(rubiks::app::solve());
    play(3);
    rubiks::app::stop_playback();

    const auto stopped = rubiks::app::timeline_cursor();
    turn(rubiks::cube::Face::Right, 1);

    // The same cut a move after any rewind makes: what was above the cursor
    // was not gone through, so it is not part of what happened.
    CHECK(rubiks::app::timeline_length() == stopped + 1);
    CHECK(rubiks::app::timeline_cursor() == stopped + 1);
}

TEST_CASE("a solve can be taken back move by move afterwards")
{
    const EngineLifecycle engine(kCanvas, kCanvas);
    arrive(13, 20);

    REQUIRE(rubiks::app::solve());
    settle();
    REQUIRE(rubiks::app::is_solved());

    REQUIRE(rubiks::app::undo());
    settle();
    CHECK_FALSE(rubiks::app::is_solved());

    // A rewind still reaches all the way back, solver moves and scramble
    // together -- it reads the record and does not ask who wrote it.
    REQUIRE(rubiks::app::solve_rewind());
    settle();
    CHECK(rubiks::app::timeline_cursor() == 0);
    CHECK(rubiks::app::is_solved());
}

TEST_CASE("a solved cube can be shared and brought back")
{
    const EngineLifecycle engine(kCanvas, kCanvas);
    arrive(17, 20);

    REQUIRE(rubiks::app::solve());
    settle();

    const auto words = record();
    const auto scramble_end = rubiks::app::timeline_scramble_end();
    const auto total = static_cast<std::uint32_t>(words.size());

    // Every word a solve wrote is one the payload can carry: a solution is
    // outer face turns, which is the mask set a shared record is held to.
    const auto address = rubiks::app::restore_buffer(total);
    REQUIRE(address != 0);
    auto* buffer = reinterpret_cast<std::uint32_t*>(address);
    for (std::uint32_t i = 0; i < total; ++i) buffer[i] = words[i];

    REQUIRE(rubiks::app::restore_apply(3, scramble_end, total - scramble_end));
    CHECK(rubiks::app::is_solved());
    CHECK(record() == words);
}
