#include "app/Application.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "EngineLifecycle.hpp"
#include "cube/Surface.hpp"
#include "graphics/Camera.hpp"
#include "interaction/DragResolver.hpp"
#include "math/Types.hpp"
#include "graphics/Layout.hpp"
#include "graphics/NetGeometry.hpp"
#include "graphics/RingsGeometry.hpp"
#include "graphics/Palette.hpp"
#include "interaction/InteractionController.hpp"

// Rendered scene contract v3: the canvas is split into a square 3D region
// showing the +X, +Y and +Z faces of a solved 3x3x3, and a net below it
// showing all six faces flat. Distinct face colors make a winding, culling or
// channel-order mistake show up as the wrong color rather than as a plausible
// picture, and the seam samples make a missing gap show up as a wrong color
// rather than as a slightly fat sticker.
//
// The layout fractions are recomputed here from the numbers written in
// docs/tasks/04-rubiks-cube-domain.md rather than read from the engine, so a
// change to the layout has to be made in both places deliberately.

namespace {

// Contract colors as RGBA bytes on little-endian memory.
using Rgba = std::array<std::uint8_t, 4>;

constexpr Rgba kBackground{32, 32, 32, 255};
constexpr Rgba kWhite{255, 255, 255, 255};   // +Y up
constexpr Rgba kYellow{255, 213, 0, 255};    // -Y down
constexpr Rgba kGreen{0, 155, 72, 255};      // +Z front
constexpr Rgba kBlue{0, 70, 173, 255};       // -Z back
constexpr Rgba kRed{183, 18, 52, 255};       // +X right
constexpr Rgba kOrange{255, 88, 0, 255};     // -X left

// Contract v4: the 3D view is lit. Each plane of the cube has one brightness,
// a byte out of 255, worked out by hand in docs/tasks/04-rubiks-cube-domain.md
// from the light's resting place; the net is a diagram and stays unlit.
constexpr unsigned kLitUp = 275;     // +Y, the plane nearest the key light
constexpr unsigned kLitFront = 230;  // +Z
constexpr unsigned kLitRight = 211;  // +X
// The cut faces a turn opens lie one layer in, so their planes are their own:
// the +Y cut at y = +1/3 of a U turn and the +X cut at x = +1/3 of an R turn.
// (Recorded for the contract; the mid-turn checks look for the body at any
// brightness a cut can have, at the place the cut is.)
[[maybe_unused]] constexpr unsigned kLitCutUp = 273;
[[maybe_unused]] constexpr unsigned kLitCutRight = 218;

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

struct Region {
    float x;
    float y;
    float side;  // the 3D region is square; for the net this is one face
};

/** Fraction of a region, measured from its top left. */
struct Sample {
    float x;
    float y;
};

// Centers of the three visible face centers, unchanged from contract v2. They
// are fractions of the 3D region rather than of the canvas, which is what
// makes them valid at any aspect ratio now that the region is always square.
constexpr Sample kUpSample{0.50f, 0.29f};
constexpr Sample kFrontSample{0.31f, 0.61f};
constexpr Sample kRightSample{0.69f, 0.61f};

// Gaps between neighbouring stickers. The cubie body shows there -- painted
// under the stickers as each slab's silhouette -- so they are exactly kBody.
constexpr Sample kSeamSamples[]{
    {0.377f, 0.645f},  // +Z, between the center and right stickers
    {0.313f, 0.534f},  // +Z, between the center and top stickers
    {0.564f, 0.321f},  // +Y, between the center and front-right stickers
};

/** One face of the unfolded cube: its place in the cross and its color. */
struct NetBlock {
    int column;
    int row;
    Rgba color;
};

// The solved net, written out by hand so it does not depend on the mapping
// the engine uses to fill it.
constexpr NetBlock kNetBlocks[]{
    {1, 0, kWhite},                                        // U
    {0, 1, kOrange}, {1, 1, kGreen}, {2, 1, kRed}, {3, 1, kBlue},  // L F R B
    {1, 2, kYellow},                                       // D
};

constexpr int kCubeSize = 3;

/**
 * The cubie body: the cut surfaces a turn opens, and what shows between the
 * stickers -- the slab silhouettes painted under them.
 */
constexpr Rgba kBody{70, 74, 82, 255};

/**
 * Half of the snap animation, which is half of the turn.
 *
 * The easing is symmetric about its middle, so the frame at half the duration
 * is the frame at 45 degrees exactly, with no tolerance needed.
 */
constexpr double kHalfTurnMs = 100.0;

float shorter_side(std::uint32_t width, std::uint32_t height)
{
    return static_cast<float>(std::min(width, height));
}

Region cube_region(std::uint32_t width, std::uint32_t height)
{
    const float unit = shorter_side(width, height);
    const float side = 0.58f * unit;
    return Region{(static_cast<float>(width) - side) * 0.5f, 0.01f * unit,
                  side};
}

Region net_region(std::uint32_t width, std::uint32_t height)
{
    const float unit = shorter_side(width, height);
    const float face_side = 0.12f * unit;
    return Region{(static_cast<float>(width) - face_side * 4.0f) * 0.5f,
                  0.62f * unit, face_side};
}

const std::uint8_t* pixel_at(std::uint32_t x, std::uint32_t y,
                             std::uint32_t width)
{
    const auto* bytes =
        reinterpret_cast<const std::uint8_t*>(rubiks::app::pixel_buffer());
    return bytes + (static_cast<std::size_t>(y) * width + x) * 4;
}

const std::uint8_t* pixel_at(float x, float y, std::uint32_t width,
                             std::uint32_t height)
{
    const auto column = static_cast<std::uint32_t>(
        std::clamp(std::lround(x), 0L, static_cast<long>(width) - 1));
    const auto row = static_cast<std::uint32_t>(
        std::clamp(std::lround(y), 0L, static_cast<long>(height) - 1));
    return pixel_at(column, row, width);
}

/** Pixel at a fraction of the square 3D region. */
const std::uint8_t* pixel_in_cube(const Sample& sample, std::uint32_t width,
                                  std::uint32_t height)
{
    const Region region = cube_region(width, height);
    return pixel_at(region.x + sample.x * region.side,
                    region.y + sample.y * region.side, width, height);
}

void require_pixel(const std::uint8_t* pixel, const Rgba& color)
{
    // Channel-by-channel exact match; a byte-order mistake cannot slip past.
    // Compared as integers so a failure prints numbers rather than bytes.
    REQUIRE(int{pixel[0]} == int{color[0]});
    REQUIRE(int{pixel[1]} == int{color[1]});
    REQUIRE(int{pixel[2]} == int{color[2]});
    REQUIRE(int{pixel[3]} == int{color[3]});
}

/**
 * Within two units per channel of a colour.
 *
 * The lit stickers are gradients following the plane's brightness, and the
 * middle of the middle sticker is the plane's reference byte. The sample
 * point sits up to a few dozen pixels off that middle, and the brightness
 * runs at about a byte per fifteen pixels across a face, so two units cover
 * it. A normal the wrong way round is tens of units off, so the check still
 * bites.
 */
void require_pixel_near(const std::uint8_t* pixel, const Rgba& color, int slack)
{
    INFO("pixel " << int{pixel[0]} << "," << int{pixel[1]} << "," << int{pixel[2]}
                  << " vs " << int{color[0]} << "," << int{color[1]} << ","
                  << int{color[2]});
    for (int i = 0; i < 3; ++i) {
        INFO("channel " << i);
        REQUIRE(std::abs(int{pixel[i]} - int{color[i]}) <= slack);
    }
    REQUIRE(int{pixel[3]} == int{color[3]});
}

void require_cube_faces(std::uint32_t width, std::uint32_t height)
{
    // Two units per channel, or four on a buffer so small that a face's
    // five-stop gradient is squeezed into a few dozen pixels and its
    // interpolation coarsens; a wrong face is still tens of units away.
    const int slack = std::min(width, height) < 256 ? 4 : 2;

    // Three faces at three brightnesses: a normal the wrong way round now
    // shows up as the wrong shade as well as the wrong colour.
    require_pixel_near(pixel_in_cube(kUpSample, width, height),
                       lit(saturated(kWhite), kLitUp), slack);
    require_pixel_near(pixel_in_cube(kFrontSample, width, height),
                       lit(saturated(kGreen), kLitFront), slack);
    require_pixel_near(pixel_in_cube(kRightSample, width, height),
                       lit(saturated(kRed), kLitRight), slack);
}

/** Reads the center of all 54 net cells and compares the whole grid. */
void require_net(std::uint32_t width, std::uint32_t height)
{
    const Region region = net_region(width, height);
    const float cell = region.side / static_cast<float>(kCubeSize);

    for (const auto& block : kNetBlocks) {
        const float origin_x =
            region.x + static_cast<float>(block.column) * region.side;
        const float origin_y =
            region.y + static_cast<float>(block.row) * region.side;

        for (int row = 0; row < kCubeSize; ++row) {
            for (int col = 0; col < kCubeSize; ++col) {
                const float x =
                    origin_x + (static_cast<float>(col) + 0.5f) * cell;
                const float y =
                    origin_y + (static_cast<float>(row) + 0.5f) * cell;
                require_pixel(pixel_at(x, y, width, height), block.color);
            }
        }
    }
}

/** Pixel at a point given in net cells from the top left of the net. */
const std::uint8_t* pixel_in_net(float column, float row, std::uint32_t width,
                                 std::uint32_t height)
{
    const Region region = net_region(width, height);
    const float cell = region.side / static_cast<float>(kCubeSize);
    return pixel_at(region.x + column * cell, region.y + row * cell, width,
                    height);
}

/** Whether a pixel is the body at some brightness a lit cut face can have. */
bool is_lit_body(const std::uint8_t* pixel)
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
 * The pixel a world-space point falls on, seen from the home viewpoint.
 *
 * For looking at a cut face where it is: the seams show the body too, so a
 * cut cannot be found by colour alone, but it can be found by place.
 */
const std::uint8_t* pixel_at_world(const rubiks::math::Vec3& world,
                                   std::uint32_t width, std::uint32_t height)
{
    const Region region = cube_region(width, height);
    const rubiks::graphics::Rect rect{region.x, region.y, region.side, region.side};
    const auto point = rubiks::interaction::project_to_screen(
        world, rubiks::graphics::default_camera(1.0f), rect);
    REQUIRE(point);
    return pixel_at(point->x, point->y, width, height);
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
constexpr rubiks::math::Vec3 kCutOfUTurn{0.70f, 1.0f / 3.0f, 0.85f};
/** The same for R: the still layers' +X cut at x = +1/3. */
constexpr rubiks::math::Vec3 kCutOfRTurn{1.0f / 3.0f, 0.70f, 0.85f};

void require_corners(std::uint32_t width, std::uint32_t height)
{
    require_pixel(pixel_at(0, 0, width), kBackground);
    require_pixel(pixel_at(width - 1, 0, width), kBackground);
    require_pixel(pixel_at(0, height - 1, width), kBackground);
    require_pixel(pixel_at(width - 1, height - 1, width), kBackground);
}

void require_scene(std::uint32_t width, std::uint32_t height)
{
    REQUIRE(rubiks::app::pixel_buffer() != 0);
    REQUIRE(rubiks::app::pixel_byte_length() == width * height * 4);
    REQUIRE(rubiks::app::render());

    require_cube_faces(width, height);
    require_net(width, height);
    require_corners(width, height);
}

/** Every byte of the 3D region, so an intrusion cannot hide in a blend. */
std::vector<std::uint8_t> cube_region_bytes(std::uint32_t width,
                                            std::uint32_t height)
{
    const Region region = cube_region(width, height);
    const auto left = static_cast<std::uint32_t>(region.x);
    const auto top = static_cast<std::uint32_t>(region.y);
    const auto side = static_cast<std::uint32_t>(region.side);

    std::vector<std::uint8_t> bytes;
    bytes.reserve(static_cast<std::size_t>(side) * side * 4);

    for (std::uint32_t row = 0; row < side && top + row < height; ++row) {
        for (std::uint32_t column = 0; column < side && left + column < width;
             ++column) {
            const auto* pixel = pixel_at(left + column, top + row, width);
            bytes.insert(bytes.end(), pixel, pixel + 4);
        }
    }
    return bytes;
}

/**
 * How many pixels of a box are drawn in a guide line's own color.
 *
 * A piece the turn is carrying rounds a cut and ends up outside the net, so
 * "anything at all is drawn here" no longer tells a guide line from a sticker.
 * The line's own color does: the stickers have none of the three, and the core
 * of a line this thick lands on it exactly whatever the edges blend with.
 */
int guide_pixels(float left, float top, float right, float bottom,
                 std::uint32_t width, std::uint32_t height)
{
    int total = 0;
    for (auto y = static_cast<std::uint32_t>(top);
         y < static_cast<std::uint32_t>(bottom) && y < height; ++y) {
        for (auto x = static_cast<std::uint32_t>(left);
             x < static_cast<std::uint32_t>(right) && x < width; ++x) {
            const auto* pixel = pixel_at(x, y, width);

            for (const auto axis : {rubiks::cube::Axis::X,
                                    rubiks::cube::Axis::Y,
                                    rubiks::cube::Axis::Z}) {
                // A guide line is not quite opaque, so what lands on an empty
                // margin is its color over the background, not the color.
                const auto guide = rubiks::graphics::guide_color(axis);
                const float cover = static_cast<float>(guide.a) / 255.0f;
                const std::array<float, 3> over{
                    guide.r * cover + kBackground[0] * (1.0f - cover),
                    guide.g * cover + kBackground[1] * (1.0f - cover),
                    guide.b * cover + kBackground[2] * (1.0f - cover)};

                constexpr float kSlack = 12.0f;
                if (std::abs(pixel[0] - over[0]) < kSlack &&
                    std::abs(pixel[1] - over[1]) < kSlack &&
                    std::abs(pixel[2] - over[2]) < kSlack) {
                    ++total;
                    break;
                }
            }
        }
    }
    return total;
}

/** The middle of a net cell, in buffer coordinates. */
Sample net_cell_point(float column, float row, std::uint32_t width,
                      std::uint32_t height)
{
    const Region region = net_region(width, height);
    const float cell = region.side / static_cast<float>(kCubeSize);
    return Sample{region.x + column * cell, region.y + row * cell};
}

}  // namespace

TEST_CASE("face, net and corner pixels match the rendered scene contract")
{
    const rubiks::test::EngineLifecycle engine(256, 256);
    require_scene(256, 256);

    SECTION("after resizing to a larger square buffer")
    {
        REQUIRE(rubiks::app::resize(512, 512));
        require_scene(512, 512);
    }

    SECTION("after resizing to a smaller square buffer")
    {
        REQUIRE(rubiks::app::resize(129, 129));
        require_scene(129, 129);
    }

}

TEST_CASE("the contract holds on non-square buffers")
{
    // The 3D region is square whatever the canvas is, so unlike contract v2
    // these fractions stay valid here.
    const rubiks::test::EngineLifecycle engine(320, 200);
    require_scene(320, 200);

    SECTION("and on a portrait buffer")
    {
        REQUIRE(rubiks::app::resize(200, 320));
        require_scene(200, 320);
    }

}

TEST_CASE("the seams between stickers show the cubie body")
{
    // A seam is about 5.5 pixels wide at this size. Below roughly 1024 the
    // gap is thin enough that anti-aliasing reaches the sample point, so the
    // smaller sizes above check faces and corners only.
    constexpr std::uint32_t kSize = 1024;

    const rubiks::test::EngineLifecycle engine(kSize, kSize);
    require_scene(kSize, kSize);

    // Contract v4: the body colour, unlit and exact, not the background --
    // and not any sticker colour either, which is what makes a seam decisive.
    for (const auto& seam : kSeamSamples) {
        require_pixel(pixel_in_cube(seam, kSize, kSize), kBody);
    }

}

// Rendered scene contract, extended to a frame in the middle of a turn. The
// same idea as above -- fixed sample points read out of a real rasterization
// -- applied where both views are moving, so that a band carried to the wrong
// place shows up as a wrong color rather than as a plausible picture.
//
// No golden image: one would have to be regenerated every time the ThorVG
// submodule moves, whereas these coordinates are rederived by hand.

TEST_CASE("a sliding turn half way through rasterizes in both views")
{
    constexpr std::uint32_t kSize = 1024;
    const rubiks::test::EngineLifecycle engine(kSize, kSize);

    REQUIRE(rubiks::app::turn_face(rubiks::cube::Face::Up, 1, 1, 1));
    REQUIRE(rubiks::app::advance(kHalfTurnMs));
    REQUIRE(rubiks::app::render());

    // U slides the four side faces' top row -- the fourth of the net's nine
    // rows -- half a face to the left, which is one and a half cells. So the
    // stickers of that row now sit on whole-cell boundaries, and the one four
    // cells in is the front face's rightmost, still green.
    require_pixel(pixel_in_net(4.0f, 3.5f, kSize, kSize), kGreen);

    // Five cells in is the one that followed it, off the right of the front
    // block and onto the right block: red.
    require_pixel(pixel_in_net(5.0f, 3.5f, kSize, kSize), kRed);

    // The row below is not in the layer and has not moved: nothing the turn
    // does moves the drawing it is passing over.
    require_pixel(pixel_in_net(4.5f, 4.5f, kSize, kSize), kGreen);
    require_pixel(pixel_in_net(4.5f, 5.5f, kSize, kSize), kGreen);

    // Up is on the turning axis, so its block turns in place: the middle of
    // it does not move, and neither does the color of a solved face.
    require_pixel(pixel_in_net(4.5f, 1.5f, kSize, kSize), kWhite);

    // Meanwhile the 3D view is turning the same layer, which opens the cut:
    // the still middle layer's top, lit as the +Y plane one layer down.
    REQUIRE(is_lit_body(pixel_at_world(kCutOfUTurn, kSize, kSize)));

    // Both views come back to rest together, and undoing the turn puts the
    // whole resting contract back exactly as it was.
    while (rubiks::app::advance(16.0)) {
    }
    REQUIRE(rubiks::app::render());
    REQUIRE_FALSE(is_lit_body(pixel_at_world(kCutOfUTurn, kSize, kSize)));

    REQUIRE(rubiks::app::turn_face(rubiks::cube::Face::Up, 1, 1, -1));
    while (rubiks::app::advance(16.0)) {
    }
    require_scene(kSize, kSize);
}

TEST_CASE("a turn across a cut band half way through rasterizes in both views")
{
    constexpr std::uint32_t kSize = 1024;
    const rubiks::test::EngineLifecycle engine(kSize, kSize);

    REQUIRE(rubiks::app::turn_face(rubiks::cube::Face::Right, 1, 1, 1));
    REQUIRE(rubiks::app::advance(kHalfTurnMs));
    REQUIRE(rubiks::app::render());

    // R's band runs down the column Up, Front and Down share in the cross, so
    // it marches up it: half way round, every cell of that column has moved a
    // face and a half, and the front's middle cell is now in the up face's.
    require_pixel(pixel_in_net(5.5f, 3.0f, kSize, kSize), kGreen);

    // With the down face's middle cell right behind it, in the front's.
    require_pixel(pixel_in_net(5.5f, 6.0f, kSize, kSize), kYellow);

    // The column beside it is in no part of the turn, so it is where it was.
    require_pixel(pixel_in_net(4.5f, 1.5f, kSize, kSize), kWhite);
    require_pixel(pixel_in_net(3.5f, 1.5f, kSize, kSize), kWhite);

    // And the right face is the one turning, so it spins in place: a solved
    // face stays one color through it, and its middle never moves.
    require_pixel(pixel_in_net(7.5f, 4.5f, kSize, kSize), kRed);

    // The cut this turn opens faces +X, one layer in from the right face.
    REQUIRE(is_lit_body(pixel_at_world(kCutOfRTurn, kSize, kSize)));

    // And the turn arrives exactly where the commit puts the cube, so the
    // frame after it is the same picture.
    while (rubiks::app::advance(16.0)) {
    }
    REQUIRE(rubiks::app::render());
    require_pixel(pixel_in_net(5.5f, 0.5f, kSize, kSize), kGreen);
    require_pixel(pixel_in_net(5.5f, 2.5f, kSize, kSize), kGreen);
}

TEST_CASE("pressing a net cell draws the rings it could turn on")
{
    constexpr std::uint32_t kSize = 1024;
    const rubiks::test::EngineLifecycle engine(kSize, kSize);

    const Region net = net_region(kSize, kSize);
    const float cell = net.side / static_cast<float>(kCubeSize);
    const float net_bottom =
        net.y + static_cast<float>(rubiks::graphics::kNetRows) * net.side;

    // Two margins that tell the two rings apart. The vertical ring bridges Up's
    // column across to Back's below the net; the horizontal one goes round past
    // the left-hand edge. Counted by the line's own color, because a drag also
    // steps the cells beside the turn out into these margins.
    const auto below = [&] {
        return guide_pixels(0.0f, net_bottom, static_cast<float>(kSize),
                            static_cast<float>(kSize), kSize, kSize);
    };
    const auto beside = [&] {
        return guide_pixels(0.0f, net.y, net.x, net_bottom, kSize, kSize);
    };

    REQUIRE(rubiks::app::render());
    REQUIRE(below() == 0);
    REQUIRE(beside() == 0);

    const auto resting_cube = cube_region_bytes(kSize, kSize);

    // The middle of the front block turns on two rings, neither of them the
    // one the front block itself spins on.
    const Sample press = net_cell_point(4.5f, 4.5f, kSize, kSize);
    REQUIRE(rubiks::app::pointer_down(press.x, press.y));
    REQUIRE(rubiks::app::render());

    REQUIRE(below() > 0);
    REQUIRE(beside() > 0);

    // And they keep off the 3D view entirely: not one byte of it moved. A
    // whole-buffer comparison rather than a search for the line's own color,
    // which would miss a pixel the edge of the line only tinted.
    REQUIRE(cube_region_bytes(kSize, kSize) == resting_cube);

    // Dragging up settles on the vertical one, and the horizontal one goes.
    rubiks::app::pointer_move(press.x, press.y - 3.0f * cell);
    REQUIRE(rubiks::app::render());
    REQUIRE(below() > 0);
    REQUIRE(beside() == 0);

    // Releasing ends the gesture, so the guide goes with it.
    rubiks::app::pointer_up();
    while (rubiks::app::advance(16.0)) {
    }
    REQUIRE(rubiks::app::render());
    REQUIRE(below() == 0);
    REQUIRE(beside() == 0);

    // A drag the other way settles on the horizontal ring instead.
    REQUIRE(rubiks::app::pointer_down(press.x, press.y));
    rubiks::app::pointer_move(press.x + 3.0f * cell, press.y);
    REQUIRE(rubiks::app::render());
    REQUIRE(beside() > 0);
    REQUIRE(below() == 0);
}

TEST_CASE("the ring diagram renders every sticker and the loops beneath them")
{
    constexpr std::uint32_t kSize = 1024;

    const rubiks::test::EngineLifecycle engine(kSize, kSize);
    REQUIRE(rubiks::app::set_view_mode(rubiks::graphics::ViewMode::Flat));
    REQUIRE(rubiks::app::set_flat_style(rubiks::graphics::FlatStyle::Rings));

    const auto rect = rubiks::graphics::layout(
                          kSize, kSize, rubiks::graphics::ViewMode::Flat,
                          rubiks::graphics::FlatStyle::Rings)
                          .rings;

    const auto sticker_at = [&](const rubiks::cube::SurfaceSticker& sticker) {
        const auto at = rubiks::graphics::rings_slot_position(sticker, rect, 3);
        REQUIRE(at.has_value());
        return *at;
    };

    REQUIRE(rubiks::app::render());

    // Every face's middle sticker shows that face's own color: nine circles
    // and fifty-four crossings, and the one this sticker is drawn at is the
    // one the layout says it is.
    const std::array<std::pair<rubiks::cube::Face, Rgba>, 6> middles{{
        {rubiks::cube::Face::Right, kRed},
        {rubiks::cube::Face::Left, kOrange},
        {rubiks::cube::Face::Up, kWhite},
        {rubiks::cube::Face::Down, kYellow},
        {rubiks::cube::Face::Front, kGreen},
        {rubiks::cube::Face::Back, kBlue},
    }};

    for (const auto& [face, color] : middles) {
        rubiks::cube::SurfaceSticker sticker{1, 1, 1, face};
        const int outer = rubiks::cube::outer_layer(face, 3);
        switch (rubiks::cube::axis_of(face)) {
            case rubiks::cube::Axis::X:
                sticker.x = outer;
                break;
            case rubiks::cube::Axis::Y:
                sticker.y = outer;
                break;
            case rubiks::cube::Axis::Z:
                sticker.z = outer;
                break;
        }

        const auto at = sticker_at(sticker);
        INFO("face " << static_cast<int>(face));
        require_pixel(pixel_at(at.x, at.y, kSize, kSize), color);
    }

    // Those samples are also what says the loops are drawn under the stickers
    // rather than over them. Every sticker sits where two circles cross, so
    // both of them run through the very point sampled above; drawn afterwards
    // they would paint their own color over each one.

    // And the loops are drawn: a fair share of the canvas carries one ring's
    // color, which no sticker and no background can. The rings are slightly
    // transparent, so what to look for is that color composited over the
    // background rather than the color itself.
    const auto ring = rubiks::graphics::guide_color(rubiks::cube::Axis::X);
    const auto over_background = [&](std::uint8_t channel,
                                     std::uint8_t under) {
        return static_cast<int>(
            (static_cast<int>(channel) * ring.a +
             static_cast<int>(under) * (255 - ring.a)) / 255);
    };
    const std::array<int, 3> expected{over_background(ring.r, kBackground[0]),
                                      over_background(ring.g, kBackground[1]),
                                      over_background(ring.b, kBackground[2])};

    std::size_t drawn = 0;
    for (std::uint32_t y = 0; y < kSize; ++y) {
        for (std::uint32_t x = 0; x < kSize; ++x) {
            const auto* pixel = pixel_at(x, y, kSize);
            const bool matched =
                std::abs(static_cast<int>(pixel[0]) - expected[0]) <= 2 &&
                std::abs(static_cast<int>(pixel[1]) - expected[1]) <= 2 &&
                std::abs(static_cast<int>(pixel[2]) - expected[2]) <= 2;
            if (matched) ++drawn;
        }
    }
    REQUIRE(drawn > 1000);
}
