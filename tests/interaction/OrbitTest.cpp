#include <cstdint>
#include <limits>
#include <optional>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "graphics/Layout.hpp"
#include "graphics/OrbitCamera.hpp"
#include "interaction/InteractionController.hpp"

namespace {

using Catch::Approx;
using namespace rubiks::interaction;
using rubiks::graphics::Camera;
using rubiks::graphics::Rect;

constexpr int kSize = 3;
constexpr std::uint32_t kCanvas = 1024;
constexpr double kFrameMs = 16.0;

Rect cube_rect()
{
    return rubiks::graphics::layout(kCanvas, kCanvas).cube;
}

Camera cube_camera()
{
    const Rect rect = cube_rect();
    return rubiks::graphics::default_camera(rect.width / rect.height);
}

/** A point in the corner of the viewport, well clear of the cube. */
float corner_x()
{
    const Rect rect = cube_rect();
    return rect.x + rect.width * 0.02f;
}

float corner_y()
{
    const Rect rect = cube_rect();
    return rect.y + rect.height * 0.02f;
}

/** Presses the background, which is what asks to look around the cube. */
void press_background(InteractionController& controller)
{
    REQUIRE(controller.pointer_down(corner_x(), corner_y(), cube_camera(),
                                    cube_rect()));
}

/** Pixels of drag that sweep the viewpoint by one quarter turn. */
float quarter_turn_pixels()
{
    return kOrbitQuarterTurnFraction * cube_rect().width;
}

}  // namespace

TEST_CASE("pressing the background begins a gesture")
{
    InteractionController controller(kSize);

    // Missing the cube is no longer nothing: it starts a viewpoint sweep, so
    // the browser still takes pointer capture and starts drawing frames.
    press_background(controller);
    REQUIRE(controller.advance(kFrameMs));

    // Nothing about the cube is involved, though.
    REQUIRE_FALSE(controller.active_rotation());
    REQUIRE_FALSE(controller.take_committed_move());
}

TEST_CASE("a drag sweeps the viewpoint the way the cube would follow")
{
    InteractionController controller(kSize);
    press_background(controller);

    // Dragging left pushes the visible face away and brings the right-hand
    // face round to meet it, which is the positive yaw direction.
    controller.pointer_move(corner_x() - quarter_turn_pixels(), corner_y());

    const auto swept = controller.take_orbit_delta();
    REQUIRE(swept);
    REQUIRE(swept->yaw_degrees == Approx(90.0f));
    REQUIRE(swept->pitch_degrees == Approx(0.0f).margin(1e-4f));
}

TEST_CASE("dragging downward tips the top of the cube towards the viewer")
{
    InteractionController controller(kSize);
    press_background(controller);

    controller.pointer_move(corner_x(), corner_y() + quarter_turn_pixels());

    const auto swept = controller.take_orbit_delta();
    REQUIRE(swept);
    REQUIRE(swept->pitch_degrees == Approx(90.0f));
    REQUIRE(swept->yaw_degrees == Approx(0.0f).margin(1e-4f));
}

TEST_CASE("dragging the other way sweeps the other way")
{
    InteractionController controller(kSize);
    press_background(controller);

    controller.pointer_move(corner_x() + quarter_turn_pixels(),
                            corner_y() - quarter_turn_pixels());

    const auto swept = controller.take_orbit_delta();
    REQUIRE(swept);
    REQUIRE(swept->yaw_degrees == Approx(-90.0f));
    REQUIRE(swept->pitch_degrees == Approx(-90.0f));
}

TEST_CASE("moves between two frames add up instead of replacing each other")
{
    InteractionController controller(kSize);
    press_background(controller);

    // Pointer events arrive more often than frames, so three steps of a third
    // have to come out the same as one whole step.
    const float step = quarter_turn_pixels() / 3.0f;
    controller.pointer_move(corner_x() - step, corner_y());
    controller.pointer_move(corner_x() - step * 2.0f, corner_y());
    controller.pointer_move(corner_x() - step * 3.0f, corner_y());

    const auto swept = controller.take_orbit_delta();
    REQUIRE(swept);
    REQUIRE(swept->yaw_degrees == Approx(90.0f).margin(1e-3f));
}

