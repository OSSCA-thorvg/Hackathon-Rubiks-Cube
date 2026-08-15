#include "app/Application.hpp"

#include <cstdint>
#include <limits>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "EngineLifecycle.hpp"
#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"

namespace {

using rubiks::test::kFrameMs;
using rubiks::test::settle;

/** Runs a fixed number of frames, for watching, which never settles. */
void run_frames(int count)
{
    for (int frame = 0; frame < count; ++frame) {
        static_cast<void>(rubiks::app::advance(kFrameMs));
    }
}

/** A copy of the frame just drawn, so two of them can be compared. */
[[nodiscard]] std::vector<std::uint8_t> drawn_frame()
{
    REQUIRE(rubiks::app::render());

    const auto* pixels =
        reinterpret_cast<const std::uint8_t*>(rubiks::app::pixel_buffer());
    const std::uint32_t length = rubiks::app::pixel_byte_length();
    REQUIRE(pixels != nullptr);
    REQUIRE(length > 0);

    return std::vector<std::uint8_t>(pixels, pixels + length);
}

/**
 * Frames enough for several moves of a pattern to have been turned.
 *
 * Long enough that the cube has visibly left where it started, short enough
 * that no pattern could have come all the way back round to it.
 */
constexpr int kFramesIntoPattern = 200;

}  // namespace

TEST_CASE("every watching pattern brings the cube back round within the bound")
{
    using rubiks::app::ambient_pattern;
    using rubiks::app::kAmbientMaxPeriod;
    using rubiks::app::kAmbientPatternCount;

    // Arithmetic rather than animation: what is being checked is a property of
    // the moves in the table, and playing them into an application to find it
    // out would only add a few thousand frames between the question and the
    // answer. The orders themselves stay in the phase document -- this holds
    // whatever they are, which is what makes it a guard on the next edit.
    for (std::uint32_t choice = 0; choice < kAmbientPatternCount; ++choice) {
        const auto& pattern = ambient_pattern(choice);
        REQUIRE_FALSE(pattern.empty());

        rubiks::cube::CubeState cube(3);
        int rounds = 0;
        while (rounds < kAmbientMaxPeriod) {
            cube.apply(pattern);
            ++rounds;
            if (cube.is_solved()) break;
        }

        REQUIRE(cube.is_solved());

        // And it is a pattern rather than a shuffle of nothing: one round of
        // it leaves the cube somewhere else.
        REQUIRE(rounds > 1);
    }
}

TEST_CASE("a choice picks a pattern by remainder, so every value is one")
{
    using rubiks::app::ambient_pattern;
    using rubiks::app::kAmbientPatternCount;

    // No rejection path and no size to ask for first: the largest value the
    // boundary can carry names a pattern just as the smallest does.
    REQUIRE(ambient_pattern(kAmbientPatternCount) == ambient_pattern(0));
    REQUIRE(ambient_pattern(0xffffffffU) ==
            ambient_pattern(0xffffffffU % kAmbientPatternCount));
}

TEST_CASE("watching repeats without end and stays nobody's")
{
    const rubiks::test::EngineLifecycle engine(256, 256);

    REQUIRE_FALSE(rubiks::app::is_ambient());
    REQUIRE(rubiks::app::ambient_start(0));
    REQUIRE(rubiks::app::is_ambient());

    // Already watching: a second call changes nothing rather than restarting.
    REQUIRE_FALSE(rubiks::app::ambient_start(1));

    // Busy from the moment it is accepted, as a scramble is, and asking for
    // frames on every one of them -- a sequence that never runs out is a frame
    // loop that never stops.
    REQUIRE(rubiks::app::is_busy());
    for (int frame = 0; frame < 1000; ++frame) {
        REQUIRE(rubiks::app::advance(kFrameMs));
    }

    // Long past the end of a four-move pattern, so it has plainly gone round
    // again, and nothing it turned belongs to anybody.
    REQUIRE_FALSE(rubiks::app::is_solved());
    REQUIRE(rubiks::app::committed_move_count() == 0);
    REQUIRE(rubiks::app::is_ambient());
}

