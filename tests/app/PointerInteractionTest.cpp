#include "app/Application.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <catch2/catch_test_macros.hpp>

#include "EngineLifecycle.hpp"
#include "cube/Cubie.hpp"
#include "graphics/CubeGeometry.hpp"
#include "graphics/Layout.hpp"
#include "graphics/NetGeometry.hpp"
#include "graphics/RingsGeometry.hpp"
#include "graphics/SlotRing.hpp"
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
constexpr Rgba kYellow{255, 213, 0, 255};
constexpr Rgba kBackground{32, 32, 32, 255};

/** The cut surface, only ever on show while a layer is turning. */
constexpr Rgba kBody{70, 74, 82, 255};

// The 3D view is lit (render contract v4): one brightness per plane of the
// cube, from docs/tasks/04-rubiks-cube-domain.md. The three planes turned
// away from the light are at the ambient floor.
constexpr unsigned kLitUp = 239;
constexpr unsigned kLitFront = 189;
constexpr unsigned kLitRight = 164;
constexpr unsigned kLitAway = 153;
/** The +Y cut a U turn opens, one layer down from the top. */
constexpr unsigned kLitCutUp = 241;

constexpr Rgba lit(const Rgba& color, unsigned brightness)
{
    return Rgba{static_cast<std::uint8_t>((color[0] * brightness + 127) / 255),
                static_cast<std::uint8_t>((color[1] * brightness + 127) / 255),
                static_cast<std::uint8_t>((color[2] * brightness + 127) / 255),
                color[3]};
}

// The six faces as the 3D view shows them: a sticker's colour at its plane's
// brightness. Named by the face so an orbit test reads as which face is where.
constexpr Rgba kUpLit = lit(kWhite, kLitUp);
constexpr Rgba kFrontLit = lit(kGreen, kLitFront);
constexpr Rgba kRightLit = lit(kRed, kLitRight);
constexpr Rgba kBackLit = lit(kBlue, kLitAway);
constexpr Rgba kLeftLit = lit(kOrange, kLitAway);

// The three face centers of the rendered scene contract, as fractions of the
// square 3D region. Which colors they read depends on the viewpoint, which is
// exactly what makes them useful for checking an orbit.
constexpr float kUpSample[]{0.50f, 0.29f};
constexpr float kLeftSample[]{0.31f, 0.61f};
constexpr float kRightSample[]{0.69f, 0.61f};

constexpr std::uint32_t kCanvas = 1024;
constexpr int kCubeSize = 3;

using rubiks::test::kFrameMs;
using rubiks::test::settle;

/**
 * Enough drag for the snap to land on one quarter turn.
 *
 * The middle of the slot that commits one turn, well clear of the boundaries
 * on either side of it so that these tests never hinge on where those sit.
 */
constexpr float kQuarterTurnDrag = 75.0f;

rubiks::graphics::Rect cube_rect(std::uint32_t width, std::uint32_t height)
{
    return rubiks::graphics::layout(width, height).cube;
}

rubiks::graphics::Camera cube_camera(std::uint32_t width, std::uint32_t height)
{
    const auto rect = cube_rect(width, height);
    return rubiks::graphics::default_camera(rect.width / rect.height);
}

