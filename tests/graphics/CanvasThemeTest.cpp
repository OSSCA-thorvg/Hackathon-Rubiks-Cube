#include "graphics/CanvasTheme.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>

using rubiks::graphics::CanvasTheme;
using rubiks::graphics::canvas_background;
using rubiks::graphics::Color;

namespace {

/** How far from pure white the light ground has to stay, per channel. */
constexpr int kWhiteSeparation = 12;

/** Distance from a channel to full, as a plain integer. */
int below_full(std::uint8_t channel)
{
    return 255 - static_cast<int>(channel);
}

}  // namespace

TEST_CASE("dark keeps the ground the renderer has always cleared to")
{
    // Every rendered-scene test in the suite was written against this value,
    // and the native build has no page in front of it to ask for another.
    REQUIRE(canvas_background(CanvasTheme::Dark) == Color{32, 32, 32, 255});
}

TEST_CASE("light is a neutral the white stickers still stand out of")
{
    const Color light = canvas_background(CanvasTheme::Light);

    // A white page would take the white face with it and leave a cube with a
    // hole in one side, so every channel stays clear of full.
    REQUIRE(below_full(light.r) >= kWhiteSeparation);
    REQUIRE(below_full(light.g) >= kWhiteSeparation);
    REQUIRE(below_full(light.b) >= kWhiteSeparation);

    // Light rather than merely off-white: the black seams are what a cube is
    // read by, and they need a ground well above them.
    REQUIRE(static_cast<int>(light.r) > 200);
    REQUIRE(static_cast<int>(light.g) > 200);
    REQUIRE(static_cast<int>(light.b) > 200);
}

TEST_CASE("both grounds are opaque")
{
    // The background is the one shape nothing is drawn behind. A translucent
    // one would leave whatever the buffer held last frame showing through.
    REQUIRE(canvas_background(CanvasTheme::Light).a == 255);
    REQUIRE(canvas_background(CanvasTheme::Dark).a == 255);
}

TEST_CASE("a value naming no theme falls back to dark")
{
    // The application boundary refuses these before they arrive, so this
    // fixes the second line of defence rather than the first: whatever is
    // asked for, the answer is a ground, never an uninitialized color.
    const auto unknown = static_cast<CanvasTheme>(7);
    REQUIRE(canvas_background(unknown) == canvas_background(CanvasTheme::Dark));
}