TEST_CASE("a scramble left to itself does run out")
{
    const rubiks::test::EngineLifecycle engine(256, 256);

    // The other side of the repeat: the field defaults to off, so the producer
    // that was here before watching was added still ends where it always did.
    REQUIRE(rubiks::app::scramble(5U, 3U));
    REQUIRE_FALSE(rubiks::app::is_ambient());

    settle();
    REQUIRE_FALSE(rubiks::app::is_busy());
}

TEST_CASE("stopping puts back the cube and the count from before watching")
{
    const rubiks::test::EngineLifecycle engine(256, 256);

    REQUIRE(rubiks::app::scramble(42U, 6U));
    settle();
    const auto scrambled = drawn_frame();

    REQUIRE(rubiks::app::turn_face(rubiks::cube::Face::Right, 1));
    settle();
    REQUIRE(rubiks::app::committed_move_count() == 1);
    const auto before = drawn_frame();

    REQUIRE(rubiks::app::ambient_start(2));
    run_frames(kFramesIntoPattern);
    REQUIRE(drawn_frame() != before);

    rubiks::app::ambient_stop();
    REQUIRE_FALSE(rubiks::app::is_ambient());
    REQUIRE_FALSE(rubiks::app::is_busy());

    // The interlude left nothing of itself: the same cube, the same count, and
    // a turn of the pattern that was in flight did not land afterwards.
    REQUIRE(rubiks::app::committed_move_count() == 1);
    settle();
    REQUIRE(rubiks::app::committed_move_count() == 1);

    // Down to the pixel, which is also the viewpoint saying it never moved.
    REQUIRE(drawn_frame() == before);

    // A cube the user can go on turning, from exactly where they left it:
    // taking that one turn back lands on the scramble it was made from.
    REQUIRE(rubiks::app::turn_face(rubiks::cube::Face::Right, -1));
    settle();
    REQUIRE(drawn_frame() == scrambled);
    REQUIRE(rubiks::app::committed_move_count() == 2);
}

TEST_CASE("stopping when nothing is being watched does nothing")
{
    const rubiks::test::EngineLifecycle engine(256, 256);

    REQUIRE(rubiks::app::scramble(7U, 4U));
    rubiks::app::ambient_stop();

    // The scramble is not a pattern being watched, so it is still playing.
    REQUIRE(rubiks::app::is_busy());
    settle();
    REQUIRE_FALSE(rubiks::app::is_solved());
}

TEST_CASE("a drag looks around a watched pattern without interrupting it")
{
    const rubiks::test::EngineLifecycle engine(256, 256);

    REQUIRE(rubiks::app::ambient_start(0));
    run_frames(kFramesIntoPattern);

    // Over the middle of the cube, which at any other moment is a grip on a
    // layer. It sweeps the viewpoint instead, exactly as it does while a
    // scramble plays, and the pattern goes on turning underneath it.
    REQUIRE(rubiks::app::pointer_down(128.0f, 60.0f));
    rubiks::app::pointer_move(80.0f, 60.0f);
    rubiks::app::pointer_up();
    REQUIRE(rubiks::app::is_ambient());
    REQUIRE(rubiks::app::advance(kFrameMs));

    // The viewpoint moved and nothing else did: stopping now gives back the
    // cube that was there, from the angle the user left it at.
    const auto swept = drawn_frame();
    rubiks::app::ambient_stop();
    REQUIRE(rubiks::app::is_solved());
    REQUIRE(rubiks::app::committed_move_count() == 0);

    rubiks::app::reset_view();
    REQUIRE(drawn_frame() != swept);
}