/** Where the middle sticker of one front face column is drawn. */
rubiks::math::Vec2 front_column(int column, std::uint32_t width,
                                std::uint32_t height)
{
    const rubiks::math::Vec3 world{
        rubiks::graphics::cubie_center(column, kCubeSize),
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

const std::uint8_t* pixel_at(float x, float y, std::uint32_t width)
{
    const auto* bytes =
        reinterpret_cast<const std::uint8_t*>(rubiks::app::pixel_buffer());
    const auto column = static_cast<std::size_t>(x);
    const auto row = static_cast<std::size_t>(y);
    return bytes + (row * width + column) * 4;
}

/** Center of one net cell, in drawing-buffer pixels. */
rubiks::math::Vec2 net_cell_point(rubiks::cube::Face face, int column, int row)
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

/** Reads the center of one net cell. */
const std::uint8_t* net_cell_pixel(rubiks::cube::Face face, int column,
                                   int row)
{
    const auto point = net_cell_point(face, column, row);
    return pixel_at(point.x, point.y, kCanvas);
}

/** Reads the center of one cell of the net's top face. */
const std::uint8_t* up_face_cell(int column, int row)
{
    return net_cell_pixel(rubiks::cube::Face::Up, column, row);
}

void require_pixel(const std::uint8_t* pixel, const Rgba& color)
{
    // Compared as integers so a failure prints numbers rather than bytes.
    REQUIRE(int{pixel[0]} == int{color[0]});
    REQUIRE(int{pixel[1]} == int{color[1]});
    REQUIRE(int{pixel[2]} == int{color[2]});
    REQUIRE(int{pixel[3]} == int{color[3]});
}

/** One column of the net's top face, all three cells. */
void require_up_column(int column, const Rgba& color)
{
    REQUIRE(rubiks::app::render());
    for (int row = 0; row < kCubeSize; ++row) {
        INFO("up face cell at column " << column << ", row " << row);
        require_pixel(up_face_cell(column, row), color);
    }
}

/** One row of an unfolded face, all three cells. */
void require_net_row(rubiks::cube::Face face, int row, const Rgba& color)
{
    REQUIRE(rubiks::app::render());
    for (int column = 0; column < kCubeSize; ++column) {
        INFO("net cell at column " << column << ", row " << row);
        require_pixel(net_cell_pixel(face, column, row), color);
    }
}

/** The net's top face right column, which R turns from white to green. */
void require_up_right_column(const Rgba& color)
{
    require_up_column(2, color);
}

/** Reads a point given as a fraction of the square 3D region. */
const std::uint8_t* cube_sample(const float (&sample)[2], std::uint32_t width,
                                std::uint32_t height)
{
    const auto rect = cube_rect(width, height);
    return pixel_at(rect.x + sample[0] * rect.width,
                    rect.y + sample[1] * rect.height, width);
}

/**
 * Whether a pixel is a colour, or that colour with a glint on it.
 *
 * From some viewpoints the light reflects off a face straight into the eye,
 * and the renderer screens white onto it there: every channel moves the same
 * fraction of the way to 255. The faces these tests name are the ones at the
 * sample points whatever the glint, so a pixel counts when one such fraction
 * explains all three channels.
 */
void require_pixel_reads(const std::uint8_t* pixel, const Rgba& color)
{
    REQUIRE(int{pixel[3]} == int{color[3]});

    // The channel with the most headroom fixes the fraction most precisely.
    int widest = 0;
    for (int i = 1; i < 3; ++i) {
        if (255 - color[i] > 255 - color[widest]) widest = i;
    }
    const int headroom = 255 - color[widest];
    double fraction =
        headroom > 0 ? double(int{pixel[widest]} - int{color[widest]}) / headroom
                     : 0.0;
    // The lit faces are gradients, whose rounding can put a channel one unit
    // under the reference; that is no glint, and reads as none.
    if (fraction < 0.0 && fraction * headroom >= -1.5) fraction = 0.0;
    REQUIRE(fraction >= 0.0);
    REQUIRE(fraction <= 1.0);

    for (int i = 0; i < 3; ++i) {
        const double expected = color[i] + fraction * (255 - color[i]);
        INFO("channel " << i << " with a glint of " << fraction);
        REQUIRE(std::abs(int{pixel[i]} - expected) <= 2.0);
    }
}

/** The three visible face centers, in screen order. */
void require_visible_faces(const Rgba& top, const Rgba& left,
                           const Rgba& right)
{
    REQUIRE(rubiks::app::render());
    require_pixel_reads(cube_sample(kUpSample, kCanvas, kCanvas), top);
    require_pixel_reads(cube_sample(kLeftSample, kCanvas, kCanvas), left);
    require_pixel_reads(cube_sample(kRightSample, kCanvas, kCanvas), right);
}

/** The whole solved net, which no viewpoint change may disturb. */
void require_solved_net()
{
    REQUIRE(rubiks::app::render());
    for (int column = 0; column < kCubeSize; ++column) {
        for (int row = 0; row < kCubeSize; ++row) {
            require_pixel(up_face_cell(column, row), kWhite);
        }
    }
}

/** Whether a coarse sweep of the 3D region finds a color anywhere in it. */
bool cube_region_shows(const Rgba& color)
{
    constexpr int kSteps = 24;
    const auto rect = cube_rect(kCanvas, kCanvas);

    for (int row = 0; row < kSteps; ++row) {
        for (int column = 0; column < kSteps; ++column) {
            const float fx = (static_cast<float>(column) + 0.5f) / kSteps;
            const float fy = (static_cast<float>(row) + 0.5f) / kSteps;
            const auto* pixel =
                pixel_at(rect.x + fx * rect.width, rect.y + fy * rect.height,
                         kCanvas);
            // Within a unit per channel: the lit faces are gradients.
            if (std::abs(int{pixel[0]} - int{color[0]}) <= 1 &&
                std::abs(int{pixel[1]} - int{color[1]}) <= 1 &&
                std::abs(int{pixel[2]} - int{color[2]}) <= 1 &&
                pixel[3] == color[3]) {
                return true;
            }
        }
    }
    return false;
}

/** A point on the background, clear of the cube's silhouette. */
rubiks::math::Vec2 background_point()
{
    const auto rect = cube_rect(kCanvas, kCanvas);
    return rubiks::math::Vec2{rect.x + rect.width * 0.02f,
                              rect.y + rect.height * 0.02f};
}

/** Drags the background sideways, sweeping the viewpoint by `quarter_turns`. */
void orbit_left(float quarter_turns)
{
    const auto rect = cube_rect(kCanvas, kCanvas);
    const auto from = background_point();

    REQUIRE(rubiks::app::pointer_down(from.x, from.y));
    rubiks::app::pointer_move(
        from.x - rubiks::interaction::kOrbitQuarterTurnFraction * rect.width *
                     quarter_turns,
        from.y);
}

/** Carries an upward drag on one front face column to a given angle. */
void move_column_upward(int column, float degrees)
{
    const auto grab = front_column(column, kCanvas, kCanvas);
    rubiks::app::pointer_move(
        grab.x, grab.y - degrees * pixels_per_degree(kCanvas, kCanvas));
}

/** Presses one front face column and drags upward, without releasing. */
void drag_column_upward(int column, float degrees)
{
    const auto grab = front_column(column, kCanvas, kCanvas);
    REQUIRE(rubiks::app::pointer_down(grab.x, grab.y));
    move_column_upward(column, degrees);
}

/** The same for the right column, the gesture that means R. */
void drag_upward(float degrees)
{
    drag_column_upward(2, degrees);
}

/**
 * Presses a net cell and drags it, without releasing.
 *
 * The distance is in face widths, which is also the net's own sensitivity:
 * one face across is one quarter turn.
 */
void drag_net_cell(rubiks::cube::Face face, int column, int row, float faces_x,
                   float faces_y)
{
    const auto net = rubiks::graphics::layout(kCanvas, kCanvas).net;
    const float face_side =
        net.width / static_cast<float>(rubiks::graphics::kNetColumns);
    const auto from = net_cell_point(face, column, row);

    REQUIRE(rubiks::app::pointer_down(from.x, from.y));
    rubiks::app::pointer_move(from.x + faces_x * face_side,
                              from.y + faces_y * face_side);
}

/** Enough of a net drag to commit, well clear of either boundary. */
constexpr float kNetTurnDrag = 0.6f;

/**
 * Drags the top row of the net's front face to the left, which means U.
 *
 * The turn is derived from where the cell would land, so this is the gesture
 * that carries the front's top row round onto the left face.
 */
void drag_net_top_row_left()
{
    drag_net_cell(rubiks::cube::Face::Front, 1, 0, -kNetTurnDrag, 0.0f);
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
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    require_up_right_column(kWhite);

    drag_upward(kQuarterTurnDrag);

    // A turn in progress is a picture in both views and a change to neither:
    // the logical cube only moves when the release settles.
    REQUIRE(rubiks::app::committed_move_count() == 0);

    rubiks::app::pointer_up();
    REQUIRE(settle() >= 1);

    // R lifts the front face's right column onto the top face.
    require_up_right_column(kGreen);

}

TEST_CASE("the net shows a 3D drag while it is happening")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    require_up_right_column(kWhite);

    // Two thirds of the way round. R's band is the column Up, Front and Down
    // share in the cross, so it marches up that column: two cells along, the
    // front face's stickers are the ones standing in the up face's block.
    drag_upward(60.0f);
    REQUIRE(rubiks::app::render());

    require_pixel(up_face_cell(2, 1), kGreen);

    // The layer the drag never touched is untouched in the drawing too.
    require_up_column(0, kWhite);

    // And the part of the band the cross cuts leaves its block through the
    // corner below it, which is empty whenever the cube is at rest. The ring
    // carries it through there somewhere along the way rather than at one
    // named angle, so the whole crossing is swept.
    const auto net = rubiks::graphics::layout(kCanvas, kCanvas).net;
    const float face_side =
        net.width / static_cast<float>(rubiks::graphics::kNetColumns);
    const float cell = face_side / static_cast<float>(kCubeSize);
    const float corner_x = net.x + 3.0f * face_side + 0.5f * cell;
    const float corner_y = net.y + 2.0f * face_side + 1.5f * cell;

    REQUIRE(pixel_at(corner_x, corner_y, kCanvas)[2] == kBackground[2]);

    bool crossed = false;
    for (int step = 1; step <= 89 && !crossed; ++step) {
        move_column_upward(2, static_cast<float>(step));
        REQUIRE(rubiks::app::render());
        crossed = pixel_at(corner_x, corner_y, kCanvas)[2] != kBackground[2];
    }
    REQUIRE(crossed);

    // And a quarter turn in, the picture the net shows is already exactly
    // what the commit is about to make true.
    move_column_upward(2, 90.0f);
    require_up_right_column(kGreen);

    // Which the commit then does, without the picture changing.
    rubiks::app::pointer_up();
    settle();
    REQUIRE(rubiks::app::committed_move_count() == 1);
    require_up_right_column(kGreen);

}

TEST_CASE("a press during the snap confirms it and starts the next drag")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    drag_upward(kQuarterTurnDrag);
    rubiks::app::pointer_up();

    // One frame, so the snap is genuinely still running when the press lands.
    REQUIRE(rubiks::app::advance(kFrameMs));
    REQUIRE(rubiks::app::committed_move_count() == 0);

    // The press that used to be dropped, taking the whole stroke with it. It
    // applies the turn the release had already decided and begins its own.
    drag_upward(kQuarterTurnDrag);
    REQUIRE(rubiks::app::committed_move_count() == 1);

    rubiks::app::pointer_up();
    settle();

    // R twice: the column the first turn lifted onto the top is replaced by
    // the one that followed it round, and neither move went missing.
    REQUIRE(rubiks::app::committed_move_count() == 2);
    require_up_right_column(kYellow);

}

