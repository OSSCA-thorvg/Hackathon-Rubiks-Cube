#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "EngineLifecycle.hpp"
#include "graphics/Light.hpp"
#include "graphics/RenderScene.hpp"
#include "render/ThorVGSoftwareRenderer.hpp"

// The renderer's side of Phase 19: how a shadow group and a highlight are
// composed out of ThorVG's scenes, effects, masks and blends. Hand-built
// RenderScenes rendered into a real SwCanvas and read back pixel by pixel, so
// the composition is asserted rather than eyeballed.

namespace {

using rubiks::graphics::Color;
using rubiks::graphics::RenderFace;
using rubiks::graphics::RenderHighlight;
using rubiks::graphics::RenderScene;
using rubiks::graphics::RenderShadow;
using rubiks::graphics::Rect;
using rubiks::math::Vec2;
using rubiks::render::ThorVGSoftwareRenderer;

constexpr std::uint32_t kSide = 256;
constexpr Color kDark{32, 32, 32, 255};
constexpr Color kLight{231, 235, 240, 255};

using Rgba = std::array<std::uint8_t, 4>;

struct Frame {
    std::unique_ptr<ThorVGSoftwareRenderer> renderer;

    Rgba at(std::uint32_t x, std::uint32_t y) const
    {
        const auto* bytes =
            reinterpret_cast<const std::uint8_t*>(renderer->pixel_buffer());
        const std::size_t offset = (static_cast<std::size_t>(y) * kSide + x) * 4;
        return Rgba{bytes[offset], bytes[offset + 1], bytes[offset + 2],
                    bytes[offset + 3]};
    }

    int luminance(std::uint32_t x, std::uint32_t y) const
    {
        const Rgba pixel = at(x, y);
        return int{pixel[0]} + int{pixel[1]} + int{pixel[2]};
    }
};

/** Renders a scene over a ground colour. The engine has to be up for ThorVG. */
Frame draw(const RenderScene& scene, Color ground = kDark)
{
    Frame frame{ThorVGSoftwareRenderer::create(kSide, kSide)};
    REQUIRE(frame.renderer);
    frame.renderer->set_background(ground);
    REQUIRE(frame.renderer->render(scene));
    return frame;
}

std::vector<Vec2> rectangle(float left, float top, float right, float bottom)
{
    return {Vec2{left, top}, Vec2{right, top}, Vec2{right, bottom},
            Vec2{left, bottom}};
}

RenderFace quad(float left, float top, float right, float bottom, Color color)
{
    RenderFace face;
    face.points = {Vec2{left, top}, Vec2{right, top}, Vec2{right, bottom},
                   Vec2{left, bottom}};
    face.color = color;
    return face;
}

/** A shadow across the middle of the canvas, faded left to right. */
RenderShadow band()
{
    RenderShadow shadow;
    shadow.polygons.push_back(rectangle(20.0f, 100.0f, 236.0f, 156.0f));
    shadow.fade_start = Vec2{40.0f, 128.0f};
    shadow.fade_end = Vec2{220.0f, 128.0f};
    shadow.opacity = rubiks::graphics::kShadowOpacity;
    shadow.blur_sigma = 3.0f;
    shadow.clip = Rect{0.0f, 0.0f, kSide, kSide};
    return shadow;
}

bool same_rgb(const Rgba& a, const Rgba& b)
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

}  // namespace

TEST_CASE("the shadow darkens the ground on both themes and nowhere else")
{
    const rubiks::test::EngineLifecycle engine(16, 16);

    RenderScene scene;
    scene.shadow = band();

    for (const auto& ground : {kDark, kLight}) {
        const Frame frame = draw(scene, ground);
        const Rgba plain{ground.r, ground.g, ground.b, 255};

        // Inside, near the dark anchor: darker than the ground.
        REQUIRE(frame.luminance(48, 128) < int{plain[0]} * 3);
        // Well outside the band and its blur: the ground exactly.
        REQUIRE(same_rgb(frame.at(128, 40), plain));
        REQUIRE(same_rgb(frame.at(128, 220), plain));
    }
}

