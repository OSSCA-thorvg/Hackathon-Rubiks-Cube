#include "interaction/InteractionController.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "cube/CubeState.hpp"
#include "graphics/CubeGeometry.hpp"
#include "graphics/Layout.hpp"
#include "interaction/DragResolver.hpp"

namespace {

using Catch::Approx;
using namespace rubiks::interaction;
using rubiks::cube::Axis;
using rubiks::cube::CubeMove;
using rubiks::cube::CubeState;
using rubiks::cube::Face;
using rubiks::graphics::Camera;
using rubiks::graphics::Rect;
using rubiks::math::Vec2;
using rubiks::math::Vec3;

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

/** Where a world point lands in viewport pixels. */
Vec2 screen_of(const Vec3& world)
{
    const auto point = project_to_screen(world, cube_camera(), cube_rect());
    REQUIRE(point);
    return *point;
}

/** Pixels of drag along a locked direction that add up to one degree. */
float pixels_per_degree()
{
    return kQuarterTurnFraction * cube_rect().width / 90.0f;
}

/** The screen direction a positive turn about `axis` carries `grab` along. */
Vec2 direction_for(Axis axis, const Vec3& grab)
{
    const auto direction = turn_direction(axis, grab, cube_camera(),
                                          cube_rect());
    REQUIRE(direction);
    return *direction;
}

/** Presses at a world point, which must be on the surface of the cube. */
void press(InteractionController& controller, const Vec3& grab)
{
    const Vec2 at = screen_of(grab);
    REQUIRE(controller.pointer_down(at.x, at.y, cube_camera(), cube_rect()));
}

/** Drags from the press point far enough to turn by `degrees`. */
void drag(InteractionController& controller, const Vec3& grab,
          const Vec2& direction, float degrees)
{
    const Vec2 from = screen_of(grab);
    const float pixels = degrees * pixels_per_degree();
    controller.pointer_move(from.x + direction.x * pixels,
                            from.y + direction.y * pixels);
}

struct Settled {
    int frames = 0;
    int commits = 0;
    std::optional<CubeMove> move;
};

/** Runs the animation to a standstill, collecting whatever it commits. */
Settled settle(InteractionController& controller)
{
    Settled result;

    while (controller.advance(kFrameMs)) {
        if (const auto move = controller.take_committed_move()) {
            ++result.commits;
            result.move = move;
        }

        ++result.frames;
        REQUIRE(result.frames < 1000);
    }

    return result;
}

/** Press, drag, release and settle: one whole gesture. */
Settled perform(const Vec3& grab, const Vec2& direction, float degrees)
{
    InteractionController controller(kSize);
    press(controller, grab);
    drag(controller, grab, direction, degrees);
    controller.pointer_up();
    return settle(controller);
}

float dot(const Vec2& a, const Vec2& b)
{
    return a.x * b.x + a.y * b.y;
}

// The centers of the three faces a fixed camera can reach. World coordinates,
// on the surface of a cube of half extent one.
constexpr Vec3 kUpCenter{0.0f, 1.0f, 0.0f};
constexpr Vec3 kFrontCenter{0.0f, 0.0f, 1.0f};
constexpr Vec3 kRightCenter{1.0f, 0.0f, 0.0f};

/** One grabbable face paired with one of the two axes it can turn about. */
struct Handle {
    const char* name;
    Vec3 grab;
    Axis axis;
    /** Where a positive turn should carry the grabbed point, on screen. */
    Vec2 expected;
};

// Screen directions with x to the right and y downwards, worked out from the
// camera basis: screen right is world (X - Z) and screen up is world
// (2Y - X - Z), both normalized. A sign error anywhere between the domain
// convention and the viewport would turn one of these around.
const std::array<Handle, 6> kHandles{{
    {"up face, X axis", kUpCenter, Axis::X, Vec2{0.866f, -0.5f}},
    {"up face, Z axis", kUpCenter, Axis::Z, Vec2{0.866f, 0.5f}},
    {"front face, X axis", kFrontCenter, Axis::X, Vec2{0.0f, -1.0f}},
    {"front face, Y axis", kFrontCenter, Axis::Y, Vec2{-0.866f, -0.5f}},
    {"right face, Y axis", kRightCenter, Axis::Y, Vec2{-0.866f, 0.5f}},
    {"right face, Z axis", kRightCenter, Axis::Z, Vec2{0.0f, 1.0f}},
}};

}  // namespace