TEST_CASE("a sweep is taken exactly once")
{
    InteractionController controller(kSize);
    press_background(controller);
    controller.pointer_move(corner_x() - quarter_turn_pixels(), corner_y());

    REQUIRE(controller.take_orbit_delta());
    REQUIRE_FALSE(controller.take_orbit_delta());

    // And a later move starts a fresh delta rather than repeating the old one.
    controller.pointer_move(corner_x() - quarter_turn_pixels() * 2.0f,
                            corner_y());
    const auto again = controller.take_orbit_delta();
    REQUIRE(again);
    REQUIRE(again->yaw_degrees == Approx(90.0f));
}

TEST_CASE("the last sweep outlives the gesture that made it")
{
    // Releasing or cancelling between a move and the next frame must not lose
    // the movement: a viewpoint has nothing to undo.
    for (const bool release : {true, false}) {
        InteractionController controller(kSize);
        press_background(controller);
        controller.pointer_move(corner_x() - quarter_turn_pixels(),
                                corner_y());

        if (release) {
            controller.pointer_up();
        } else {
            controller.cancel();
        }

        const auto swept = controller.take_orbit_delta();
        REQUIRE(swept);
        REQUIRE(swept->yaw_degrees == Approx(90.0f));

        // The gesture itself is over.
        REQUIRE_FALSE(controller.advance(kFrameMs));
    }
}

TEST_CASE("a reset drops a pending sweep along with everything else")
{
    InteractionController controller(kSize);
    press_background(controller);
    controller.pointer_move(corner_x() - quarter_turn_pixels(), corner_y());

    controller.reset();

    REQUIRE_FALSE(controller.take_orbit_delta());
    REQUIRE_FALSE(controller.advance(kFrameMs));
}

TEST_CASE("a second pointer during an orbit is ignored")
{
    InteractionController controller(kSize);
    press_background(controller);

    REQUIRE_FALSE(controller.pointer_down(corner_x(), corner_y(),
                                          cube_camera(), cube_rect()));
}

TEST_CASE("non-finite coordinates cannot poison the viewpoint")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();

    InteractionController controller(kSize);
    press_background(controller);

    controller.pointer_move(nan, corner_y());
    REQUIRE_FALSE(controller.take_orbit_delta());

    // The gesture survives, still measuring from where the pointer really is.
    controller.pointer_move(corner_x() - quarter_turn_pixels(), corner_y());
    const auto swept = controller.take_orbit_delta();
    REQUIRE(swept);
    REQUIRE(swept->yaw_degrees == Approx(90.0f));
}

TEST_CASE("an orbit never turns a layer")
{
    InteractionController controller(kSize);
    press_background(controller);

    controller.pointer_move(corner_x() - quarter_turn_pixels() * 3.0f,
                            corner_y() + quarter_turn_pixels());
    REQUIRE_FALSE(controller.active_rotation());

    controller.pointer_up();
    REQUIRE_FALSE(controller.take_committed_move());
    REQUIRE_FALSE(controller.active_rotation());
}

TEST_CASE("an orbit is not busy and coexists with a programmatic move")
{
    // A sweep has no commit to protect, so it must block neither a keyboard
    // turn now nor queued playback later. Busy is about changing the cube.
    const auto turn = rubiks::cube::moves::R(kSize);

    InteractionController controller(kSize);
    press_background(controller);
    REQUIRE_FALSE(controller.is_busy());

    REQUIRE(controller.start_move(turn));
    REQUIRE(controller.is_busy());

    // The snap animates while the orbit keeps sweeping underneath it.
    const auto turning = controller.active_rotation();
    REQUIRE(turning);
    REQUIRE(turning->axis == turn.axis);
    REQUIRE(turning->layers == turn.layers);

    controller.pointer_move(corner_x() - quarter_turn_pixels(), corner_y());
    const auto swept = controller.take_orbit_delta();
    REQUIRE(swept);
    REQUIRE(swept->yaw_degrees == Approx(90.0f));

    // The move still commits exactly once, untouched by the sweep.
    std::optional<rubiks::cube::CubeMove> committed;
    for (int frame = 0; frame < 100 && !committed; ++frame) {
        REQUIRE(controller.advance(kFrameMs));
        committed = controller.take_committed_move();
    }
    REQUIRE(committed);
    REQUIRE(*committed == turn);
    REQUIRE_FALSE(controller.take_committed_move());
}
