#pragma once

#include <cstdint>
#include <cstdlib>

#include <catch2/catch_test_macros.hpp>

#include "EngineLifecycle.hpp"
#include "math/Types.hpp"

/**
 * The rendered scene contract, as the bytes of a frame the application drew.
 *
 * What RenderContractTest pins and what the other application tests read a
 * frame against: the colours, the brightness of each lit plane and the
 * arithmetic that puts one on the other, where the three visible faces are
 * sampled, and the checks of a single pixel those tests share.
 *
 * Every value is written here by hand, from the numbers in
 * docs/tasks/04-rubiks-cube-domain.md, and none is read from the engine, so a
 * change to a colour or to the lighting has to be made in both places
 * deliberately.
 *
 * Beside the application's tests rather than in tests/support: require_pixel()
 * asserts, and these are the bytes of a frame, which only the application
 * draws.
 */
namespace rubiks::test::contract {

// Contract colors as RGBA bytes on little-endian memory.
inline constexpr Rgba kDarkGround{32, 32, 32, 255};      // the dark theme
inline constexpr Rgba kLightGround{231, 235, 240, 255};  // the light theme
inline constexpr Rgba kWhite{255, 255, 255, 255};   // +Y up, as the net draws it
// The same white as the 3D view lights it: graphics::kPaperWhite.
inline constexpr Rgba kPaperWhite{216, 216, 216, 255};
inline constexpr Rgba kYellow{255, 213, 0, 255};    // -Y down
inline constexpr Rgba kGreen{0, 155, 72, 255};      // +Z front
inline constexpr Rgba kBlue{0, 70, 173, 255};       // -Z back
inline constexpr Rgba kRed{183, 18, 52, 255};       // +X right
inline constexpr Rgba kOrange{255, 88, 0, 255};     // -X left

// Contract v4: the 3D view is lit. Each plane of the cube has one brightness,
// a byte out of 255, worked out by hand in docs/tasks/04-rubiks-cube-domain.md
// from the light's resting place; the net is a diagram and stays unlit.
// The planes turned away from the light are at the ambient floor: the kicker
// lights nothing, it only glints.
inline constexpr unsigned kLitUp = 275;     // +Y, the plane nearest the key light
inline constexpr unsigned kLitFront = 230;  // +Z
inline constexpr unsigned kLitRight = 211;  // +X
inline constexpr unsigned kLitAway = 191;   // -X and -Z: the ambient floor
// The cut faces a turn opens lie one layer in, so their planes are their own:
// the +Y cut at y = +1/3 of a U turn and the +X cut at x = +1/3 of an R turn.
// (Recorded for the contract; the mid-turn checks look for the body at any
// brightness a cut can have, at the place the cut is.)
inline constexpr unsigned kLitCutUp = 273;
inline constexpr unsigned kLitCutRight = 218;

/**
 * A colour at a brightness, in the integer arithmetic the engine uses.
 *
 * The brightness may pass 255 -- the palette colour is a well-lit face, not
 * the brightest -- and each channel stops at 255.
 */
constexpr std::uint8_t lit_channel(unsigned value, unsigned brightness)
{
    const unsigned scaled = (value * brightness + 127) / 255;
    return static_cast<std::uint8_t>(scaled > 255 ? 255 : scaled);
}

constexpr Rgba lit(const Rgba& color, unsigned brightness)
{
    return Rgba{lit_channel(color[0], brightness), lit_channel(color[1], brightness),
                lit_channel(color[2], brightness), color[3]};
}

/**
 * A sticker's colour with its chroma raised before it is lit (contract v4,
 * fifth revision): gray = (2126 r + 7152 g + 722 b + 5000) / 10000, and each
 * channel gray + ((c - gray) * 125 + 50) / 100, rounded away from grey.
 */
constexpr std::uint8_t saturated_channel(int value, int gray)
{
    const int offset = (value - gray) * 125;
    const int rounded = offset >= 0 ? (offset + 50) / 100 : -((-offset + 50) / 100);
    const int result = gray + rounded;
    return static_cast<std::uint8_t>(result < 0 ? 0 : result > 255 ? 255 : result);
}

constexpr Rgba saturated(const Rgba& color)
{
    const int gray = (2126 * color[0] + 7152 * color[1] + 722 * color[2] + 5000) / 10000;
    return Rgba{saturated_channel(color[0], gray), saturated_channel(color[1], gray),
                saturated_channel(color[2], gray), color[3]};
}

/**
 * The cubie body: the cut surfaces a turn opens, and what shows between the
 * stickers -- the slab silhouettes painted under them.
 */
inline constexpr Rgba kBody{70, 74, 82, 255};

/** Fraction of a region, measured from its top left. */
struct Sample {
    float x;
    float y;
};

// Centers of the three visible face centers, unchanged from contract v2. They
// are fractions of the 3D region rather than of the canvas, which is what
// makes them valid at any aspect ratio now that the region is always square.
inline constexpr Sample kUpSample{0.50f, 0.29f};
inline constexpr Sample kFrontSample{0.31f, 0.61f};
inline constexpr Sample kRightSample{0.69f, 0.61f};

/** Requires a pixel to be exactly a colour. */
inline void require_pixel(const std::uint8_t* pixel, const Rgba& color)
{
    // Channel-by-channel exact match; a byte-order mistake cannot slip past.
    // Compared as integers so a failure prints numbers rather than bytes.
    REQUIRE(int{pixel[0]} == int{color[0]});
    REQUIRE(int{pixel[1]} == int{color[1]});
    REQUIRE(int{pixel[2]} == int{color[2]});
    REQUIRE(int{pixel[3]} == int{color[3]});
}

/** Whether a pixel is the body at some brightness a lit cut face can have. */
[[nodiscard]] inline bool is_lit_body(const std::uint8_t* pixel)
{
    // The body by its hue rather than by a table of shades: kBody is
    // (70, 74, 82), and every brightness of it, and every white glint laid
    // over it, keeps the channels in that order with the blue-green step
    // twice the green-red step. A glint on a cut face can be as strong as
    // the lamp's whole specular, so its amount is not bounded here; what is
    // is the spread, which shrinks towards white and is zero for a white
    // sticker, and the darkness, which rules out the ground.
    if (pixel[3] != kBody[3]) return false;
    const int r = pixel[0], g = pixel[1], b = pixel[2];
    if (r > g || g > b) return false;
    const int spread = b - r;
    if (spread < 2 || spread > 16) return false;
    if (std::abs((b - g) - 2 * (g - r)) > 3) return false;
    return r >= 40 && r <= 220;
}

/**
 * A point on the cut a turn opens, clear of the turned layer at half way.
 *
 * U turns the top layer; the still middle layer's top, y = +1/3, shows where
 * the turned layer (a square turned 45 degrees, a diamond) does not cover
 * it: the corners, |x| + |z| > sqrt 2. At rest the line of sight to this
 * point lands well inside a front-face sticker, green -- not a body at all.
 * Worked out by ray-casting from the home eye, with a tenth of a unit to
 * spare from every seam.
 */
inline constexpr math::Vec3 kCutOfUTurn{0.70f, 1.0f / 3.0f, 0.85f};
/** The same for R: the still layers' +X cut at x = +1/3. */
inline constexpr math::Vec3 kCutOfRTurn{1.0f / 3.0f, 0.70f, 0.85f};

}  // namespace rubiks::test::contract