TEST_CASE("a positive turn moves the grabbed point where the camera shows it")
{
    for (const auto& handle : kHandles) {
        INFO(handle.name);
        const Vec2 actual = direction_for(handle.axis, handle.grab);

        // Within about 25 degrees of the direction worked out by hand, which
        // is far tighter than any sign mistake could survive.
        REQUIRE(dot(actual, handle.expected) > 0.9f);
    }
}

TEST_CASE("dragging the front face's right column upward performs R")
{
    // The anchor that ties screen space to the domain: this is the gesture a
    // person makes to turn the right-hand layer, and R is what it must mean.
    const Vec3 grab{rubiks::graphics::cubie_center(2, kSize),
                    rubiks::graphics::cubie_center(1, kSize), 1.0f};

    InteractionController controller(kSize);
    press(controller, grab);
    drag(controller, grab, Vec2{0.0f, -1.0f}, 120.0f);

    const auto turning = controller.active_rotation();
    REQUIRE(turning);
    REQUIRE(turning->axis == Axis::X);
    REQUIRE(turning->layers == rubiks::cube::layer(2));
    REQUIRE(turning->angle_degrees > 0.0f);

    controller.pointer_up();
    const Settled settled = settle(controller);

    REQUIRE(settled.commits == 1);
    REQUIRE(settled.move);
    REQUIRE(*settled.move == rubiks::cube::moves::R(kSize));
}

TEST_CASE("every grabbable face turns both ways about both of its axes")
{
    for (const auto& handle : kHandles) {
        INFO(handle.name);
        const Vec2 direction = direction_for(handle.axis, handle.grab);

        // The center sticker of a face sits in the middle layer of both axes
        // it can turn about.
        const auto middle = rubiks::cube::layer(1);

        const Settled forward = perform(handle.grab, direction, 120.0f);
        REQUIRE(forward.move);
        REQUIRE(*forward.move == CubeMove{handle.axis, middle, 1});

        const Settled backward =
            perform(handle.grab, Vec2{-direction.x, -direction.y}, 120.0f);
        REQUIRE(backward.move);
        REQUIRE(*backward.move == CubeMove{handle.axis, middle, -1});
    }
}

TEST_CASE("a drag inside the dead zone turns nothing")
{
    InteractionController controller(kSize);
    press(controller, kFrontCenter);

    const Vec2 from = screen_of(kFrontCenter);
    const float inside = kDeadZoneFraction * cube_rect().width * 0.5f;
    controller.pointer_move(from.x, from.y - inside);

    REQUIRE_FALSE(controller.active_rotation());

    controller.pointer_up();
    REQUIRE_FALSE(controller.advance(kFrameMs));
    REQUIRE_FALSE(controller.take_committed_move());
}

TEST_CASE("the axis stays locked for the rest of the gesture")
{
    const Vec2 upward = direction_for(Axis::X, kFrontCenter);
    const Vec2 sideways = direction_for(Axis::Y, kFrontCenter);

    InteractionController controller(kSize);
    press(controller, kFrontCenter);
    drag(controller, kFrontCenter, upward, 30.0f);

    const auto locked = controller.active_rotation();
    REQUIRE(locked);
    REQUIRE(locked->axis == Axis::X);

    // Turning the drag towards the other candidate must not move the layer
    // out from under the finger.
    drag(controller, kFrontCenter, sideways, 60.0f);

    const auto still_locked = controller.active_rotation();
    REQUIRE(still_locked);
    REQUIRE(still_locked->axis == Axis::X);
}

TEST_CASE("an undecidable drag resolves deterministically")
{
    // Both candidates score exactly zero here, so only the tie-break decides.
    // The rule matters less than being the same answer every time.
    const Camera camera = cube_camera();
    const Rect rect = cube_rect();

    const auto ray = pointer_ray(screen_of(kUpCenter).x,
                                 screen_of(kUpCenter).y, camera, rect);
    REQUIRE(ray);
    const auto pick = pick_cube(*ray, kSize);
    REQUIRE(pick);

    const auto first = resolve_axis(*pick, Vec2{0.0f, 0.0f}, camera, rect);
    const auto second = resolve_axis(*pick, Vec2{0.0f, 0.0f}, camera, rect);
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(first->axis == second->axis);
    REQUIRE(first->axis == Axis::X);
}

TEST_CASE("pressing away from the cube starts nothing")
{
    InteractionController controller(kSize);
    const Rect rect = cube_rect();

    // A corner of the viewport, outside the silhouette.
    REQUIRE_FALSE(controller.pointer_down(rect.x + rect.width * 0.02f,
                                          rect.y + rect.height * 0.02f,
                                          cube_camera(), rect));
    REQUIRE_FALSE(controller.active_rotation());
    REQUIRE_FALSE(controller.advance(kFrameMs));
}