TEST_CASE("a press during a button turn confirms that too")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    // Keyboard and DOM turns animate through the same snap, so the rule
    // reaches them without knowing about them: a press ends whatever turn is
    // running, whoever asked for it.
    REQUIRE(rubiks::app::turn_face(rubiks::cube::Face::Right, 1, 1, 1));
    REQUIRE(rubiks::app::advance(kFrameMs));

    drag_upward(kQuarterTurnDrag);
    REQUIRE(rubiks::app::committed_move_count() == 1);

    rubiks::app::pointer_up();
    settle();

    REQUIRE(rubiks::app::committed_move_count() == 2);
    require_up_right_column(kYellow);

}

TEST_CASE("the drag after a confirmation picks the cell it was aimed at")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    drag_upward(kQuarterTurnDrag);
    rubiks::app::pointer_up();
    REQUIRE(rubiks::app::advance(kFrameMs));

    // The middle column this time, so a confirmation that left the new
    // gesture aimed at the old layer would show up as the wrong turn.
    drag_column_upward(1, kQuarterTurnDrag);
    rubiks::app::pointer_up();
    settle();

    REQUIRE(rubiks::app::committed_move_count() == 2);
    require_up_column(1, kGreen);

    // The layer the confirmed turn moved is where it was left, and the one
    // neither gesture aimed at never moved.
    require_up_column(2, kGreen);
    require_up_column(0, kWhite);

}

