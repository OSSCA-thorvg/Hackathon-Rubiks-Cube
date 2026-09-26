#include "app/Application.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "EngineLifecycle.hpp"
#include "cube/Cubie.hpp"
#include "graphics/Layout.hpp"
#include "graphics/NetGeometry.hpp"

// Surfaces: more than one canvas drawn by one engine. Each holds the scenes it
// shows and has a size of its own; where the canvases go on a page is the
// page's, and what is in each of them is laid out by the one layout a single
// canvas has always used. Surface 0 is the single canvas, and every other test
// in this directory is about it.

namespace {

using Rgba = std::array<std::uint8_t, 4>;
using rubiks::app::kSceneAxes;
using rubiks::app::kSceneCube;
using rubiks::app::kSceneNet;
using rubiks::app::kSceneRings;
using rubiks::test::kFrameMs;
using rubiks::test::settle;

constexpr Rgba kDarkGround{32, 32, 32, 255};
constexpr Rgba kLightGround{231, 235, 240, 255};
constexpr Rgba kWhite{255, 255, 255, 255};
constexpr Rgba kGreen{0, 155, 72, 255};
constexpr Rgba kRed{183, 18, 52, 255};

constexpr std::uint32_t kCubeSide = 512;
constexpr std::uint32_t kNetWidth = 480;
constexpr std::uint32_t kNetHeight = 360;
constexpr std::uint32_t kAxesSide = 96;

/** The surfaces a page with a canvas per view would make. */
constexpr std::uint32_t kCube = 0;
constexpr std::uint32_t kNet = 1;
constexpr std::uint32_t kAxes = 2;

/** One pixel of a surface's last frame. */
Rgba pixel(std::uint32_t id, std::uint32_t width, float x, float y)
{
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(
        rubiks::app::surface_pixel_buffer(id));
    REQUIRE(bytes != nullptr);
    const std::size_t at =
        (static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) * 4;
    return Rgba{bytes[at], bytes[at + 1], bytes[at + 2], bytes[at + 3]};
}

/** Where the net sits on a surface holding the net alone. */
rubiks::graphics::Rect net_alone()
{
    return rubiks::graphics::layout(kNetWidth, kNetHeight,
                                    rubiks::graphics::ViewMode::Flat,
                                    rubiks::graphics::FlatStyle::Net)
        .net;
}

/** The middle of one net cell, in the net surface's pixels. */
rubiks::math::Vec2 net_cell(rubiks::cube::Face face, int column, int row)
{
    const auto net = net_alone();
    const float face_side =
        net.width / static_cast<float>(rubiks::graphics::kNetColumns);
    const float cell = face_side / 3.0f;
    const auto block = rubiks::graphics::net_block(face);
    return rubiks::math::Vec2{
        net.x + static_cast<float>(block.column) * face_side +
            (static_cast<float>(column) + 0.5f) * cell,
        net.y + static_cast<float>(block.row) * face_side +
            (static_cast<float>(row) + 0.5f) * cell};
}

Rgba net_pixel(rubiks::cube::Face face, int column, int row)
{
    const auto at = net_cell(face, column, row);
    return pixel(kNet, kNetWidth, at.x, at.y);
}

/**
 * A page with the cube, the net and the axes on three canvases.
 *
 * Surface 0 is given the cube alone -- it starts out holding everything --
 * and the other two are given a size and a scene each.
 */
void open_three_canvases()
{
    REQUIRE(rubiks::app::set_surface_scenes(kCube, kSceneCube));
    REQUIRE(rubiks::app::resize_surface(kNet, kNetWidth, kNetHeight));
    REQUIRE(rubiks::app::set_surface_scenes(kNet, kSceneNet));
    REQUIRE(rubiks::app::resize_surface(kAxes, kAxesSide, kAxesSide));
    REQUIRE(rubiks::app::set_surface_scenes(kAxes, kSceneAxes));
}

/** Sweeps the viewpoint a little, from the cube surface's empty corner. */
void sweep_viewpoint()
{
    REQUIRE(rubiks::app::pointer_down_on(kCube, 4.0f, 4.0f));
    rubiks::app::pointer_move(64.0f, 4.0f);
    static_cast<void>(rubiks::app::advance(kFrameMs));
    rubiks::app::pointer_up();
    settle();
}

}  // namespace

