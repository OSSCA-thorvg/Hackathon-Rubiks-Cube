#include "app/Application.hpp"

#include <catch2/catch_test_macros.hpp>

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

    REQUIRE(rubiks::app::initialize(256, 256));
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

    rubiks::app::shutdown();
}

TEST_CASE("programmatic face turns commit and count like pointer moves")
{
    REQUIRE(rubiks::app::initialize(256, 256));

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
    rubiks::app::shutdown();
}

TEST_CASE("view modes preserve state and net mode declines pointer gestures")
{
    using rubiks::graphics::ViewMode;

    REQUIRE(rubiks::app::initialize(256, 256));
    REQUIRE(rubiks::app::scramble(7U));
    REQUIRE(rubiks::app::set_view_mode(ViewMode::Net));
    REQUIRE(rubiks::app::view_mode() == ViewMode::Net);
    REQUIRE(rubiks::app::render());
    REQUIRE_FALSE(rubiks::app::pointer_down(128.0f, 128.0f));
    REQUIRE_FALSE(rubiks::app::is_solved());

    REQUIRE(rubiks::app::set_view_mode(ViewMode::Both));
    REQUIRE(rubiks::app::view_mode() == ViewMode::Both);
    REQUIRE_FALSE(rubiks::app::is_solved());
    REQUIRE(rubiks::app::render());

    rubiks::app::shutdown();
    REQUIRE(rubiks::app::view_mode() == ViewMode::Both);
}
