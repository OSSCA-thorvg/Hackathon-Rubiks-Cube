#include "app/Application.hpp"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "EngineLifecycle.hpp"
#include "cube/Assembly.hpp"
#include "cube/Cubie.hpp"
#include "graphics/Layout.hpp"
#include "graphics/NetGeometry.hpp"

// Colouring a real cube onto the net: what a draft is while it is open, what
// it takes to be believed, and what the application looks like afterwards.
// Whether a colouring is a cube at all is settled in tests/cube, which asks it
// of the domain without an application around it.

namespace {

using rubiks::test::EngineLifecycle;
using rubiks::test::settle;

constexpr std::uint32_t kCanvas = 512;

/** A point inside the net's middle face, which the cross always fills. */
struct Aim {
    float x;
    float y;
};

/**
 * The middle of one cell of the front face, in drawing-buffer pixels.
 *
 * The same arithmetic the other pointer tests aim with: the net is a cross of
 * blocks, each block a face, each face a grid of cells.
 */
[[nodiscard]] Aim front_cell(int col, int row, int size)
{
    const auto net = rubiks::graphics::layout(kCanvas, kCanvas).net;
    const float side =
        net.width / static_cast<float>(rubiks::graphics::kNetColumns);
    const float cell = side / static_cast<float>(size);
    const auto block = rubiks::graphics::net_block(rubiks::cube::Face::Front);

    return Aim{net.x + static_cast<float>(block.column) * side +
                   (static_cast<float>(col) + 0.5f) * cell,
               net.y + static_cast<float>(block.row) * side +
                   (static_cast<float>(row) + 0.5f) * cell};
}

/** How many stickers of the draft carry a colour. */
[[nodiscard]] int carried(rubiks::cube::FaceColor colour)
{
    return rubiks::app::painted_count(colour);
}

}  // namespace

TEST_CASE("a draft opens on the cube as it stands and closes without a trace")
{
    const EngineLifecycle engine(kCanvas, kCanvas);
    REQUIRE(rubiks::app::scramble(7, 12));
    settle();

    const auto before = rubiks::app::painting_draft();
    CHECK(before.empty());
    CHECK_FALSE(rubiks::app::is_painting());

    REQUIRE(rubiks::app::begin_painting());
    CHECK(rubiks::app::is_painting());

    // The draft starts as what is on the cube, because somebody copying a
    // muddled cube has far less to change than somebody starting from nothing.
    const auto opened = rubiks::app::painting_draft();
    CHECK(opened.size() == 9 * 6);
    for (const auto face : rubiks::cube::faces()) {
        CHECK(carried(rubiks::cube::solved_color(face)) == 9);
    }

    // Asking twice is not a way of asking for the first one to be thrown away.
    CHECK_FALSE(rubiks::app::begin_painting());

    const auto solved_before = rubiks::app::is_solved();
    rubiks::app::cancel_painting();
    CHECK_FALSE(rubiks::app::is_painting());
    CHECK(rubiks::app::is_solved() == solved_before);
}

TEST_CASE("a press on the net colours instead of turning while a draft is open")
{
    const EngineLifecycle engine(kCanvas, kCanvas);
    const auto record_before = rubiks::app::committed_move_count();

    REQUIRE(rubiks::app::begin_painting());
    REQUIRE(rubiks::app::set_brush(rubiks::cube::FaceColor::Red));

    const auto aim = front_cell(1, 1, 3);
    CHECK(rubiks::app::paint_at(aim.x, aim.y));

    // One green sticker became red, and nothing about the cube moved.
    CHECK(carried(rubiks::cube::FaceColor::Red) == 10);
    CHECK(carried(rubiks::cube::FaceColor::Green) == 8);
    CHECK(rubiks::app::committed_move_count() == record_before);
    CHECK(rubiks::app::is_solved());

    // And the same press arriving as a gesture goes the same way: a drag
    // across the net carries the brush rather than a layer.
    CHECK(rubiks::app::pointer_down(aim.x, aim.y));
    const auto along = front_cell(2, 1, 3);
    rubiks::app::pointer_move(along.x, along.y);
    rubiks::app::pointer_up();
    settle();

    CHECK(carried(rubiks::cube::FaceColor::Red) == 11);
    CHECK(rubiks::app::committed_move_count() == record_before);
    CHECK(rubiks::app::is_solved());
}

TEST_CASE("a face is filled in one stroke")
{
    const EngineLifecycle engine(kCanvas, kCanvas);
    REQUIRE(rubiks::app::set_cube_size(6));
    REQUIRE(rubiks::app::begin_painting());
    REQUIRE(rubiks::app::set_brush(rubiks::cube::FaceColor::Yellow));

    const auto aim = front_cell(2, 3, 6);
    CHECK(rubiks::app::fill_face_at(aim.x, aim.y));

    // Thirty-six of one face, on top of the thirty-six that were already
    // yellow on the face below. A six by six is two hundred and sixteen cells
    // and a twenty-eight is four thousand seven hundred, so this is the
    // difference between the feature being usable at those sizes and not.
    CHECK(carried(rubiks::cube::FaceColor::Yellow) == 72);
    CHECK(carried(rubiks::cube::FaceColor::Green) == 0);
}