TEST_CASE("two casters overlapping are no darker than one")
{
    const rubiks::test::EngineLifecycle engine(16, 16);

    RenderScene one;
    one.shadow = band();

    RenderScene two = one;
    // A second polygon covering the same ground, as two turning slabs would.
    two.shadow->polygons.push_back(rectangle(20.0f, 100.0f, 236.0f, 156.0f));
    two.shadow->polygons.push_back(rectangle(60.0f, 110.0f, 200.0f, 150.0f));

    const Frame single = draw(one);
    const Frame doubled = draw(two);
    for (std::uint32_t x = 24; x < 232; x += 8) {
        for (std::uint32_t y = 104; y < 152; y += 8) {
            INFO("pixel " << x << ", " << y);
            REQUIRE(same_rgb(single.at(x, y), doubled.at(x, y)));
        }
    }
}

TEST_CASE("the shadow fades from its anchor towards its far end")
{
    const rubiks::test::EngineLifecycle engine(16, 16);

    RenderScene scene;
    scene.shadow = band();
    const Frame frame = draw(scene);

    // Along the middle row, from the anchor outwards, it only gets lighter --
    // within the one unit per channel that gradient and blur rounding move.
    int previous = frame.luminance(40, 128);
    for (std::uint32_t x = 48; x <= 220; x += 4) {
        const int current = frame.luminance(x, 128);
        INFO("column " << x);
        REQUIRE(current + 3 >= previous);
        previous = std::max(previous, current);
    }
    // And it is still a shadow at the far end: the floor, not nothing.
    REQUIRE(frame.luminance(216, 128) < 32 * 3);
    REQUIRE(frame.luminance(216, 128) > frame.luminance(40, 128));
}

TEST_CASE("a light overhead fades the shadow evenly outwards")
{
    const rubiks::test::EngineLifecycle engine(16, 16);

    RenderScene scene;
    scene.shadow = band();
    scene.shadow->polygons = {rectangle(48.0f, 48.0f, 208.0f, 208.0f)};
    scene.shadow->fade_start = Vec2{128.0f, 128.0f};
    scene.shadow->fade_end = Vec2{129.0f, 128.0f};  // no direction to speak of
    const Frame frame = draw(scene);

    // Darkest in the middle, lighter towards every side alike.
    const int middle = frame.luminance(128, 128);
    REQUIRE(frame.luminance(64, 128) > middle);
    REQUIRE(frame.luminance(192, 128) > middle);
    REQUIRE(frame.luminance(128, 64) > middle);
    REQUIRE(frame.luminance(128, 192) > middle);
    REQUIRE(frame.luminance(64, 128) == frame.luminance(192, 128));
}

TEST_CASE("the blur softens the shadow's edge")
{
    const rubiks::test::EngineLifecycle engine(16, 16);

    RenderScene scene;
    scene.shadow = band();
    scene.shadow->fade_end = scene.shadow->fade_start;  // even, to isolate the edge
    scene.shadow->polygons = {rectangle(48.0f, 48.0f, 208.0f, 208.0f)};
    scene.shadow->fade_start = Vec2{128.0f, 128.0f};
    scene.shadow->fade_end = Vec2{128.0f, 128.0f};
    const Frame frame = draw(scene);

    // Two pixels inside the outline are lighter than the deep inside, and two
    // pixels outside it are darker than the ground: the edge has been spread.
    const int deep = frame.luminance(128, 128);
    REQUIRE(frame.luminance(50, 128) > deep);
    REQUIRE(frame.luminance(46, 128) < 32 * 3);
    REQUIRE(frame.luminance(46, 128) > frame.luminance(50, 128));
}

