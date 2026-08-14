#include "app/Application.hpp"

#include <catch2/catch_test_macros.hpp>

#include "EngineLifecycle.hpp"

namespace {

/** Advances the application until its current animation is settled. */
void settle_application()
{
    int frames = 0;
    while (rubiks::app::advance(16.0)) {
        ++frames;
        REQUIRE(frames < 1000);
    }
}

}  // namespace

TEST_CASE("calls before initialization fail")
{
    REQUIRE(!rubiks::app::is_initialized());
    REQUIRE(!rubiks::app::render());
    REQUIRE(!rubiks::app::resize(64, 64));
    REQUIRE(rubiks::app::pixel_buffer() == 0);
    REQUIRE(rubiks::app::pixel_byte_length() == 0);
}

TEST_CASE("initialize and shutdown are idempotent")
{
    REQUIRE(rubiks::app::initialize(64, 64));
    REQUIRE(rubiks::app::initialize(64, 64));
    REQUIRE(rubiks::app::is_initialized());
    REQUIRE(rubiks::app::pixel_buffer() != 0);
    REQUIRE(rubiks::app::pixel_byte_length() == 64u * 64u * 4u);
    REQUIRE(rubiks::app::render());

    rubiks::app::shutdown();
    rubiks::app::shutdown();
    REQUIRE(!rubiks::app::is_initialized());
    REQUIRE(!rubiks::app::render());
    REQUIRE(rubiks::app::pixel_buffer() == 0);
    REQUIRE(rubiks::app::pixel_byte_length() == 0);
}

TEST_CASE("the application can be reinitialized after shutdown")
{
    REQUIRE(rubiks::app::initialize(32, 32));
    REQUIRE(rubiks::app::render());
    rubiks::app::shutdown();
    REQUIRE(!rubiks::app::is_initialized());
}

TEST_CASE("scramble and reset own only the cube solve session")
{
    using rubiks::graphics::ViewMode;

    const rubiks::test::EngineLifecycle engine(256, 256);
    REQUIRE(rubiks::app::is_solved());
    REQUIRE(rubiks::app::committed_move_count() == 0);
    REQUIRE(rubiks::app::view_mode() == ViewMode::Both);

    REQUIRE(rubiks::app::set_view_mode(ViewMode::Cube3D));
    REQUIRE(rubiks::app::scramble(42U));
    REQUIRE_FALSE(rubiks::app::is_solved());
    REQUIRE(rubiks::app::committed_move_count() == 0);
    REQUIRE(rubiks::app::view_mode() == ViewMode::Cube3D);

    rubiks::app::reset_cube();
    REQUIRE(rubiks::app::is_solved());
    REQUIRE(rubiks::app::committed_move_count() == 0);
    REQUIRE(rubiks::app::view_mode() == ViewMode::Cube3D);
}

TEST_CASE("programmatic face turns commit and count like pointer moves")
{
    const rubiks::test::EngineLifecycle engine(256, 256);

    REQUIRE(rubiks::app::turn_face(rubiks::cube::Face::Right, 1));
    REQUIRE(rubiks::app::is_busy());
    REQUIRE_FALSE(
        rubiks::app::turn_face(rubiks::cube::Face::Up, 1));
    settle_application();
    REQUIRE_FALSE(rubiks::app::is_busy());
    REQUIRE_FALSE(rubiks::app::is_solved());
    REQUIRE(rubiks::app::committed_move_count() == 1);

    REQUIRE(rubiks::app::turn_face(rubiks::cube::Face::Right, -1));
    settle_application();
    REQUIRE(rubiks::app::is_solved());
    REQUIRE(rubiks::app::committed_move_count() == 2);

    REQUIRE_FALSE(rubiks::app::turn_face(rubiks::cube::Face::Right, 0));
    REQUIRE_FALSE(rubiks::app::turn_face(rubiks::cube::Face::Right, 3));
    REQUIRE_FALSE(rubiks::app::turn_face(
        static_cast<rubiks::cube::Face>(99), 1));
}

TEST_CASE("view modes preserve state and net mode takes pointer gestures")
{
    using rubiks::graphics::ViewMode;

    {
        const rubiks::test::EngineLifecycle engine(256, 256);
        REQUIRE(rubiks::app::scramble(7U));
        REQUIRE(rubiks::app::set_view_mode(ViewMode::Flat));
        REQUIRE(rubiks::app::view_mode() == ViewMode::Flat);
        REQUIRE(rubiks::app::render());

        // The center of the canvas is the center of the net block, so the net
        // is the one view on screen and it answers the press itself.
        REQUIRE(rubiks::app::pointer_down(128.0f, 128.0f));
        rubiks::app::pointer_cancel();
        REQUIRE_FALSE(rubiks::app::is_solved());

        REQUIRE(rubiks::app::set_view_mode(ViewMode::Both));
        REQUIRE(rubiks::app::view_mode() == ViewMode::Both);
        REQUIRE_FALSE(rubiks::app::is_solved());
        REQUIRE(rubiks::app::render());
    }
    REQUIRE(rubiks::app::view_mode() == ViewMode::Both);
}

TEST_CASE("the flat view keeps its style whichever regions are on screen")
{
    using rubiks::graphics::FlatStyle;
    using rubiks::graphics::ViewMode;

    const rubiks::test::EngineLifecycle engine(256, 256);

    REQUIRE(rubiks::app::scramble(11U));
    REQUIRE(rubiks::app::flat_style() == FlatStyle::Net);

    // The style is its own axis: it can be set while the flat view is not up
    // at all, and is waiting when it comes back.
    REQUIRE(rubiks::app::set_view_mode(ViewMode::Cube3D));
    REQUIRE(rubiks::app::set_flat_style(FlatStyle::Rings));
    REQUIRE(rubiks::app::flat_style() == FlatStyle::Rings);
    REQUIRE(rubiks::app::render());

    // Every one of the nine combinations draws: the two axes are a product,
    // not a list with the awkward pairings left out.
    for (const auto mode : {ViewMode::Cube3D, ViewMode::Both, ViewMode::Flat}) {
        for (const auto style :
             {FlatStyle::Net, FlatStyle::Rings, FlatStyle::Both}) {
            REQUIRE(rubiks::app::set_view_mode(mode));
            REQUIRE(rubiks::app::set_flat_style(style));
            REQUIRE(rubiks::app::view_mode() == mode);
            REQUIRE(rubiks::app::flat_style() == style);
            REQUIRE(rubiks::app::render());
        }
    }

    // Looking at the cube another way never changes it.
    REQUIRE_FALSE(rubiks::app::is_solved());

    // The value that used to be a fourth mode is not one again.
    REQUIRE_FALSE(rubiks::app::set_view_mode(static_cast<ViewMode>(3)));
    REQUIRE_FALSE(rubiks::app::set_flat_style(static_cast<FlatStyle>(3)));
}