TEST_CASE("a press with no cube on screen is refused, watched or not")
{
    const rubiks::test::EngineLifecycle engine(256, 256);

    REQUIRE(rubiks::app::ambient_start(0));

    // A coordinate that is not a place, refused before anything is changed
    // the way every other malformed press is.
    REQUIRE_FALSE(rubiks::app::pointer_down(
        std::numeric_limits<float>::quiet_NaN(), 60.0f));
    REQUIRE(rubiks::app::is_ambient());

    // And with the cube itself off screen there is no viewpoint to sweep, so
    // the press has nothing to start -- the same answer a scramble gives.
    REQUIRE(rubiks::app::set_view_mode(rubiks::graphics::ViewMode::Flat));
    REQUIRE_FALSE(rubiks::app::pointer_down(128.0f, 128.0f));
    REQUIRE(rubiks::app::is_ambient());
}

TEST_CASE("commands that make a new cube end watching without putting one back")
{
    SECTION("reset")
    {
        const rubiks::test::EngineLifecycle engine(256, 256);
        REQUIRE(rubiks::app::scramble(3U, 5U));
        settle();

        REQUIRE(rubiks::app::ambient_start(1));
        run_frames(kFramesIntoPattern);

        rubiks::app::reset_cube();
        REQUIRE_FALSE(rubiks::app::is_ambient());
        REQUIRE_FALSE(rubiks::app::is_busy());

        // The scrambled cube the watching was taken from is not what comes
        // back. The command's own cube is, and the pattern's last turn does
        // not land on it a few frames later either.
        REQUIRE(rubiks::app::is_solved());
        settle();
        REQUIRE(rubiks::app::is_solved());
        REQUIRE(rubiks::app::committed_move_count() == 0);
    }

    SECTION("scramble")
    {
        const rubiks::test::EngineLifecycle engine(256, 256);
        REQUIRE(rubiks::app::ambient_start(3));
        run_frames(kFramesIntoPattern);

        REQUIRE(rubiks::app::scramble(9U, 5U));
        REQUIRE_FALSE(rubiks::app::is_ambient());

        // Turned into a solved cube, as a scramble always is, rather than into
        // whatever the pattern had reached.
        REQUIRE(rubiks::app::is_solved());
        settle();
        REQUIRE_FALSE(rubiks::app::is_solved());
        REQUIRE_FALSE(rubiks::app::is_busy());
    }

    SECTION("shutdown")
    {
        REQUIRE(rubiks::app::initialize(256, 256));
        REQUIRE(rubiks::app::ambient_start(0));
        run_frames(kFramesIntoPattern);

        rubiks::app::shutdown();
        REQUIRE_FALSE(rubiks::app::is_ambient());

        // And the next lifecycle starts on a cube of its own.
        const rubiks::test::EngineLifecycle engine(256, 256);
        REQUIRE(rubiks::app::is_solved());
        REQUIRE_FALSE(rubiks::app::is_ambient());
    }
}

TEST_CASE("watching outlives a resize and a change of view")
{
    using rubiks::graphics::FlatStyle;
    using rubiks::graphics::ViewMode;

    const rubiks::test::EngineLifecycle engine(256, 256);

    REQUIRE(rubiks::app::ambient_start(0));
    run_frames(kFramesIntoPattern);

    // Looking at it another way is not input into the cube, so the pattern
    // carries on where it was -- the same as a scramble part way through.
    REQUIRE(rubiks::app::resize(320, 200));
    REQUIRE(rubiks::app::set_view_mode(ViewMode::Flat));
    REQUIRE(rubiks::app::set_flat_style(FlatStyle::Rings));
    REQUIRE(rubiks::app::is_ambient());
    REQUIRE(rubiks::app::advance(kFrameMs));
    REQUIRE(rubiks::app::render());

    rubiks::app::ambient_stop();
    REQUIRE(rubiks::app::is_solved());
}

TEST_CASE("watching before initialization is refused and asked about safely")
{
    REQUIRE_FALSE(rubiks::app::is_initialized());
    REQUIRE_FALSE(rubiks::app::ambient_start(0));
    REQUIRE_FALSE(rubiks::app::is_ambient());
    rubiks::app::ambient_stop();
    REQUIRE_FALSE(rubiks::app::is_ambient());
}