TEST_CASE("confirming a snap that turns nothing leaves the cube alone")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    // Short of the commit threshold, so this release was going to spring back.
    drag_upward(15.0f);
    rubiks::app::pointer_up();
    REQUIRE(rubiks::app::advance(kFrameMs));

    drag_upward(kQuarterTurnDrag);
    REQUIRE(rubiks::app::committed_move_count() == 0);

    rubiks::app::pointer_up();
    settle();

    REQUIRE(rubiks::app::committed_move_count() == 1);
    require_up_right_column(kGreen);

}

TEST_CASE("a press on the background confirms the snap as well")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    drag_upward(kQuarterTurnDrag);
    rubiks::app::pointer_up();
    REQUIRE(rubiks::app::advance(kFrameMs));

    // Where the press landed is no part of the rule: a snap is over once the
    // next press arrives, whether that press turns a layer or sweeps the view.
    const auto corner = background_point();
    REQUIRE(rubiks::app::pointer_down(corner.x, corner.y));

    REQUIRE(rubiks::app::committed_move_count() == 1);
    require_up_right_column(kGreen);

    rubiks::app::pointer_up();
    settle();
    REQUIRE(rubiks::app::committed_move_count() == 1);

}

TEST_CASE("a press that cannot start anything confirms nothing")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    drag_upward(kQuarterTurnDrag);
    rubiks::app::pointer_up();
    REQUIRE(rubiks::app::advance(kFrameMs));

    const float nan = std::numeric_limits<float>::quiet_NaN();
    REQUIRE_FALSE(rubiks::app::pointer_down(nan, nan));

    // Every reason to reject a press comes before the confirmation, so a press
    // that begins nothing has changed nothing either: the snap still owns the
    // turn and still finishes it on its own.
    REQUIRE(rubiks::app::committed_move_count() == 0);

    settle();
    REQUIRE(rubiks::app::committed_move_count() == 1);
    require_up_right_column(kGreen);

}

TEST_CASE("a press off the net in net-only view confirms nothing")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    drag_upward(kQuarterTurnDrag);
    rubiks::app::pointer_up();

    // Switching the view cancels gestures but never a snap, so one is still
    // running under a view with no 3D region to fall back on.
    REQUIRE(rubiks::app::set_view_mode(rubiks::graphics::ViewMode::Flat));
    REQUIRE(rubiks::app::advance(kFrameMs));

    // The corner of the canvas, clear of the net block. There is no viewpoint
    // on screen to sweep, so this press starts nothing -- and a press that
    // starts nothing confirms nothing.
    REQUIRE_FALSE(rubiks::app::pointer_down(2.0f, 2.0f));
    REQUIRE(rubiks::app::committed_move_count() == 0);

    settle();
    REQUIRE(rubiks::app::committed_move_count() == 1);

}

