#include "app/Application.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "graphics/CubeGeometry.hpp"
#include "graphics/Layout.hpp"
#include "interaction/DragResolver.hpp"
#include "interaction/InteractionController.hpp"

// The application side of pointer interaction: that events reach the
// controller at all, that a finished gesture reaches the cube, and that the
// lifecycle events which invalidate a gesture cannot leave a turn behind.

namespace {

using Rgba = std::array<std::uint8_t, 4>;

constexpr Rgba kGreen{0, 155, 72, 255};
constexpr Rgba kWhite{255, 255, 255, 255};
constexpr Rgba kRed{183, 18, 52, 255};
constexpr Rgba kBlue{0, 70, 173, 255};
constexpr Rgba kOrange{255, 88, 0, 255};

// The three face centers of the rendered scene contract, as fractions of the
// square 3D region. Which colors they read depends on the viewpoint, which is
// exactly what makes them useful for checking an orbit.
constexpr float kUpSample[]{0.50f, 0.29f};
constexpr float kLeftSample[]{0.31f, 0.61f};
constexpr float kRightSample[]{0.69f, 0.61f};

constexpr std::uint32_t kCanvas = 1024;
constexpr int kCubeSize = 3;
constexpr double kFrameMs = 16.0;

/** Enough drag for the snap to land on one quarter turn. */
constexpr float kQuarterTurnDrag = 120.0f;

rubiks::graphics::Rect cube_rect(std::uint32_t width, std::uint32_t height)
{
    return rubiks::graphics::layout(width, height).cube;
}

rubiks::graphics::Camera cube_camera(std::uint32_t width, std::uint32_t height)
{
    const auto rect = cube_rect(width, height);
    return rubiks::graphics::default_camera(rect.width / rect.height);
}

/** Where the middle sticker of the front face's right column is drawn. */
rubiks::math::Vec2 front_right_column(std::uint32_t width,
                                      std::uint32_t height)
{
    const rubiks::math::Vec3 world{
        rubiks::graphics::cubie_center(2, kCubeSize),
        rubiks::graphics::cubie_center(1, kCubeSize), 1.0f};

    const auto point = rubiks::interaction::project_to_screen(
        world, cube_camera(width, height), cube_rect(width, height));
    REQUIRE(point);
    return *point;
}

float pixels_per_degree(std::uint32_t width, std::uint32_t height)
{
    return rubiks::interaction::kQuarterTurnFraction *
           cube_rect(width, height).width / 90.0f;
}

/** Runs the animation out, so the cube is at rest afterwards. */
int settle()
{
    int frames = 0;
    while (rubiks::app::advance(kFrameMs)) {
        ++frames;
        REQUIRE(frames < 1000);
    }
    return frames;
}

const std::uint8_t* pixel_at(float x, float y, std::uint32_t width)
{
    const auto* bytes =
        reinterpret_cast<const std::uint8_t*>(rubiks::app::pixel_buffer());
    const auto column = static_cast<std::size_t>(x);
    const auto row = static_cast<std::size_t>(y);
    return bytes + (row * width + column) * 4;
}

/** Reads the center of one cell of the net's top face. */
const std::uint8_t* up_face_cell(int column, int row, std::uint32_t width,
                                 std::uint32_t height)
{
    const auto net = rubiks::graphics::layout(width, height).net;
    const float face_side =
        net.width / static_cast<float>(rubiks::graphics::kNetColumns);
    const float cell = face_side / static_cast<float>(kCubeSize);

    // The top face sits in the second column of the cross, on the first row.
    return pixel_at(net.x + face_side + (static_cast<float>(column) + 0.5f) * cell,
                    net.y + (static_cast<float>(row) + 0.5f) * cell, width);
}

void require_pixel(const std::uint8_t* pixel, const Rgba& color)
{
    REQUIRE(pixel[0] == color[0]);
    REQUIRE(pixel[1] == color[1]);
    REQUIRE(pixel[2] == color[2]);
    REQUIRE(pixel[3] == color[3]);
}

/** The net's top face right column, which R turns from white to green. */
void require_up_right_column(const Rgba& color)
{
    REQUIRE(rubiks::app::render());
    for (int row = 0; row < kCubeSize; ++row) {
        require_pixel(up_face_cell(2, row, kCanvas, kCanvas), color);
    }
}

/** Reads a point given as a fraction of the square 3D region. */
const std::uint8_t* cube_sample(const float (&sample)[2], std::uint32_t width,
                                std::uint32_t height)
{
    const auto rect = cube_rect(width, height);
    return pixel_at(rect.x + sample[0] * rect.width,
                    rect.y + sample[1] * rect.height, width);
}

/** The three visible face centers, in screen order. */
void require_visible_faces(const Rgba& top, const Rgba& left,
                           const Rgba& right)
{
    REQUIRE(rubiks::app::render());
    require_pixel(cube_sample(kUpSample, kCanvas, kCanvas), top);
    require_pixel(cube_sample(kLeftSample, kCanvas, kCanvas), left);
    require_pixel(cube_sample(kRightSample, kCanvas, kCanvas), right);
}

/** The whole solved net, which no viewpoint change may disturb. */
void require_solved_net()
{
    REQUIRE(rubiks::app::render());
    for (int column = 0; column < kCubeSize; ++column) {
        for (int row = 0; row < kCubeSize; ++row) {
            require_pixel(up_face_cell(column, row, kCanvas, kCanvas), kWhite);
        }
    }
}

/** Drags the background sideways, sweeping the viewpoint by `quarter_turns`. */
void orbit_left(float quarter_turns)
{
    const auto rect = cube_rect(kCanvas, kCanvas);
    const float from_x = rect.x + rect.width * 0.02f;
    const float from_y = rect.y + rect.height * 0.02f;

    REQUIRE(rubiks::app::pointer_down(from_x, from_y));
    rubiks::app::pointer_move(
        from_x - rubiks::interaction::kOrbitQuarterTurnFraction * rect.width *
                     quarter_turns,
        from_y);
}

/** Presses the front right column and drags upward, without releasing. */
void drag_upward(float degrees)
{
    const auto grab = front_right_column(kCanvas, kCanvas);
    REQUIRE(rubiks::app::pointer_down(grab.x, grab.y));
    rubiks::app::pointer_move(
        grab.x, grab.y - degrees * pixels_per_degree(kCanvas, kCanvas));
}

}  // namespace

