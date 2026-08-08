#include "app/Application.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

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

// Gaps between neighbouring stickers. Nothing is drawn there, so they must be
// exactly the background color.
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
    REQUIRE(pixel[0] == color[0]);
    REQUIRE(pixel[1] == color[1]);
    REQUIRE(pixel[2] == color[2]);
    REQUIRE(pixel[3] == color[3]);
}

void require_cube_faces(std::uint32_t width, std::uint32_t height)
{
    require_pixel(pixel_in_cube(kUpSample, width, height), kWhite);
    require_pixel(pixel_in_cube(kFrontSample, width, height), kGreen);
    require_pixel(pixel_in_cube(kRightSample, width, height), kRed);
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

}  // namespace

TEST_CASE("face, net and corner pixels match the rendered scene contract")
{
    REQUIRE(rubiks::app::initialize(256, 256));
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

    rubiks::app::shutdown();
}

TEST_CASE("the contract holds on non-square buffers")
{
    // The 3D region is square whatever the canvas is, so unlike contract v2
    // these fractions stay valid here.
    REQUIRE(rubiks::app::initialize(320, 200));
    require_scene(320, 200);

    SECTION("and on a portrait buffer")
    {
        REQUIRE(rubiks::app::resize(200, 320));
        require_scene(200, 320);
    }

    rubiks::app::shutdown();
}

TEST_CASE("the seams between stickers show the background")
{
    // A seam is about 5.5 pixels wide at this size. Below roughly 1024 the
    // gap is thin enough that anti-aliasing reaches the sample point, so the
    // smaller sizes above check faces and corners only.
    constexpr std::uint32_t kSize = 1024;

    REQUIRE(rubiks::app::initialize(kSize, kSize));
    require_scene(kSize, kSize);

    for (const auto& seam : kSeamSamples) {
        require_pixel(pixel_in_cube(seam, kSize, kSize), kBackground);
    }

    rubiks::app::shutdown();
}
