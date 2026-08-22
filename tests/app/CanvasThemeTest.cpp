#include "app/Application.hpp"

#include <array>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "EngineLifecycle.hpp"
#include "graphics/CanvasTheme.hpp"
#include "graphics/Palette.hpp"

using rubiks::graphics::CanvasTheme;
using rubiks::test::arrive;
using rubiks::test::drawn_frame;
using rubiks::test::EngineLifecycle;
using rubiks::test::settle;

namespace {

// Contract colors as RGBA bytes on little-endian memory, the same order
// RenderContractTest reads them in.
using Rgba = std::array<std::uint8_t, 4>;

constexpr Rgba kDarkGround{32, 32, 32, 255};
constexpr Rgba kLightGround{231, 235, 240, 255};

constexpr std::uint32_t kWidth = 320;
constexpr std::uint32_t kHeight = 240;

/** One pixel of the frame just drawn, by its column and row. */
Rgba pixel_at(const std::vector<std::uint8_t>& frame, std::uint32_t x,
              std::uint32_t y)
{
    const std::size_t offset =
        (static_cast<std::size_t>(y) * kWidth + x) * 4;
    REQUIRE(offset + 4 <= frame.size());
    return Rgba{frame[offset], frame[offset + 1], frame[offset + 2],
                frame[offset + 3]};
}

/**
 * The four corners, which no scene reaches.
 *
 * The cube sits in a square region and the net below it, so a corner is the
 * ground and nothing else at every aspect ratio this test uses. Checking all
 * four is what makes "fills the background" mean the whole of it rather than
 * one lucky pixel.
 */
void require_ground(const std::vector<std::uint8_t>& frame, const Rgba& color)
{
    for (const auto corner : {std::pair<std::uint32_t, std::uint32_t>{0, 0},
                              {kWidth - 1, 0},
                              {0, kHeight - 1},
                              {kWidth - 1, kHeight - 1}}) {
        const Rgba pixel = pixel_at(frame, corner.first, corner.second);
        REQUIRE(pixel[0] == color[0]);
        REQUIRE(pixel[1] == color[1]);
        REQUIRE(pixel[2] == color[2]);
        REQUIRE(pixel[3] == color[3]);
    }
}

}  // namespace

TEST_CASE("the ground starts dark and stays the old pixel contract")
{
    EngineLifecycle engine(kWidth, kHeight);

    REQUIRE(rubiks::app::canvas_theme() == CanvasTheme::Dark);
    require_ground(drawn_frame(), kDarkGround);
}

TEST_CASE("light fills the whole ground")
{
    EngineLifecycle engine(kWidth, kHeight);

    REQUIRE(rubiks::app::set_canvas_theme(CanvasTheme::Light));
    REQUIRE(rubiks::app::canvas_theme() == CanvasTheme::Light);
    require_ground(drawn_frame(), kLightGround);

    // And back, because a person switching a page twice is the ordinary case
    // and a one-way door would pass the test above.
    REQUIRE(rubiks::app::set_canvas_theme(CanvasTheme::Dark));
    require_ground(drawn_frame(), kDarkGround);
}

TEST_CASE("a value naming no theme is refused and changes nothing")
{
    EngineLifecycle engine(kWidth, kHeight);

    REQUIRE(rubiks::app::set_canvas_theme(CanvasTheme::Light));
    const std::vector<std::uint8_t> before = drawn_frame();

    REQUIRE_FALSE(rubiks::app::set_canvas_theme(static_cast<CanvasTheme>(7)));
    REQUIRE_FALSE(rubiks::app::set_canvas_theme(static_cast<CanvasTheme>(-1)));

    REQUIRE(rubiks::app::canvas_theme() == CanvasTheme::Light);
    REQUIRE(drawn_frame() == before);
}

TEST_CASE("outside a lifecycle the theme is dark and cannot be set")
{
    REQUIRE_FALSE(rubiks::app::set_canvas_theme(CanvasTheme::Light));
    REQUIRE(rubiks::app::canvas_theme() == CanvasTheme::Dark);
}

TEST_CASE("the theme leaves the cube, the record and the palette alone")
{
    EngineLifecycle engine(kWidth, kHeight);

    REQUIRE(rubiks::app::set_palette(rubiks::graphics::Palette::HighContrast));
    arrive(7, 12);

    const std::vector<std::uint8_t> scrambled = drawn_frame();
    const int cursor = rubiks::app::timeline_cursor();
    const int length = rubiks::app::timeline_length();
    const bool solved = rubiks::app::is_solved();

    REQUIRE(rubiks::app::set_canvas_theme(CanvasTheme::Light));

    REQUIRE(rubiks::app::timeline_cursor() == cursor);
    REQUIRE(rubiks::app::timeline_length() == length);
    REQUIRE(rubiks::app::is_solved() == solved);
    REQUIRE(rubiks::app::palette() == rubiks::graphics::Palette::HighContrast);
    REQUIRE(rubiks::app::view_mode() == rubiks::graphics::ViewMode::Both);
    REQUIRE_FALSE(rubiks::app::is_busy());

    // The cube itself is untouched: only the ground moved, so the frame
    // differs from the one above without the camera or the stickers moving.
    const std::vector<std::uint8_t> relit = drawn_frame();
    REQUIRE(relit != scrambled);
    require_ground(relit, kLightGround);

    // Put the ground back and the two frames are the same picture again,
    // which is the whole of "nothing but the background changed".
    REQUIRE(rubiks::app::set_canvas_theme(CanvasTheme::Dark));
    REQUIRE(drawn_frame() == scrambled);
}

TEST_CASE("the theme is accepted while a sequence is playing")
{
    EngineLifecycle engine(kWidth, kHeight);

    REQUIRE(rubiks::app::scramble(3, 20));
    REQUIRE(rubiks::app::is_busy());

    // Like the palette and unlike the view commands: a person switching to
    // Light in the middle of a scramble gets a light page, and the scramble
    // keeps running.
    REQUIRE(rubiks::app::set_canvas_theme(CanvasTheme::Light));
    REQUIRE(rubiks::app::is_busy());
    require_ground(drawn_frame(), kLightGround);

    settle();
    REQUIRE(rubiks::app::timeline_cursor() == 20);
    require_ground(drawn_frame(), kLightGround);
}