TEST_CASE("dragging a net cell turns the cube")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    drag_net_top_row_left();
    REQUIRE(rubiks::app::committed_move_count() == 0);

    rubiks::app::pointer_up();
    REQUIRE(settle() >= 1);

    REQUIRE(rubiks::app::committed_move_count() == 1);

    // U carries each side face's top row round onto the face to its left, so
    // the front takes the right's red and the left takes the front's green.
    require_net_row(rubiks::cube::Face::Front, 0, kRed);
    require_net_row(rubiks::cube::Face::Left, 0, kGreen);
    require_net_row(rubiks::cube::Face::Right, 0, kBlue);
    require_net_row(rubiks::cube::Face::Back, 0, kOrange);

    // The rows the turn did not take are where they were, and Up turns in
    // place, so a solved cube's top face is still one color.
    require_net_row(rubiks::cube::Face::Front, 1, kGreen);
    require_solved_net();

}

TEST_CASE("a net drag commits the same move a button would")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    drag_net_top_row_left();
    rubiks::app::pointer_up();
    settle();

    // Undoing it by name leaves a solved cube, so the drag committed exactly
    // U and nothing near it. Past the release the two paths are one move.
    REQUIRE(rubiks::app::turn_face(rubiks::cube::Face::Up, 1, 1, -1));
    settle();

    REQUIRE(rubiks::app::is_solved());
    REQUIRE(rubiks::app::committed_move_count() == 2);

}

TEST_CASE("a net drag turns the 3D view with it")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    REQUIRE(rubiks::app::render());
    REQUIRE_FALSE(cube_region_shows(lit(kBody, kLitCutUp)));

    drag_net_top_row_left();
    REQUIRE(rubiks::app::render());

    // One rotation drives both views, so a layer swung away in the net has
    // swung away in the cube too, leaving the cut surface on show -- lit as
    // the +Y plane one layer down.
    REQUIRE(cube_region_shows(lit(kBody, kLitCutUp)));

    rubiks::app::pointer_up();
    settle();
    REQUIRE(rubiks::app::render());
    REQUIRE_FALSE(cube_region_shows(lit(kBody, kLitCutUp)));

}

TEST_CASE("a net drag past a quarter turn settles on the turn it reached")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    // Two and a half faces across: at the drag sensitivity that is 225
    // degrees, which settles on three quarter turns. The net used to stop at
    // one, because one was all it could draw; it takes a band as far round its
    // loop as the finger goes now, so the picture and the commit agree however
    // far that is. Three quarters one way is one the other, so the front row
    // ends up holding the row that one quarter backwards would bring it.
    drag_net_cell(rubiks::cube::Face::Front, 1, 0, -2.5f, 0.0f);
    rubiks::app::pointer_up();
    settle();

    REQUIRE(rubiks::app::committed_move_count() == 1);
    require_net_row(rubiks::cube::Face::Front, 0, kOrange);
}

TEST_CASE("the net is draggable in net-only view")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    REQUIRE(rubiks::app::set_view_mode(rubiks::graphics::ViewMode::Flat));

    // The net-only layout is a different rectangle from the shared one.
    const auto net = rubiks::graphics::layout(kCanvas, kCanvas,
                                              rubiks::graphics::ViewMode::Flat)
                         .net;
    const float face_side =
        net.width / static_cast<float>(rubiks::graphics::kNetColumns);
    const float cell = face_side / static_cast<float>(kCubeSize);

    // The front face's top middle cell.
    const float x = net.x + face_side + 1.5f * cell;
    const float y = net.y + face_side + 0.5f * cell;

    REQUIRE(rubiks::app::pointer_down(x, y));
    rubiks::app::pointer_move(x - 0.6f * face_side, y);
    rubiks::app::pointer_up();
    settle();

    REQUIRE(rubiks::app::committed_move_count() == 1);

    // Read back in the shared layout, where the net cell helpers apply.
    REQUIRE(rubiks::app::set_view_mode(rubiks::graphics::ViewMode::Both));
    require_net_row(rubiks::cube::Face::Front, 0, kRed);

}