TEST_CASE("a second pointer during a gesture is ignored")
{
    InteractionController controller(kSize);
    press(controller, kFrontCenter);

    const Vec2 elsewhere = screen_of(kRightCenter);
    REQUIRE_FALSE(controller.pointer_down(elsewhere.x, elsewhere.y,
                                          cube_camera(), cube_rect()));
}

TEST_CASE("input during the snap is ignored")
{
    const Vec2 direction = direction_for(Axis::X, kFrontCenter);

    InteractionController controller(kSize);
    press(controller, kFrontCenter);
    drag(controller, kFrontCenter, direction, 120.0f);
    controller.pointer_up();

    const Vec2 at = screen_of(kFrontCenter);
    REQUIRE_FALSE(
        controller.pointer_down(at.x, at.y, cube_camera(), cube_rect()));

    const auto during = controller.active_rotation();
    REQUIRE(during);
    const float angle = during->angle_degrees;

    // A move without a gesture must not disturb the animation.
    controller.pointer_move(at.x + 500.0f, at.y);
    const auto after = controller.active_rotation();
    REQUIRE(after);
    REQUIRE(after->angle_degrees == Approx(angle));

    const Settled settled = settle(controller);
    REQUIRE(settled.commits == 1);
}

TEST_CASE("cancelling a drag leaves the cube alone")
{
    const Vec2 direction = direction_for(Axis::X, kFrontCenter);

    InteractionController controller(kSize);
    press(controller, kFrontCenter);
    drag(controller, kFrontCenter, direction, 120.0f);
    REQUIRE(controller.active_rotation());

    controller.cancel();

    // The transient turn goes at once rather than snapping back: teardown has
    // no frame loop left to animate with, so there is one cancel path.
    REQUIRE_FALSE(controller.active_rotation());
    REQUIRE_FALSE(controller.advance(kFrameMs));
    REQUIRE_FALSE(controller.take_committed_move());
}

TEST_CASE("cancelling during the snap does not stop it")
{
    const Vec2 direction = direction_for(Axis::X, kFrontCenter);

    InteractionController controller(kSize);
    press(controller, kFrontCenter);
    drag(controller, kFrontCenter, direction, 120.0f);
    controller.pointer_up();

    // The release already said what the user wanted, so a late cancel from a
    // lost capture must not undo it.
    controller.cancel();
    REQUIRE(controller.active_rotation());

    const Settled settled = settle(controller);
    REQUIRE(settled.commits == 1);
    REQUIRE(settled.move);
    REQUIRE(settled.move->quarter_turns == 1);
}

TEST_CASE("a release snaps to the nearest quarter turn")
{
    struct Case {
        float dragged;
        int quarter_turns;
    };

    // The examples from the design document, both directions, and both sides
    // of the halfway point. A drag cannot land on exactly 45 degrees -- the
    // angle comes from a pixel difference -- so the rule is pinned by the
    // pair straddling it rather than by the unreachable tie itself.
    constexpr std::array<Case, 9> kCases{{
        {37.0f, 0},
        {67.0f, 1},
        {143.0f, 2},
        {-37.0f, 0},
        {-67.0f, -1},
        {-143.0f, -2},
        {44.5f, 0},
        {45.5f, 1},
        {270.0f, 3},
    }};

    const Vec2 direction = direction_for(Axis::X, kFrontCenter);

    for (const auto& sample : kCases) {
        INFO(sample.dragged << " degrees");
        const Settled settled = perform(kFrontCenter, direction,
                                        sample.dragged);

        if (sample.quarter_turns == 0) {
            REQUIRE(settled.commits == 0);
            REQUIRE_FALSE(settled.move);
            continue;
        }

        REQUIRE(settled.commits == 1);
        REQUIRE(settled.move);
        REQUIRE(settled.move->quarter_turns == sample.quarter_turns);
    }
}

TEST_CASE("a committed gesture agrees with the named move")
{
    const Vec3 grab{rubiks::graphics::cubie_center(2, kSize),
                    rubiks::graphics::cubie_center(1, kSize), 1.0f};

    const Settled settled = perform(grab, Vec2{0.0f, -1.0f}, 90.0f);
    REQUIRE(settled.move);

    CubeState dragged(kSize);
    dragged.apply(*settled.move);

    CubeState named(kSize);
    named.apply(rubiks::cube::moves::R(kSize));

    REQUIRE(dragged == named);
}