TEST_CASE("a draft of the cube as it stands is applied and becomes the start")
{
    const EngineLifecycle engine(kCanvas, kCanvas);
    REQUIRE(rubiks::app::scramble(3, 15));
    settle();

    REQUIRE(rubiks::app::timeline_length() > 0);

    REQUIRE(rubiks::app::begin_painting());
    REQUIRE(rubiks::app::apply_painting());

    CHECK_FALSE(rubiks::app::is_painting());
    CHECK_FALSE(rubiks::app::is_solved());

    // What arrives is a starting position and not a move: the record goes, the
    // way it goes for a change of size. A rewind afterwards comes back to this
    // cube rather than to a solved one, which is what somebody who painted
    // their own cube would take it to mean.
    CHECK(rubiks::app::timeline_length() == 0);
    CHECK(rubiks::app::committed_move_count() == 0);

    // And it is still a cube a solver will take on.
    CHECK(rubiks::app::can_solve());
    REQUIRE(rubiks::app::solve());
    settle();
    CHECK(rubiks::app::is_solved());
}

TEST_CASE("a colouring no turning reaches is refused and says where")
{
    const EngineLifecycle engine(kCanvas, kCanvas);
    REQUIRE(rubiks::app::begin_painting());
    REQUIRE(rubiks::app::set_brush(rubiks::cube::FaceColor::Red));

    const auto aim = front_cell(0, 0, 3);
    REQUIRE(rubiks::app::paint_at(aim.x, aim.y));

    CHECK_FALSE(rubiks::app::apply_painting());
    CHECK(rubiks::app::painting_fault() == rubiks::cube::PaintFault::ColourCount);
    CHECK(rubiks::app::painting_blamed_count() > 0);
    CHECK(rubiks::app::painting_blamed_at(0) >= 0);
    CHECK(rubiks::app::painting_blamed_at(-1) == -1);
    CHECK(rubiks::app::painting_blamed_at(rubiks::app::painting_blamed_count()) ==
          -1);

    // A refusal leaves the draft open and exactly as it was, so what is wrong
    // stays on screen while it is being mended.
    CHECK(rubiks::app::is_painting());
    CHECK(carried(rubiks::cube::FaceColor::Red) == 10);
    CHECK(rubiks::app::is_solved());

    // Mended, it goes through.
    REQUIRE(rubiks::app::set_brush(rubiks::cube::FaceColor::Green));
    REQUIRE(rubiks::app::paint_at(aim.x, aim.y));
    CHECK(rubiks::app::apply_painting());
    CHECK(rubiks::app::is_solved());
}

TEST_CASE("a draft is not taken of a cube in motion, and does not outlive it")
{
    const EngineLifecycle engine(kCanvas, kCanvas);

    // Mid-scramble the cube is at a moment nobody chose, so there is nothing
    // there worth copying.
    REQUIRE(rubiks::app::scramble(1, 20));
    REQUIRE(rubiks::app::is_busy());
    CHECK_FALSE(rubiks::app::begin_painting());
    settle();

    REQUIRE(rubiks::app::begin_painting());
    REQUIRE(rubiks::app::set_cube_size(4));
    CHECK_FALSE(rubiks::app::is_painting());

    // A draft is a colouring of one cube of one size, so every command that
    // puts a different cube there closes it rather than leaving it describing
    // something that is gone.
    REQUIRE(rubiks::app::begin_painting());
    rubiks::app::reset_cube();
    CHECK_FALSE(rubiks::app::is_painting());

    REQUIRE(rubiks::app::begin_painting());
    REQUIRE(rubiks::app::scramble(2, 4));
    CHECK_FALSE(rubiks::app::is_painting());
    settle();
}

TEST_CASE("nothing about painting answers before the engine is up")
{
    CHECK_FALSE(rubiks::app::is_painting());
    CHECK_FALSE(rubiks::app::begin_painting());
    CHECK_FALSE(rubiks::app::apply_painting());
    CHECK_FALSE(rubiks::app::paint_at(0.0f, 0.0f));
    CHECK_FALSE(rubiks::app::fill_face_at(0.0f, 0.0f));
    CHECK_FALSE(rubiks::app::set_brush(rubiks::cube::FaceColor::Blue));
    CHECK(rubiks::app::painted_count(rubiks::cube::FaceColor::Blue) == 0);
    CHECK(rubiks::app::painting_blamed_count() == 0);
    CHECK(rubiks::app::painting_draft().empty());
    rubiks::app::cancel_painting();
}