TEST_CASE("an empty corner of the cross is not a net cell")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    const auto net = rubiks::graphics::layout(kCanvas, kCanvas).net;
    const float face_side =
        net.width / static_cast<float>(rubiks::graphics::kNetColumns);

    // Top left of the cross, which the unfolding leaves empty. Inside the net
    // rectangle, but on no face.
    const float x = net.x + 0.5f * face_side;
    const float y = net.y + 0.5f * face_side;

    // With the cube on screen this is background like any other, so it sweeps
    // the viewpoint; what it must never do is turn a layer.
    REQUIRE(rubiks::app::pointer_down(x, y));
    rubiks::app::pointer_move(x - face_side, y);
    rubiks::app::pointer_up();
    settle();

    REQUIRE(rubiks::app::committed_move_count() == 0);
    require_solved_net();

    // With only the net on screen there is no viewpoint behind it either, so
    // the same press starts nothing at all.
    REQUIRE(rubiks::app::set_view_mode(rubiks::graphics::ViewMode::Flat));
    const auto only = rubiks::graphics::layout(kCanvas, kCanvas,
                                               rubiks::graphics::ViewMode::Flat)
                          .net;
    const float only_face =
        only.width / static_cast<float>(rubiks::graphics::kNetColumns);
    REQUIRE_FALSE(rubiks::app::pointer_down(only.x + 0.5f * only_face,
                                            only.y + 0.5f * only_face));

}

TEST_CASE("a net drag and a cube drag cannot run at once")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    drag_net_top_row_left();

    // One gesture at a time, whichever view it started in: the second press
    // is refused rather than layered on top of the first.
    const auto grab = front_column(2, kCanvas, kCanvas);
    REQUIRE_FALSE(rubiks::app::pointer_down(grab.x, grab.y));

    rubiks::app::pointer_up();
    settle();

    // Only the net drag's turn, and the refused press left nothing behind.
    REQUIRE(rubiks::app::committed_move_count() == 1);
    require_net_row(rubiks::cube::Face::Front, 0, kRed);
    require_up_column(2, kWhite);

}

TEST_CASE("a net drag too short to commit springs back")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    // Past the dead zone, nowhere near the threshold that commits.
    drag_net_cell(rubiks::cube::Face::Front, 1, 0, -0.2f, 0.0f);
    rubiks::app::pointer_up();
    settle();

    REQUIRE(rubiks::app::committed_move_count() == 0);
    REQUIRE(rubiks::app::is_solved());
    require_solved_net();

}

TEST_CASE("pressing away from the cube never turns a layer")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

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

}

TEST_CASE("cancelling a gesture leaves the cube alone")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

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

}

TEST_CASE("resizing during a drag abandons it")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    drag_upward(kQuarterTurnDrag);

    // The gesture holds screen directions from the old viewport, so it cannot
    // survive; dropping it is what keeps the angle from jumping.
    REQUIRE(rubiks::app::resize(800, 600));
    REQUIRE_FALSE(rubiks::app::advance(kFrameMs));

    rubiks::app::pointer_up();
    REQUIRE_FALSE(rubiks::app::advance(kFrameMs));

    REQUIRE(rubiks::app::resize(kCanvas, kCanvas));
    require_up_right_column(kWhite);

}

TEST_CASE("resizing during the snap lets it finish")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    drag_upward(kQuarterTurnDrag);
    rubiks::app::pointer_up();

    // The release already decided the turn, and the snap animates an angle
    // rather than reading the screen, so a resize cannot invalidate it.
    REQUIRE(rubiks::app::resize(800, 600));
    REQUIRE(settle() >= 1);

    REQUIRE(rubiks::app::resize(kCanvas, kCanvas));
    require_up_right_column(kGreen);

}

TEST_CASE("dragging the background sweeps the viewpoint")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    // Home shows the +Z, +X and +Y faces around the (1, 1, 1) diagonal.
    require_visible_faces(kUpLit, kFrontLit, kRightLit);

    // A quarter turn brings the next corner round, so the face that was on
    // the right moves to the left and the back face takes its place.
    orbit_left(1.0f);
    rubiks::app::pointer_up();
    settle();

    require_visible_faces(kUpLit, kRightLit, kBackLit);

    // The cube itself never moved.
    require_solved_net();

}

TEST_CASE("sweeping the other way brings the opposite corner round")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    // The other neighbouring corner: the front face slides right and the left
    // face comes into view beside it.
    orbit_left(-1.0f);
    rubiks::app::pointer_up();
    settle();

    require_visible_faces(kUpLit, kLeftLit, kFrontLit);
    require_solved_net();

}

TEST_CASE("a press between two frames sees the viewpoint it was aimed at")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

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

    require_visible_faces(kUpLit, kRightLit, kBackLit);

}

TEST_CASE("a resize keeps the viewpoint and applies the last sweep")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    orbit_left(1.0f);

    // The gesture cannot continue across a resize, but what it already swept
    // still counts.
    REQUIRE(rubiks::app::resize(800, 600));
    REQUIRE(rubiks::app::resize(kCanvas, kCanvas));
    settle();

    require_visible_faces(kUpLit, kRightLit, kBackLit);

}