TEST_CASE("pointer entry points are safe before initialization")
{
    REQUIRE_FALSE(rubiks::app::is_initialized());

    REQUIRE_FALSE(rubiks::app::pointer_down(10.0f, 10.0f));
    REQUIRE_FALSE(rubiks::app::advance(kFrameMs));

    // These return nothing, so the contract is only that they are harmless.
    rubiks::app::pointer_move(10.0f, 10.0f);
    rubiks::app::pointer_up();
    rubiks::app::pointer_cancel();

    REQUIRE_FALSE(rubiks::app::is_initialized());
}

TEST_CASE("a drag gesture turns the cube")
{
    REQUIRE(rubiks::app::initialize(kCanvas, kCanvas));

    require_up_right_column(kWhite);

    drag_upward(kQuarterTurnDrag);

    // A turn in progress does not touch the logical cube, and the net only
    // ever draws the logical cube.
    require_up_right_column(kWhite);

    rubiks::app::pointer_up();
    REQUIRE(settle() >= 1);

    // R lifts the front face's right column onto the top face.
    require_up_right_column(kGreen);

    rubiks::app::shutdown();
}

TEST_CASE("pressing away from the cube never turns a layer")
{
    REQUIRE(rubiks::app::initialize(kCanvas, kCanvas));

    // A corner of the 3D region, outside the silhouette. This starts a
    // viewpoint sweep rather than nothing, but the cube itself is untouched
    // however far it is dragged.
    const auto rect = cube_rect(kCanvas, kCanvas);
    const float corner_x = rect.x + rect.width * 0.02f;
    const float corner_y = rect.y + rect.height * 0.02f;

    REQUIRE(rubiks::app::pointer_down(corner_x, corner_y));
    rubiks::app::pointer_move(corner_x - 300.0f, corner_y);
    rubiks::app::pointer_up();
    settle();

    require_up_right_column(kWhite);

    rubiks::app::shutdown();
}

TEST_CASE("cancelling a gesture leaves the cube alone")
{
    REQUIRE(rubiks::app::initialize(kCanvas, kCanvas));

    drag_upward(kQuarterTurnDrag);
    rubiks::app::pointer_cancel();

    // Nothing to animate and nothing committed: a cancelled pointer is not a
    // release, however far it was dragged.
    REQUIRE_FALSE(rubiks::app::advance(kFrameMs));
    require_up_right_column(kWhite);

    // A release arriving after the cancel finds no gesture to end.
    rubiks::app::pointer_up();
    REQUIRE_FALSE(rubiks::app::advance(kFrameMs));
    require_up_right_column(kWhite);

    rubiks::app::shutdown();
}

TEST_CASE("resizing during a drag abandons it")
{
    REQUIRE(rubiks::app::initialize(kCanvas, kCanvas));

    drag_upward(kQuarterTurnDrag);

    // The gesture holds screen directions from the old viewport, so it cannot
    // survive; dropping it is what keeps the angle from jumping.
    REQUIRE(rubiks::app::resize(800, 600));
    REQUIRE_FALSE(rubiks::app::advance(kFrameMs));

    rubiks::app::pointer_up();
    REQUIRE_FALSE(rubiks::app::advance(kFrameMs));

    REQUIRE(rubiks::app::resize(kCanvas, kCanvas));
    require_up_right_column(kWhite);

    rubiks::app::shutdown();
}

TEST_CASE("resizing during the snap lets it finish")
{
    REQUIRE(rubiks::app::initialize(kCanvas, kCanvas));

    drag_upward(kQuarterTurnDrag);
    rubiks::app::pointer_up();

    // The release already decided the turn, and the snap animates an angle
    // rather than reading the screen, so a resize cannot invalidate it.
    REQUIRE(rubiks::app::resize(800, 600));
    REQUIRE(settle() >= 1);

    REQUIRE(rubiks::app::resize(kCanvas, kCanvas));
    require_up_right_column(kGreen);

    rubiks::app::shutdown();
}