TEST_CASE("the snap runs for a bounded number of frames and commits once")
{
    const Vec2 direction = direction_for(Axis::X, kFrontCenter);

    InteractionController controller(kSize);
    press(controller, kFrontCenter);

    // While a finger is down there is always another frame to draw.
    REQUIRE(controller.advance(kFrameMs));

    drag(controller, kFrontCenter, direction, 120.0f);
    controller.pointer_up();

    const Settled settled = settle(controller);
    REQUIRE(settled.commits == 1);

    // Snapping never travels more than a half quarter turn, so the animation
    // is bounded without needing a cap on its duration.
    const auto longest = static_cast<int>(
        kSnapMsPerQuarterTurn * 0.5 / kFrameMs) + 2;
    REQUIRE(settled.frames <= longest);
    REQUIRE(settled.frames >= 1);

    // Settled means settled: nothing left to draw and nothing left to take.
    REQUIRE_FALSE(controller.advance(kFrameMs));
    REQUIRE_FALSE(controller.take_committed_move());
    REQUIRE_FALSE(controller.active_rotation());
}

TEST_CASE("releasing alone commits nothing")
{
    const Vec2 direction = direction_for(Axis::X, kFrontCenter);

    InteractionController controller(kSize);
    press(controller, kFrontCenter);
    drag(controller, kFrontCenter, direction, 120.0f);
    controller.pointer_up();

    // There is one commit path, the frame the animation ends on, so letting
    // go does not by itself change anything.
    REQUIRE_FALSE(controller.take_committed_move());
    REQUIRE(controller.active_rotation());

    REQUIRE(settle(controller).commits == 1);
}

TEST_CASE("a snap with nothing left to travel ends on the next frame")
{
    const Vec2 direction = direction_for(Axis::X, kFrontCenter);
    const Vec2 start = screen_of(kFrontCenter);

    InteractionController controller(kSize);
    press(controller, kFrontCenter);
    drag(controller, kFrontCenter, direction, 120.0f);

    // Back to where the finger went down: the angle is exactly zero again,
    // which is the one case where the snap has no distance to cover.
    controller.pointer_move(start.x, start.y);
    const auto returned = controller.active_rotation();
    REQUIRE(returned);
    REQUIRE(returned->angle_degrees == Approx(0.0f).margin(1e-4f));

    controller.pointer_up();

    REQUIRE(controller.advance(0.0));
    REQUIRE_FALSE(controller.take_committed_move());
    REQUIRE_FALSE(controller.active_rotation());
    REQUIRE_FALSE(controller.advance(0.0));
}

TEST_CASE("odd elapsed times cannot break the animation")
{
    const Vec2 direction = direction_for(Axis::X, kFrontCenter);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();

    InteractionController controller(kSize);
    press(controller, kFrontCenter);
    drag(controller, kFrontCenter, direction, 120.0f);
    controller.pointer_up();

    const auto start = controller.active_rotation();
    REQUIRE(start);

    // None of these may move the animation on.
    REQUIRE(controller.advance(nan));
    REQUIRE(controller.advance(-1000.0));
    REQUIRE(controller.advance(0.0));

    const auto unchanged = controller.active_rotation();
    REQUIRE(unchanged);
    REQUIRE(unchanged->angle_degrees == Approx(start->angle_degrees));
    REQUIRE_FALSE(controller.take_committed_move());

    // A huge delta is clamped, so it finishes the snap rather than doing
    // anything stranger.
    REQUIRE(controller.advance(infinity));
    REQUIRE_FALSE(controller.take_committed_move());

    const Settled settled = settle(controller);
    REQUIRE(settled.commits == 1);
}

TEST_CASE("non-finite pointer coordinates are ignored")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const Vec2 direction = direction_for(Axis::X, kFrontCenter);

    InteractionController controller(kSize);
    REQUIRE_FALSE(
        controller.pointer_down(nan, nan, cube_camera(), cube_rect()));

    press(controller, kFrontCenter);
    drag(controller, kFrontCenter, direction, 120.0f);

    const auto before = controller.active_rotation();
    REQUIRE(before);

    controller.pointer_move(nan, 10.0f);
    const auto after = controller.active_rotation();
    REQUIRE(after);
    REQUIRE(after->angle_degrees == Approx(before->angle_degrees));
}

TEST_CASE("reset returns the controller to rest")
{
    const Vec2 direction = direction_for(Axis::X, kFrontCenter);

    InteractionController controller(kSize);
    press(controller, kFrontCenter);
    drag(controller, kFrontCenter, direction, 120.0f);
    controller.pointer_up();
    REQUIRE(controller.active_rotation());

    controller.reset();

    REQUIRE_FALSE(controller.active_rotation());
    REQUIRE_FALSE(controller.advance(kFrameMs));
    REQUIRE_FALSE(controller.take_committed_move());
}