TEST_CASE("scramble and reset keep the viewpoint the drag left behind")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    // A sweep no frame has published yet. Neither command owns the camera,
    // so both have to keep it rather than drop it with the gesture state.
    orbit_left(1.0f);
    rubiks::app::pointer_up();
    REQUIRE(rubiks::app::scramble(42U, 20U));
    settle();

    // Face centers never move under outer-face turns, so a scrambled cube
    // still names the faces around the viewpoint by the same three colors.
    require_visible_faces(kUpLit, kRightLit, kBackLit);

    orbit_left(1.0f);
    rubiks::app::pointer_up();
    rubiks::app::reset_cube();
    settle();

    require_visible_faces(kUpLit, kBackLit, kLeftLit);
    require_solved_net();

}

TEST_CASE("shutdown returns the viewpoint home")
{
    REQUIRE(rubiks::app::initialize(kCanvas, kCanvas));
    orbit_left(1.0f);
    rubiks::app::pointer_up();
    settle();
    require_visible_faces(kUpLit, kRightLit, kBackLit);

    rubiks::app::shutdown();
    REQUIRE(rubiks::app::initialize(kCanvas, kCanvas));

    // Back to the viewpoint the rendered scene contract was derived from.
    require_visible_faces(kUpLit, kFrontLit, kRightLit);

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
    require_visible_faces(kUpLit, kFrontLit, kRightLit);

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

namespace {

/** The rectangle the ring diagram fills when it is the only view. */
rubiks::graphics::Rect rings_rect()
{
    return rubiks::graphics::layout(kCanvas, kCanvas,
                                    rubiks::graphics::ViewMode::Flat,
                                    rubiks::graphics::FlatStyle::Rings)
        .rings;
}

/** Switches to the ring diagram alone and hands back its rectangle. */
rubiks::graphics::Rect show_rings()
{
    REQUIRE(rubiks::app::set_view_mode(rubiks::graphics::ViewMode::Flat));
    REQUIRE(
        rubiks::app::set_flat_style(rubiks::graphics::FlatStyle::Rings));
    return rings_rect();
}

/**
 * Drags a finger `slots` of the way round one ring, starting on a slot.
 *
 * Along the loop rather than straight across it, because that is what a hand
 * following the sticker it grabbed actually does -- and what the drawing does
 * with that sticker.
 */
void drag_round_ring(const rubiks::graphics::SlotRing& ring, float from,
                     float slots)
{
    const auto start = ring.at(from).position;
    REQUIRE(rubiks::app::pointer_down(start.x, start.y));

    constexpr int kSteps = 32;
    for (int step = 1; step <= kSteps; ++step) {
        const float along =
            from + slots * static_cast<float>(step) / static_cast<float>(kSteps);
        const auto at = ring.at(along).position;
        rubiks::app::pointer_move(at.x, at.y);
    }
}

}  // namespace

TEST_CASE("dragging a sticker round its ring turns that layer")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    const auto rect = show_rings();
    const auto ring = rubiks::graphics::rings_ring(rubiks::cube::Axis::X, 2,
                                                   rect, kCubeSize);
    REQUIRE_FALSE(ring.empty());

    // Three slots is a quarter turn on any ring, so a little past three is
    // comfortably past the commit boundary and short of the next one.
    drag_round_ring(ring, 0.0f, 3.4f);
    rubiks::app::pointer_up();
    settle();

    REQUIRE(rubiks::app::committed_move_count() == 1);
    REQUIRE_FALSE(rubiks::app::is_solved());

    // Exactly one positive quarter turn of the layer that ring belongs to,
    // which is R -- so R' puts the cube back and nothing else would.
    REQUIRE(rubiks::app::turn_face(rubiks::cube::Face::Right, 1, 1, -1));
    settle();
    REQUIRE(rubiks::app::is_solved());
}

TEST_CASE("a ring drag the other way turns the same layer back")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    const auto rect = show_rings();
    const auto ring = rubiks::graphics::rings_ring(rubiks::cube::Axis::X, 2,
                                                   rect, kCubeSize);

    // Slots run the way a positive turn carries stickers, so going back down
    // them is the negative turn -- there is no sign table to get wrong.
    drag_round_ring(ring, 6.0f, -3.4f);
    rubiks::app::pointer_up();
    settle();

    REQUIRE(rubiks::app::committed_move_count() == 1);
    REQUIRE(rubiks::app::turn_face(rubiks::cube::Face::Right, 1, 1, 1));
    settle();
    REQUIRE(rubiks::app::is_solved());
}

TEST_CASE("a ring drag carried right round the loop commits nothing")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    const auto rect = show_rings();
    const auto ring = rubiks::graphics::rings_ring(rubiks::cube::Axis::Y, 1,
                                                   rect, kCubeSize);

    // Twelve slots is the whole loop. The sweep is accumulated a step at a
    // time rather than measured from the press, so it reads as four quarter
    // turns rather than folding back to none part way round.
    drag_round_ring(ring, 0.0f, 12.0f);
    rubiks::app::pointer_up();
    settle();

    REQUIRE(rubiks::app::committed_move_count() == 0);
    REQUIRE(rubiks::app::is_solved());
}