TEST_CASE("dragging the background sweeps the viewpoint")
{
    REQUIRE(rubiks::app::initialize(kCanvas, kCanvas));

    // Home shows the +Z, +X and +Y faces around the (1, 1, 1) diagonal.
    require_visible_faces(kWhite, kGreen, kRed);

    // A quarter turn brings the next corner round, so the face that was on
    // the right moves to the left and the back face takes its place.
    orbit_left(1.0f);
    rubiks::app::pointer_up();
    settle();

    require_visible_faces(kWhite, kRed, kBlue);

    // The cube itself never moved.
    require_solved_net();

    rubiks::app::shutdown();
}

TEST_CASE("sweeping the other way brings the opposite corner round")
{
    REQUIRE(rubiks::app::initialize(kCanvas, kCanvas));

    // The other neighbouring corner: the front face slides right and the left
    // face comes into view beside it.
    orbit_left(-1.0f);
    rubiks::app::pointer_up();
    settle();

    require_visible_faces(kWhite, kOrange, kGreen);
    require_solved_net();

    rubiks::app::shutdown();
}

TEST_CASE("a press between two frames sees the viewpoint it was aimed at")
{
    REQUIRE(rubiks::app::initialize(kCanvas, kCanvas));

    orbit_left(1.0f);
    rubiks::app::pointer_up();

    // No frame has run, so the sweep is still waiting. Pressing now has to
    // apply it exactly once: not zero times, which would pick against a
    // viewpoint the user never saw, and not twice.
    const auto rect = cube_rect(kCanvas, kCanvas);
    REQUIRE(rubiks::app::pointer_down(rect.x + rect.width * 0.02f,
                                      rect.y + rect.height * 0.02f));
    rubiks::app::pointer_up();
    settle();

    require_visible_faces(kWhite, kRed, kBlue);

    rubiks::app::shutdown();
}

TEST_CASE("a resize keeps the viewpoint and applies the last sweep")
{
    REQUIRE(rubiks::app::initialize(kCanvas, kCanvas));

    orbit_left(1.0f);

    // The gesture cannot continue across a resize, but what it already swept
    // still counts.
    REQUIRE(rubiks::app::resize(800, 600));
    REQUIRE(rubiks::app::resize(kCanvas, kCanvas));
    settle();

    require_visible_faces(kWhite, kRed, kBlue);

    rubiks::app::shutdown();
}

TEST_CASE("scramble and reset keep the viewpoint the drag left behind")
{
    REQUIRE(rubiks::app::initialize(kCanvas, kCanvas));

    // A sweep no frame has published yet. Neither command owns the camera,
    // so both have to keep it rather than drop it with the gesture state.
    orbit_left(1.0f);
    rubiks::app::pointer_up();
    REQUIRE(rubiks::app::scramble(42U));
    settle();

    // Face centers never move under outer-face turns, so a scrambled cube
    // still names the faces around the viewpoint by the same three colors.
    require_visible_faces(kWhite, kRed, kBlue);

    orbit_left(1.0f);
    rubiks::app::pointer_up();
    rubiks::app::reset_cube();
    settle();

    require_visible_faces(kWhite, kBlue, kOrange);
    require_solved_net();

    rubiks::app::shutdown();
}

TEST_CASE("shutdown returns the viewpoint home")
{
    REQUIRE(rubiks::app::initialize(kCanvas, kCanvas));
    orbit_left(1.0f);
    rubiks::app::pointer_up();
    settle();
    require_visible_faces(kWhite, kRed, kBlue);

    rubiks::app::shutdown();
    REQUIRE(rubiks::app::initialize(kCanvas, kCanvas));

    // Back to the viewpoint the rendered scene contract was derived from.
    require_visible_faces(kWhite, kGreen, kRed);

    rubiks::app::shutdown();
}

TEST_CASE("a layer still turns correctly after the viewpoint moves")
{
    REQUIRE(rubiks::app::initialize(kCanvas, kCanvas));

    // A whole turn of the viewpoint lands back where it started, so the
    // gesture that means R at home has to still mean R here.
    orbit_left(4.0f);
    rubiks::app::pointer_up();
    settle();
    require_visible_faces(kWhite, kGreen, kRed);

    drag_upward(kQuarterTurnDrag);
    rubiks::app::pointer_up();
    settle();

    require_up_right_column(kGreen);

    rubiks::app::shutdown();
}

TEST_CASE("shutdown clears the gesture along with the cube")
{
    REQUIRE(rubiks::app::initialize(kCanvas, kCanvas));
    drag_upward(kQuarterTurnDrag);
    rubiks::app::shutdown();

    REQUIRE(rubiks::app::initialize(kCanvas, kCanvas));

    // Neither the half-finished gesture nor any turn survived.
    REQUIRE_FALSE(rubiks::app::advance(kFrameMs));
    require_up_right_column(kWhite);

    rubiks::app::shutdown();
}
