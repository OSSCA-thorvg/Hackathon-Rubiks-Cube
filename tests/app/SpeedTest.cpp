#include "app/Application.hpp"

#include <cmath>
#include <limits>

#include <catch2/catch_test_macros.hpp>

#include "EngineLifecycle.hpp"
#include "graphics/Layout.hpp"
#include "interaction/InteractionController.hpp"
#include "math/Types.hpp"

// How fast everything turns, as one multiplier. What is checked here is that
// the multiplier reaches both places a tempo is handed over -- a sequence
// being played and a drag being released -- and that a turn already running
// keeps the duration it began with.

namespace {

constexpr std::uint32_t kCanvas = 512;

using rubiks::test::kFrameMs;

/** Counts the frames a sequence or a snap takes to finish. */
int frames_to_settle()
{
    int frames = 0;
    while (rubiks::app::advance(kFrameMs)) {
        ++frames;
        REQUIRE(frames < 10000);
    }
    return frames;
}

/** The center of the front face's right column: the grab that means R. */
rubiks::math::Vec2 front_right_column()
{
    const auto rect = rubiks::graphics::layout(kCanvas, kCanvas,
                                               rubiks::graphics::ViewMode::Both,
                                               rubiks::graphics::FlatStyle::Net)
                          .cube;
    return rubiks::math::Vec2{rect.x + rect.width * 0.433f,
                              rect.y + rect.height * 0.694f};
}

/** How far the pointer moves to turn a layer by one degree. */
float pixels_per_degree()
{
    const auto rect = rubiks::graphics::layout(kCanvas, kCanvas,
                                               rubiks::graphics::ViewMode::Both,
                                               rubiks::graphics::FlatStyle::Net)
                          .cube;
    return rubiks::interaction::kQuarterTurnFraction * rect.width / 90.0f;
}

/**
 * Drags the right column part of the way round and lets go.
 *
 * Part of the way rather than all of it: a release at the full quarter has
 * arrived already, so the settle has no angle left to cover and takes the same
 * handful of frames however fast it is told to run. Fifty degrees is past the
 * boundary that carries a turn forward, so what is left is a real animation.
 */
int frames_for_released_drag()
{
    const auto grab = front_right_column();
    REQUIRE(rubiks::app::pointer_down(grab.x, grab.y));
    rubiks::app::pointer_move(grab.x, grab.y - 50.0f * pixels_per_degree());
    rubiks::app::pointer_up();
    return frames_to_settle();
}

}  // namespace

TEST_CASE("the speed multiplier is one until it is set")
{
    // Before there is an engine at all, so a control reading it during startup
    // gets the value the engine will actually be running at.
    REQUIRE(rubiks::app::speed_scale() == 1.0f);

    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);
    REQUIRE(rubiks::app::speed_scale() == 1.0f);
}

TEST_CASE("out of range speeds are clamped and nonsense is refused")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    REQUIRE(rubiks::app::set_speed_scale(2.0f));
    REQUIRE(rubiks::app::speed_scale() == 2.0f);

    // Clamped rather than refused: a slider that ran past the end still meant
    // "as fast as it goes", and there is nothing to tell the person about.
    REQUIRE(rubiks::app::set_speed_scale(100.0f));
    REQUIRE(rubiks::app::speed_scale() == 4.0f);

    REQUIRE(rubiks::app::set_speed_scale(0.0f));
    REQUIRE(rubiks::app::speed_scale() == 0.25f);

    REQUIRE(rubiks::app::set_speed_scale(-3.0f));
    REQUIRE(rubiks::app::speed_scale() == 0.25f);

    // Not a speed that was too big, but not a speed: clamping either of these
    // would be inventing an answer, and dividing by them stops a turn ending.
    REQUIRE(rubiks::app::set_speed_scale(1.0f));
    REQUIRE_FALSE(
        rubiks::app::set_speed_scale(std::numeric_limits<float>::quiet_NaN()));
    REQUIRE_FALSE(
        rubiks::app::set_speed_scale(std::numeric_limits<float>::infinity()));
    REQUIRE(rubiks::app::speed_scale() == 1.0f);
}

TEST_CASE("a played sequence runs shorter at a higher multiplier")
{
    int at_one = 0;
    int at_two = 0;

    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);
        REQUIRE(rubiks::app::scramble(11U, 8U));
        at_one = frames_to_settle();
    }
    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);
        REQUIRE(rubiks::app::set_speed_scale(2.0f));
        REQUIRE(rubiks::app::scramble(11U, 8U));
        at_two = frames_to_settle();
    }

    // Fewer frames, not exactly half of them: a fixed step lands where it
    // lands, and pinning the ratio would be fixing the rounding rather than
    // the behaviour. The same scramble on both sides, so what differs is the
    // multiplier and nothing else.
    REQUIRE(at_two < at_one);
    REQUIRE(at_two > 0);
}

TEST_CASE("a drag release settles sooner at a higher multiplier")
{
    int at_one = 0;
    int at_two = 0;

    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);
        at_one = frames_for_released_drag();
    }
    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);
        REQUIRE(rubiks::app::set_speed_scale(2.0f));
        at_two = frames_for_released_drag();
    }

    // The evidence that the multiplier reaches the pointer path and not only
    // the playback loop. Those are two separate places a tempo is handed over,
    // and a change that missed this one would leave every turn a person makes
    // by hand running at the written speed.
    REQUIRE(at_one > 0);
    REQUIRE(at_two < at_one);
}

TEST_CASE("a turn already running keeps the duration it began with")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    REQUIRE(rubiks::app::scramble(7U, 8U));

    // Two frames in, so a move is under way rather than about to begin.
    REQUIRE(rubiks::app::advance(kFrameMs));
    REQUIRE(rubiks::app::advance(kFrameMs));

    // Accepted mid-sequence, and the frames that follow still arrive: nothing
    // is re-timed under a slider being dragged, and the sequence finishes.
    REQUIRE(rubiks::app::set_speed_scale(4.0f));
    REQUIRE(rubiks::app::speed_scale() == 4.0f);

    const int rest = frames_to_settle();
    REQUIRE(rest > 0);
    REQUIRE_FALSE(rubiks::app::is_busy());
}