TEST_CASE("the clip keeps the shadow inside its region")
{
    const rubiks::test::EngineLifecycle engine(16, 16);

    RenderScene scene;
    scene.shadow = band();
    scene.shadow->clip = Rect{0.0f, 0.0f, 128.0f, kSide};
    const Frame frame = draw(scene);

    // The band runs to x = 236, but the clip ends at 128: everything to the
    // right of it, blur included, is the ground exactly.
    for (std::uint32_t x = 129; x < kSide; x += 3) {
        INFO("column " << x);
        REQUIRE(same_rgb(frame.at(x, 128), Rgba{32, 32, 32, 255}));
    }
    REQUIRE(frame.luminance(100, 128) < 32 * 3);
}

TEST_CASE("the cube's silhouette keeps the shadow out of its own seams")
{
    const rubiks::test::EngineLifecycle engine(16, 16);

    RenderScene scene;
    scene.shadow = band();
    scene.shadow->occluders.push_back(rectangle(100.0f, 90.0f, 156.0f, 166.0f));
    // Two stickers inside the silhouette with a seam between them.
    scene.faces.push_back(quad(104.0f, 96.0f, 126.0f, 160.0f, Color{200, 0, 0, 255}));
    scene.faces.push_back(quad(130.0f, 96.0f, 152.0f, 160.0f, Color{0, 200, 0, 255}));
    const Frame frame = draw(scene);

    // The seam shows the ground and only the ground, though the shadow
    // polygon runs straight under it.
    REQUIRE(same_rgb(frame.at(128, 128), Rgba{32, 32, 32, 255}));
    // Either side of the silhouette the shadow is there.
    REQUIRE(frame.luminance(80, 128) < 32 * 3);
    REQUIRE(frame.luminance(176, 128) < 32 * 3);
    // And the stickers are their own colour, untouched by anything below.
    REQUIRE(same_rgb(frame.at(115, 128), Rgba{200, 0, 0, 255}));
}

TEST_CASE("a highlight brightens its own face and never a face in front")
{
    const rubiks::test::EngineLifecycle engine(16, 16);

    RenderScene scene;
    // A face at the back with a glint spread across all of it.
    RenderFace behind = quad(40.0f, 40.0f, 216.0f, 216.0f, Color{40, 90, 160, 255});
    behind.highlight = RenderHighlight{Vec2{128.0f, 128.0f}, Vec2{228.0f, 128.0f}, 255};
    scene.faces.push_back(behind);
    // A face drawn after it, covering its right half.
    scene.faces.push_back(quad(128.0f, 40.0f, 216.0f, 216.0f, Color{60, 60, 60, 255}));
    const Frame frame = draw(scene);

    // The glint brightens the back face towards white, most at its centre...
    REQUIRE(frame.luminance(120, 128) > 40 + 90 + 160);
    REQUIRE(frame.luminance(120, 128) > frame.luminance(60, 128));
    // ...and the face in front is its own flat colour, glint or no glint.
    for (std::uint32_t x = 132; x < 216; x += 12) {
        INFO("column " << x);
        REQUIRE(same_rgb(frame.at(x, 128), Rgba{60, 60, 60, 255}));
    }

    SECTION("a screened glint keeps the hue it lands on")
    {
        // Screen towards white never darkens a channel and never turns a blue
        // sticker into anything but a lighter blue.
        const Rgba lit = frame.at(120, 128);
        REQUIRE(lit[0] >= 40);
        REQUIRE(lit[1] >= 90);
        REQUIRE(lit[2] >= 160);
        REQUIRE(lit[2] > lit[0]);
    }
}

TEST_CASE("a shadow with no strength draws nothing")
{
    const rubiks::test::EngineLifecycle engine(16, 16);

    RenderScene scene;
    scene.shadow = band();
    scene.shadow->opacity = 0;
    const Frame frame = draw(scene);
    REQUIRE(same_rgb(frame.at(48, 128), Rgba{32, 32, 32, 255}));
}