TEST_CASE("surface 0 holds every scene, and the others start with nothing")
{
    const rubiks::test::EngineLifecycle engine(kCubeSide, kCubeSide);

    REQUIRE(rubiks::app::surface_scenes(0) == rubiks::app::kAllScenes);
    for (std::uint32_t id = 1; id < rubiks::app::kSurfaceCount; ++id) {
        REQUIRE(rubiks::app::surface_scenes(id) == 0);
        REQUIRE(rubiks::app::surface_pixel_buffer(id) == 0);
    }

    // The single-canvas calls are surface 0's.
    REQUIRE(rubiks::app::pixel_buffer() == rubiks::app::surface_pixel_buffer(0));
    REQUIRE(rubiks::app::pixel_byte_length() == kCubeSide * kCubeSide * 4);
}

TEST_CASE("a surface holding one scene draws it alone, filling the surface")
{
    const rubiks::test::EngineLifecycle engine(kCubeSide, kCubeSide);
    open_three_canvases();
    REQUIRE(rubiks::app::render());

    // The net is the net-alone layout of its own surface, stickers and all.
    REQUIRE(net_pixel(rubiks::cube::Face::Up, 1, 1) == kWhite);
    REQUIRE(net_pixel(rubiks::cube::Face::Front, 0, 2) == kGreen);
    REQUIRE(net_pixel(rubiks::cube::Face::Right, 2, 0) == kRed);
    REQUIRE(pixel(kNet, kNetWidth, 2.0f, 2.0f) == kDarkGround);

    // The cube is the cube-alone layout: a square in the middle of its
    // surface, with nothing of the net below it any more.
    const auto cube = rubiks::graphics::layout(kCubeSide, kCubeSide,
                                               rubiks::graphics::ViewMode::Cube3D)
                          .cube;
    REQUIRE(pixel(kCube, kCubeSide, cube.x + cube.width * 0.5f,
                  cube.y + cube.height * 0.29f) != kDarkGround);
    REQUIRE(pixel(kCube, kCubeSide, 2.0f, 2.0f) == kDarkGround);

    // The axes meet in the middle of theirs, and leave its corners bare.
    REQUIRE(pixel(kAxes, kAxesSide, kAxesSide * 0.5f, kAxesSide * 0.5f) !=
            kDarkGround);
    REQUIRE(pixel(kAxes, kAxesSide, 1.0f, kAxesSide - 2.0f) == kDarkGround);
}

TEST_CASE("a surface draws only what the view mode shows")
{
    const rubiks::test::EngineLifecycle engine(kCubeSide, kCubeSide);
    open_three_canvases();

    // The cube alone on screen: the net surface is bare ground, and so is the
    // cube's, once the cube is not shown either.
    REQUIRE(rubiks::app::set_view_mode(rubiks::graphics::ViewMode::Cube3D));
    REQUIRE(rubiks::app::render());
    REQUIRE(net_pixel(rubiks::cube::Face::Up, 1, 1) == kDarkGround);

    REQUIRE(rubiks::app::set_view_mode(rubiks::graphics::ViewMode::Flat));
    REQUIRE(rubiks::app::render());
    REQUIRE(net_pixel(rubiks::cube::Face::Up, 1, 1) == kWhite);
    REQUIRE(pixel(kCube, kCubeSide, kCubeSide * 0.5f, kCubeSide * 0.35f) ==
            kDarkGround);

    // The axes go with the cube, because they say where its axes point.
    REQUIRE(pixel(kAxes, kAxesSide, kAxesSide * 0.5f, kAxesSide * 0.5f) ==
            kDarkGround);
}

TEST_CASE("a press lands on the surface it was made on")
{
    const rubiks::test::EngineLifecycle engine(kCubeSide, kCubeSide);
    open_three_canvases();

    // A drag of the net's front top row to the left, in the net surface's
    // own pixels, is U -- the same as it is on one canvas.
    const auto from = net_cell(rubiks::cube::Face::Front, 1, 0);
    const float face_side =
        net_alone().width / static_cast<float>(rubiks::graphics::kNetColumns);
    REQUIRE(rubiks::app::pointer_down_on(kNet, from.x, from.y));
    rubiks::app::pointer_move(from.x - 0.6f * face_side, from.y);
    rubiks::app::pointer_up();
    settle();
    REQUIRE(rubiks::app::committed_move_count() == 1);

    REQUIRE(rubiks::app::turn_face(rubiks::cube::Face::Up, 1, 1, -1));
    settle();
    REQUIRE(rubiks::app::is_solved());

    // Off the net on a surface holding nothing else, a press starts nothing:
    // there is no viewpoint there to sweep.
    REQUIRE_FALSE(rubiks::app::pointer_down_on(kNet, 1.0f, 1.0f));

    // The axes hold no layers and no viewpoint of their own either.
    REQUIRE_FALSE(rubiks::app::pointer_down_on(kAxes, kAxesSide * 0.5f,
                                               kAxesSide * 0.5f));
}

