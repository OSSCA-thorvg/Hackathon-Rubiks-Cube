#include "app/Application.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "EngineLifecycle.hpp"
#include "cube/CubeMove.hpp"
#include "cube/Cubie.hpp"
#include "cube/Scramble.hpp"

namespace {

constexpr int kSize = 3;

using rubiks::test::kFrameMs;
using rubiks::test::settle;

/**
 * The face whose clockwise turn is the given axis and layer, with its sign.
 *
 * The named faces are how the outside asks for a turn, and a scramble is made
 * of nothing but outer faces, so every move of one can be spoken back through
 * `turn_face`. The sign is there because half of the six turn negatively about
 * their axis.
 *
 * Read out of the named moves rather than restated here. Which layer each face
 * is and which way it turns about its axis are facts the domain already holds,
 * and a second copy of them would go on agreeing after the first had changed.
 */
struct NamedFace {
    rubiks::cube::Face face;
    int sign;
};

[[nodiscard]] NamedFace named_face(const rubiks::cube::CubeMove& move)
{
    using namespace rubiks::cube::moves;
    const std::array<rubiks::cube::CubeMove, rubiks::cube::kFaceCount> named{
        R(kSize), L(kSize), U(kSize), D(kSize), F(kSize), B(kSize)};
    const auto all = rubiks::cube::faces();

    for (std::size_t index = 0; index < all.size(); ++index) {
        if (named[index].axis == move.axis &&
            named[index].layers == move.layers) {
            return NamedFace{all[index], named[index].quarter_turns};
        }
    }

    FAIL("no outer face turns this layer");
    return NamedFace{all[0], 1};
}

/**
 * Turns a sequence back, one settled move at a time.
 *
 * How a played sequence is checked against the moves it was given: undoing
 * exactly those moves leaves a solved cube, and any move the playback dropped,
 * repeated, or took in the wrong order leaves it scrambled.
 */
void undo_through_faces(const std::vector<rubiks::cube::CubeMove>& plan)
{
    for (auto move = plan.rbegin(); move != plan.rend(); ++move) {
        const NamedFace named = named_face(*move);

        // A half turn undoes itself, and -2 is not one of the three amounts a
        // face turn is asked for.
        const int turns = std::abs(move->quarter_turns) == 2
                              ? 2
                              : -move->quarter_turns * named.sign;

        REQUIRE(rubiks::app::turn_face(named.face, turns));
        settle();
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

    // The scramble is turned rather than applied, so the cube is still solved
    // here and reaches the scrambled state only once the sequence has played.
    REQUIRE(rubiks::app::scramble(42U, 20U));
    REQUIRE(rubiks::app::is_solved());
    REQUIRE(rubiks::app::is_busy());

    settle();
    REQUIRE_FALSE(rubiks::app::is_busy());
    REQUIRE_FALSE(rubiks::app::is_solved());
    REQUIRE(rubiks::app::committed_move_count() == 0);
    REQUIRE(rubiks::app::view_mode() == ViewMode::Cube3D);

    rubiks::app::reset_cube();
    REQUIRE(rubiks::app::is_solved());
    REQUIRE(rubiks::app::committed_move_count() == 0);
    REQUIRE(rubiks::app::view_mode() == ViewMode::Cube3D);
}

TEST_CASE("a played scramble applies exactly the sequence it was given")
{
    const rubiks::test::EngineLifecycle engine(256, 256);
    const auto plan = rubiks::cube::make_scramble(kSize, 1234U, 12U);
    REQUIRE(plan.size() == 12);

    REQUIRE(rubiks::app::scramble(1234U, 12U));
    settle();
    REQUIRE_FALSE(rubiks::app::is_solved());

    // Anything dropped, repeated, or taken out of order survives this.
    undo_through_faces(plan);
    REQUIRE(rubiks::app::is_solved());
}

TEST_CASE("played moves are nobody's, and the cube is busy until the last one")
{
    const rubiks::test::EngineLifecycle engine(256, 256);

    REQUIRE(rubiks::app::scramble(5U, 4U));

    // Busy from the moment it is accepted, before a single frame has run --
    // which is the window a keyboard turn would otherwise slip through.
    REQUIRE(rubiks::app::is_busy());
    REQUIRE_FALSE(rubiks::app::turn_face(rubiks::cube::Face::Right, 1));

    // Busy without a break from there to the end, and every frame it is busy
    // it asks for another -- between two of its moves nothing is animating,
    // so without the sequence in that answer the loop would stop halfway.
    int frames = 0;
    while (rubiks::app::is_busy()) {
        REQUIRE(rubiks::app::advance(kFrameMs));
        ++frames;
        REQUIRE(frames < 1000);
    }

    REQUIRE(frames > 0);
    REQUIRE_FALSE(rubiks::app::advance(kFrameMs));
    REQUIRE(rubiks::app::committed_move_count() == 0);

    // A move of the user's own counts again straight away.
    REQUIRE(rubiks::app::turn_face(rubiks::cube::Face::Right, 1));
    settle();
    REQUIRE(rubiks::app::committed_move_count() == 1);
}

TEST_CASE("a scramble count outside the range changes nothing")
{
    const rubiks::test::EngineLifecycle engine(256, 256);

    REQUIRE_FALSE(rubiks::app::scramble(1U, 0U));
    REQUIRE_FALSE(
        rubiks::app::scramble(1U, rubiks::app::kMaxScrambleMoves + 1U));

    // Refused before anything was touched: no playback, and the cube it would
    // have restarted is still the one that was there.
    REQUIRE_FALSE(rubiks::app::is_busy());
    REQUIRE(rubiks::app::is_solved());

    REQUIRE(rubiks::app::scramble(1U, 1U));
    settle();
    REQUIRE_FALSE(rubiks::app::is_solved());
}

TEST_CASE("resizing and switching views leave a playing sequence alone")
{
    using rubiks::graphics::FlatStyle;
    using rubiks::graphics::ViewMode;

    const rubiks::test::EngineLifecycle engine(256, 256);
    const auto plan = rubiks::cube::make_scramble(kSize, 9U, 8U);

    REQUIRE(rubiks::app::scramble(9U, 8U));

    // A phone turned sideways part way through a scramble. Dropping the
    // sequence here would leave the cube half scrambled with nothing busy.
    for (int frame = 0; frame < 20; ++frame) {
        static_cast<void>(rubiks::app::advance(kFrameMs));
    }
    REQUIRE(rubiks::app::resize(320, 200));
    REQUIRE(rubiks::app::set_view_mode(ViewMode::Flat));
    REQUIRE(rubiks::app::set_flat_style(FlatStyle::Rings));
    REQUIRE(rubiks::app::is_busy());

    settle();
    undo_through_faces(plan);
    REQUIRE(rubiks::app::is_solved());
}

TEST_CASE("discarding a sequence stops it where the last commit left it")
{
    const rubiks::test::EngineLifecycle engine(256, 256);

    REQUIRE(rubiks::app::scramble(3U, 10U));

    // Part way in, with a turn of the sequence in flight. The player and that
    // turn go together: left behind, it would commit onto the fresh cube some
    // frames later and reset would have quietly kept one move of the old one.
    for (int frame = 0; frame < 12; ++frame) {
        static_cast<void>(rubiks::app::advance(kFrameMs));
    }

    rubiks::app::reset_cube();
    REQUIRE_FALSE(rubiks::app::is_busy());
    REQUIRE(rubiks::app::is_solved());

    settle();
    REQUIRE(rubiks::app::is_solved());
    REQUIRE(rubiks::app::committed_move_count() == 0);
}

TEST_CASE("programmatic face turns commit and count like pointer moves")
{
    const rubiks::test::EngineLifecycle engine(256, 256);

    REQUIRE(rubiks::app::turn_face(rubiks::cube::Face::Right, 1));
    REQUIRE(rubiks::app::is_busy());
    REQUIRE_FALSE(
        rubiks::app::turn_face(rubiks::cube::Face::Up, 1));
    settle();
    REQUIRE_FALSE(rubiks::app::is_busy());
    REQUIRE_FALSE(rubiks::app::is_solved());
    REQUIRE(rubiks::app::committed_move_count() == 1);

    REQUIRE(rubiks::app::turn_face(rubiks::cube::Face::Right, -1));
    settle();
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
        REQUIRE(rubiks::app::scramble(7U, 20U));
        settle();
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

    REQUIRE(rubiks::app::scramble(11U, 20U));
    settle();
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
