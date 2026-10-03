#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "EngineLifecycle.hpp"
#include "RendererFrame.hpp"
#include "graphics/Light.hpp"
#include "graphics/RenderScene.hpp"

// The renderer's side of Phase 19.5: a storey of lifted pieces as one scene
// with ThorVG's drop shadow under it. Hand-built RenderScenes rendered into a
// real SwCanvas and read back pixel by pixel.

namespace {

using rubiks::graphics::Color;
using rubiks::graphics::LiftShadow;
using rubiks::graphics::RenderGroup;
using rubiks::graphics::RenderScene;
using rubiks::test::draw;
using rubiks::test::Frame;
using rubiks::test::is_ground;
using rubiks::test::kDark;
using rubiks::test::kLight;
using rubiks::test::quad;
using rubiks::test::Rgba;
using rubiks::test::same_rgb;

constexpr std::uint32_t kSide = 128;
constexpr Color kGreen{0, 155, 72, 255};
constexpr Color kOrange{255, 88, 0, 255};

/** One square held off the page, its shadow `distance` down and to the right. */
RenderGroup lifted(float left, float top, float side, Color color,
                   float distance)
{
    RenderGroup group;
    group.faces.push_back(quad(left, top, left + side, top + side, color));
    group.shadow = LiftShadow{distance, 100, 0.5f * distance};
    return group;
}

/**
 * The last darkened pixel along row `line` and along column `line`, reading
 * outwards from the middle of the canvas: how far the shadow reaches.
 */
std::pair<std::uint32_t, std::uint32_t> shade_reach(const Frame& frame,
                                                    std::uint32_t line,
                                                    Color ground)
{
    const int lit = int{ground.r} + int{ground.g} + int{ground.b};
    std::uint32_t right = 0;
    std::uint32_t down = 0;
    for (std::uint32_t i = 0; i < kSide; ++i) {
        if (frame.luminance(i, line) < lit) right = i;
        if (frame.luminance(line, i) < lit) down = i;
    }
    return {right, down};
}

}  // namespace

TEST_CASE("a lifted piece shades the ground down and to the right of it")
{
    const rubiks::test::EngineLifecycle engine(16, 16);

    RenderScene scene;
    scene.groups.push_back(lifted(40.0f, 40.0f, 40.0f, kGreen, 7.0f));

    for (const auto& ground : {kDark, kLight}) {
        const Frame frame = draw(scene, kSide, ground);

        // The piece itself, in its own colour.
        REQUIRE(same_rgb(frame.at(60, 60), Rgba{kGreen.r, kGreen.g, kGreen.b, 255}));

        // Just past the bottom right edge is under the shadow (a diagonal
        // distance of 7 is a push of 4 along each axis): darker than the
        // ground, and on the shadow's side of neutral -- bluer than it is red,
        // since the tint is the ground shadow's cool colour.
        const int lit = int{ground.r} + int{ground.g} + int{ground.b};
        REQUIRE(frame.luminance(82, 82) < lit);
        const Rgba shaded = frame.at(82, 82);
        REQUIRE(int{shaded[2]} - int{ground.b} >= int{shaded[0]} - int{ground.r});

        // The same distance past the top left edge is untouched ground.
        REQUIRE(is_ground(frame.at(37, 37), ground));
        REQUIRE(is_ground(frame.at(60, 37), ground));
        REQUIRE(is_ground(frame.at(37, 60), ground));

        // And well clear of it, nothing anywhere.
        REQUIRE(is_ground(frame.at(110, 110), ground));
    }
}

TEST_CASE("a higher storey's shadow falls further")
{
    const rubiks::test::EngineLifecycle engine(16, 16);

    RenderScene low;
    low.groups.push_back(lifted(30.0f, 30.0f, 30.0f, kGreen, 4.0f));
    RenderScene high;
    high.groups.push_back(lifted(30.0f, 30.0f, 30.0f, kGreen, 8.0f));

    const Frame near = draw(low, kSide, kLight);
    const Frame far = draw(high, kSide, kLight);
    // Both shadows reach past the piece's edge at 60, to the right along a
    // row through the piece's middle and down along a column through it, and
    // the further lift reaches further on both -- by the extra push along
    // each axis, which for twice the diagonal distance is 8 / sqrt 2 less
    // 4 / sqrt 2, cut to whole pixels: three.
    const auto near_reach = shade_reach(near, 45, kLight);
    const auto far_reach = shade_reach(far, 45, kLight);
    REQUIRE(near_reach.first >= 61);
    REQUIRE(near_reach.second >= 61);
    REQUIRE(far_reach.first >= near_reach.first + 3);
    REQUIRE(far_reach.second >= near_reach.second + 3);
}