TEST_CASE("a press outside the cube's surface still sweeps its viewpoint")
{
    const rubiks::test::EngineLifecycle engine(kCubeSide, kCubeSide);
    open_three_canvases();
    REQUIRE(rubiks::app::render());
    const std::uint32_t before = rubiks::app::surface_frame(kCube);

    // A page may hand over a press from beside the canvas, in the canvas's
    // pixels -- which puts it outside the surface. It is background there as
    // much as anywhere, so it looks around.
    REQUIRE(rubiks::app::pointer_down_on(kCube, -40.0f, 100.0f));
    rubiks::app::pointer_move(20.0f, 100.0f);
    static_cast<void>(rubiks::app::advance(kFrameMs));
    rubiks::app::pointer_up();
    settle();

    REQUIRE(rubiks::app::committed_move_count() == 0);
    REQUIRE(rubiks::app::render());
    REQUIRE(rubiks::app::surface_frame(kCube) != before);
}

TEST_CASE("a surface whose picture has not changed keeps its frame")
{
    const rubiks::test::EngineLifecycle engine(kCubeSide, kCubeSide);
    open_three_canvases();
    REQUIRE(rubiks::app::render());

    const auto frames = []() {
        return std::array<std::uint32_t, 3>{rubiks::app::surface_frame(kCube),
                                            rubiks::app::surface_frame(kNet),
                                            rubiks::app::surface_frame(kAxes)};
    };
    const auto drawn = frames();

    // Nothing moved, so nothing is drawn.
    REQUIRE(rubiks::app::render());
    REQUIRE(frames() == drawn);

    // A sweep of the viewpoint moves the cube and the axes, never the net.
    sweep_viewpoint();
    REQUIRE(rubiks::app::render());
    auto swept = frames();
    REQUIRE(swept[0] != drawn[0]);
    REQUIRE(swept[1] == drawn[1]);
    REQUIRE(swept[2] != drawn[2]);

    // A turn moves the cube and the net, never the axes.
    REQUIRE(rubiks::app::turn_face(rubiks::cube::Face::Right, 1, 1, 1));
    settle();
    REQUIRE(rubiks::app::render());
    const auto turned = frames();
    REQUIRE(turned[0] != swept[0]);
    REQUIRE(turned[1] != swept[1]);
    REQUIRE(turned[2] == swept[2]);

    // A cube turned away and back again between two frames is the cube that
    // was drawn: nothing to draw, however many commits there were.
    REQUIRE(rubiks::app::turn_face(rubiks::cube::Face::Up, 1, 1, 1));
    settle();
    REQUIRE(rubiks::app::turn_face(rubiks::cube::Face::Up, 1, 1, -1));
    settle();
    REQUIRE(rubiks::app::render());
    REQUIRE(frames() == turned);

    // Asking for every surface is what a measurement does.
    rubiks::app::invalidate();
    REQUIRE(rubiks::app::render());
    const auto again = frames();
    for (std::size_t i = 0; i < again.size(); ++i) {
        REQUIRE(again[i] == turned[i] + 1);
    }
}

TEST_CASE("a change of look reaches every surface, even one sized after it")
{
    const rubiks::test::EngineLifecycle engine(kCubeSide, kCubeSide);
    open_three_canvases();
    REQUIRE(rubiks::app::render());

    REQUIRE(rubiks::app::set_canvas_theme(rubiks::graphics::CanvasTheme::Light));
    REQUIRE(rubiks::app::resize_surface(3, 64, 64));
    REQUIRE(rubiks::app::set_surface_scenes(3, kSceneRings));
    REQUIRE(rubiks::app::render());

    REQUIRE(pixel(kCube, kCubeSide, 2.0f, 2.0f) == kLightGround);
    REQUIRE(pixel(kNet, kNetWidth, 2.0f, 2.0f) == kLightGround);
    REQUIRE(pixel(kAxes, kAxesSide, 1.0f, 1.0f) == kLightGround);
    REQUIRE(pixel(3, 64, 1.0f, 1.0f) == kLightGround);
}

TEST_CASE("zero by zero puts a surface away until it is given a size again")
{
    const rubiks::test::EngineLifecycle engine(kCubeSide, kCubeSide);
    open_three_canvases();
    REQUIRE(rubiks::app::render());
    const std::uint32_t before = rubiks::app::surface_frame(kNet);

    REQUIRE(rubiks::app::resize_surface(kNet, 0, 0));
    REQUIRE(rubiks::app::surface_pixel_buffer(kNet) == 0);
    REQUIRE(rubiks::app::surface_pixel_byte_length(kNet) == 0);
    REQUIRE_FALSE(rubiks::app::pointer_down_on(kNet, 10.0f, 10.0f));
    REQUIRE(rubiks::app::render());
    REQUIRE(rubiks::app::surface_frame(kNet) == before);

    // Given one again, it is drawn again, whether or not the cube moved.
    REQUIRE(rubiks::app::resize_surface(kNet, kNetWidth, kNetHeight));
    REQUIRE(rubiks::app::render());
    REQUIRE(rubiks::app::surface_frame(kNet) == before + 1);
    REQUIRE(net_pixel(rubiks::cube::Face::Up, 1, 1) == kWhite);
}