TEST_CASE("a press away from the ring diagram's stickers starts nothing")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    const auto rect = show_rings();

    // A corner of the region: inside the view, outside every circle, and on no
    // sticker. Note that an axis's own centre would not do -- the middle
    // circles have the radius of the centre separation, so they run through
    // the other two centres and cross there, putting a sticker on each.
    const auto middle = rubiks::math::Vec2{rect.x + 2.0f, rect.y + 2.0f};

    // A drag on the cube, released, so a snap is running with a turn decided.
    drag_upward(kQuarterTurnDrag);
    rubiks::app::pointer_up();
    REQUIRE(rubiks::app::is_busy());
    REQUIRE(rubiks::app::committed_move_count() == 0);

    // The press misses, and being refused it cannot have confirmed the snap
    // on its way out. Phase 7 put every reason to refuse ahead of that.
    REQUIRE_FALSE(rubiks::app::pointer_down(middle.x, middle.y));
    REQUIRE(rubiks::app::committed_move_count() == 0);

    settle();
    REQUIRE(rubiks::app::committed_move_count() == 1);
}

TEST_CASE("a press over the cube while a sequence plays only looks around it")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    require_visible_faces(kUpLit, kFrontLit, kRightLit);
    REQUIRE(rubiks::app::scramble(21U, 6U));

    // Straight onto a sticker, which at any other moment takes hold of the
    // layer under it. The press reaches no pick at all here, so the drag that
    // follows sweeps the viewpoint instead.
    const auto grab = front_column(2, kCanvas, kCanvas);
    const auto rect = cube_rect(kCanvas, kCanvas);
    REQUIRE(rubiks::app::pointer_down(grab.x, grab.y));
    rubiks::app::pointer_move(
        grab.x -
            rubiks::interaction::kOrbitQuarterTurnFraction * rect.width,
        grab.y);
    rubiks::app::pointer_up();

    settle();

    // A quarter turn round, and the sequence still owns every move made.
    require_visible_faces(kUpLit, kRightLit, kBackLit);
    REQUIRE(rubiks::app::committed_move_count() == 0);
    REQUIRE_FALSE(rubiks::app::is_solved());
}

TEST_CASE("a sequence plays on while the viewpoint is being swept")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    REQUIRE(rubiks::app::scramble(21U, 6U));
    orbit_left(1.0f);

    // A sweep is not busy, so every move still finds the controller free. If
    // one did not, this loop would never see the sequence end.
    int frames = 0;
    while (rubiks::app::is_busy()) {
        REQUIRE(rubiks::app::advance(kFrameMs));
        ++frames;
        REQUIRE(frames < 1000);
    }

    rubiks::app::pointer_up();
    settle();

    require_visible_faces(kUpLit, kRightLit, kBackLit);
    REQUIRE(rubiks::app::committed_move_count() == 0);
    REQUIRE_FALSE(rubiks::app::is_solved());
}

TEST_CASE("a press while a sequence plays in the flat view starts nothing")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    // Both flat drawings in turn, so neither picking path is reached: the net
    // cell under the middle of the canvas, and a sticker of the diagram.
    for (const auto style :
         {rubiks::graphics::FlatStyle::Net,
          rubiks::graphics::FlatStyle::Rings}) {
        REQUIRE(rubiks::app::set_view_mode(rubiks::graphics::ViewMode::Flat));
        REQUIRE(rubiks::app::set_flat_style(style));
        REQUIRE(rubiks::app::scramble(21U, 6U));

        const auto rect = rings_rect();
        const auto ring =
            rubiks::graphics::rings_ring(rubiks::cube::Axis::X, 2, rect,
                                         kCubeSize);
        const auto at = style == rubiks::graphics::FlatStyle::Net
                            ? rubiks::math::Vec2{0.5f * kCanvas, 0.5f * kCanvas}
                            : ring.at(0.0f).position;

        // There is no cube on screen to look around, so a press has nothing
        // it may start. Sent to the empty cube region instead it would be
        // measured against a rectangle of no width, and the infinity that
        // yields would leave the viewpoint NaN for the rest of the session.
        REQUIRE_FALSE(rubiks::app::pointer_down(at.x, at.y));
        rubiks::app::pointer_move(at.x + 0.2f * kCanvas, at.y);
        rubiks::app::pointer_up();

        settle();
        REQUIRE(rubiks::app::committed_move_count() == 0);
        REQUIRE_FALSE(rubiks::app::is_solved());
    }

    // Back to the layout the face samples are calibrated against. A viewpoint
    // that had gone NaN could not draw the cube at all.
    REQUIRE(rubiks::app::set_view_mode(rubiks::graphics::ViewMode::Both));
    REQUIRE(rubiks::app::set_flat_style(rubiks::graphics::FlatStyle::Net));
    require_visible_faces(kUpLit, kFrontLit, kRightLit);
}