TEST_CASE("an upper storey shades the storey below it and not itself")
{
    const rubiks::test::EngineLifecycle engine(16, 16);

    // The lower group is a wide plate; the upper is a smaller square over its
    // middle, lifted further, with a crisp edge so that what is read is the
    // order and not the blur.
    RenderScene scene;
    scene.groups.push_back(lifted(20.0f, 20.0f, 88.0f, kGreen, 3.0f));
    RenderGroup upper;
    upper.faces.push_back(quad(50.0f, 50.0f, 78.0f, 78.0f, kOrange));
    upper.shadow = LiftShadow{8.0f, 100, 1.0f};
    scene.groups.push_back(upper);

    const Frame frame = draw(scene, kSide, kLight);

    // The upper piece is whole: its own shadow does not darken it.
    REQUIRE(same_rgb(frame.at(64, 64), Rgba{kOrange.r, kOrange.g, kOrange.b, 255}));

    // Past its bottom right corner, on the lower piece, the green is darker
    // than the green well away from it.
    const int clear = frame.luminance(30, 30);
    REQUIRE(frame.luminance(82, 82) < clear - 20);
    // The same distance past its top left corner is plain green.
    REQUIRE(frame.luminance(46, 46) == clear);
}

TEST_CASE("a scene with no groups is the frame it always was")
{
    const rubiks::test::EngineLifecycle engine(16, 16);

    // Pieces at rest are faces, not empty groups: the two ways of putting the
    // same squares on the page are the same pixels, so a turn landing changes
    // nothing but where the pieces are drawn from.
    RenderScene flat;
    flat.faces.push_back(quad(20.0f, 20.0f, 60.0f, 60.0f, kGreen));
    flat.faces.push_back(quad(60.0f, 20.0f, 100.0f, 60.0f, kOrange));

    RenderScene grouped;
    grouped.faces.push_back(quad(20.0f, 20.0f, 60.0f, 60.0f, kGreen));
    RenderGroup group;
    group.faces.push_back(quad(60.0f, 20.0f, 100.0f, 60.0f, kOrange));
    group.shadow = LiftShadow{0.0f, 0, 0.0f};
    grouped.groups.push_back(group);

    const Frame a = draw(flat, kSide, kLight);
    const Frame b = draw(grouped, kSide, kLight);
    REQUIRE(a.same_as(b));
}

TEST_CASE("the lift shadow is the ground shadow's colour")
{
    const rubiks::test::EngineLifecycle engine(16, 16);

    // Fully opaque and unblurred, the pixel under the shadow is the tint
    // itself: the ground multiplied by the shadow colour, the same arithmetic
    // the cube's own shadow uses.
    RenderScene scene;
    RenderGroup group;
    group.faces.push_back(quad(40.0f, 40.0f, 80.0f, 80.0f, kGreen));
    group.shadow = LiftShadow{12.0f, 255, 0.0f};
    scene.groups.push_back(group);

    for (const auto& ground : {kDark, kLight}) {
        const Frame frame = draw(scene, kSide, ground);
        const Rgba expected{
            static_cast<std::uint8_t>(ground.r * rubiks::graphics::kShadowColor.r / 255),
            static_cast<std::uint8_t>(ground.g * rubiks::graphics::kShadowColor.g / 255),
            static_cast<std::uint8_t>(ground.b * rubiks::graphics::kShadowColor.b / 255),
            255};
        const Rgba shaded = frame.at(85, 85);
        for (int channel = 0; channel < 3; ++channel) {
            REQUIRE(std::abs(int{shaded[channel]} - int{expected[channel]}) <= 1);
        }
    }
}