TEST_CASE("a drag on one surface survives another surface being resized")
{
    const rubiks::test::EngineLifecycle engine(kCubeSide, kCubeSide);
    open_three_canvases();

    const auto from = net_cell(rubiks::cube::Face::Front, 1, 0);
    const float face_side =
        net_alone().width / static_cast<float>(rubiks::graphics::kNetColumns);
    REQUIRE(rubiks::app::pointer_down_on(kNet, from.x, from.y));

    // The page moved the cube's canvas under the drag; the net's did not.
    REQUIRE(rubiks::app::resize_surface(kCube, 400, 400));

    rubiks::app::pointer_move(from.x - 0.6f * face_side, from.y);
    rubiks::app::pointer_up();
    settle();
    REQUIRE(rubiks::app::committed_move_count() == 1);

    // Resizing the surface the drag is on still drops it, as it always has.
    REQUIRE(rubiks::app::pointer_down_on(kNet, from.x, from.y));
    REQUIRE(rubiks::app::resize_surface(kNet, kNetWidth + 8, kNetHeight + 6));
    rubiks::app::pointer_move(from.x - 0.6f * face_side, from.y);
    rubiks::app::pointer_up();
    settle();
    REQUIRE(rubiks::app::committed_move_count() == 1);
}

TEST_CASE("a colouring stroke stays on the surface it began on")
{
    const rubiks::test::EngineLifecycle engine(kCubeSide, kCubeSide);
    open_three_canvases();

    REQUIRE(rubiks::app::begin_painting());
    REQUIRE(rubiks::app::set_brush(rubiks::cube::FaceColor::Red));
    const int red = rubiks::app::painted_count(rubiks::cube::FaceColor::Red);

    const auto first = net_cell(rubiks::cube::Face::Up, 0, 0);
    const auto second = net_cell(rubiks::cube::Face::Up, 1, 0);
    REQUIRE(rubiks::app::pointer_down_on(kNet, first.x, first.y));
    rubiks::app::pointer_move(second.x, second.y);
    rubiks::app::pointer_up();

    REQUIRE(rubiks::app::painted_count(rubiks::cube::FaceColor::Red) == red + 2);

    // The net surface shows the draft; the net-free cube surface cannot take
    // a stroke at all.
    REQUIRE(rubiks::app::render());
    REQUIRE(net_pixel(rubiks::cube::Face::Up, 0, 0) == kRed);
    REQUIRE_FALSE(rubiks::app::pointer_down_on(kCube, 100.0f, 100.0f));
}

TEST_CASE("surfaces and scenes the engine does not have are refused")
{
    const rubiks::test::EngineLifecycle engine(kCubeSide, kCubeSide);
    const std::uint32_t none = rubiks::app::kSurfaceCount;

    REQUIRE_FALSE(rubiks::app::set_surface_scenes(none, kSceneCube));
    REQUIRE_FALSE(rubiks::app::set_surface_scenes(1, 1u << 7));
    REQUIRE_FALSE(rubiks::app::resize_surface(none, 10, 10));
    REQUIRE_FALSE(rubiks::app::resize_surface(1, 0, 10));
    REQUIRE_FALSE(rubiks::app::pointer_down_on(none, 1.0f, 1.0f));
    REQUIRE(rubiks::app::surface_scenes(none) == 0);
    REQUIRE(rubiks::app::surface_frame(none) == 0);
    REQUIRE(rubiks::app::surface_pixel_buffer(none) == 0);

    // Surface 0 keeps its old contract: it is always some size.
    REQUIRE_FALSE(rubiks::app::resize(0, 0));
    REQUIRE(rubiks::app::surface_pixel_buffer(0) != 0);
}

TEST_CASE("surface calls are refused before initialization")
{
    REQUIRE_FALSE(rubiks::app::is_initialized());
    REQUIRE_FALSE(rubiks::app::resize_surface(1, 10, 10));
    REQUIRE_FALSE(rubiks::app::set_surface_scenes(1, kSceneNet));
    REQUIRE_FALSE(rubiks::app::pointer_down_on(1, 1.0f, 1.0f));
    REQUIRE(rubiks::app::surface_frame(1) == 0);
    REQUIRE(rubiks::app::surface_pixel_buffer(1) == 0);
    rubiks::app::invalidate();
}
