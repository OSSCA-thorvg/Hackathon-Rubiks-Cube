#include "app/Application.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

// Rendered scene contract v2: the fixed camera shows the +X, +Y, and +Z faces
// of one cube, each a different color, over a solid background. Distinct face
// colors make a winding, culling, or channel-order mistake show up as the
// wrong color rather than as a plausible picture.

namespace {

// Contract colors as RGBA bytes on little-endian memory.
constexpr std::uint8_t kBackgroundColor[4] = {32, 32, 32, 255};
constexpr std::uint8_t kUpColor[4] = {255, 255, 255, 255};    // +Y white
constexpr std::uint8_t kFrontColor[4] = {0, 155, 72, 255};    // +Z green
constexpr std::uint8_t kRightColor[4] = {183, 18, 52, 255};   // +X red

/** Sample point as a fraction of the drawing buffer. */
struct Sample {
    float x;
    float y;
};

// Derived from the projected centroid of each visible face. Only defined for
// a square buffer, because the fractions depend on the aspect ratio.
constexpr Sample kUpSample{0.50f, 0.29f};
constexpr Sample kFrontSample{0.31f, 0.61f};
constexpr Sample kRightSample{0.69f, 0.61f};

const std::uint8_t* pixel_at(std::uint32_t x, std::uint32_t y,
                             std::uint32_t width)
{
    const auto* bytes =
        reinterpret_cast<const std::uint8_t*>(rubiks::app::pixel_buffer());
    return bytes + (static_cast<std::size_t>(y) * width + x) * 4;
}

const std::uint8_t* pixel_at(const Sample& sample, std::uint32_t size)
{
    const auto extent = static_cast<float>(size - 1);
    const auto x = static_cast<std::uint32_t>(std::lround(sample.x * extent));
    const auto y = static_cast<std::uint32_t>(std::lround(sample.y * extent));
    return pixel_at(x, y, size);
}

void require_pixel(const std::uint8_t* pixel, const std::uint8_t (&color)[4])
{
    // Channel-by-channel exact match; a byte-order mistake cannot slip past.
    REQUIRE(pixel[0] == color[0]);
    REQUIRE(pixel[1] == color[1]);
    REQUIRE(pixel[2] == color[2]);
    REQUIRE(pixel[3] == color[3]);
}

void require_scene(std::uint32_t size)
{
    REQUIRE(rubiks::app::pixel_buffer() != 0);
    REQUIRE(rubiks::app::pixel_byte_length() == size * size * 4);
    REQUIRE(rubiks::app::render());

    require_pixel(pixel_at(kUpSample, size), kUpColor);
    require_pixel(pixel_at(kFrontSample, size), kFrontColor);
    require_pixel(pixel_at(kRightSample, size), kRightColor);

    // The silhouette never reaches the corners at this camera distance.
    require_pixel(pixel_at(0, 0, size), kBackgroundColor);
    require_pixel(pixel_at(size - 1, 0, size), kBackgroundColor);
    require_pixel(pixel_at(0, size - 1, size), kBackgroundColor);
    require_pixel(pixel_at(size - 1, size - 1, size), kBackgroundColor);
}

}  // namespace

TEST_CASE("face and corner pixels match the rendered scene contract")
{
    REQUIRE(rubiks::app::initialize(256, 256));
    require_scene(256);

    SECTION("after resizing to a larger square buffer")
    {
        REQUIRE(rubiks::app::resize(512, 512));
        require_scene(512);
    }

    SECTION("after resizing to a smaller square buffer")
    {
        REQUIRE(rubiks::app::resize(129, 129));
        require_scene(129);
    }

    rubiks::app::shutdown();
}

TEST_CASE("a non-square buffer keeps the scene inside the image")
{
    // The sample fractions are aspect-dependent, so a non-square target only
    // has to render without failing and keep its corners clear.
    REQUIRE(rubiks::app::initialize(320, 200));
    REQUIRE(rubiks::app::render());

    require_pixel(pixel_at(0, 0, 320), kBackgroundColor);
    require_pixel(pixel_at(319, 0, 320), kBackgroundColor);
    require_pixel(pixel_at(0, 199, 320), kBackgroundColor);
    require_pixel(pixel_at(319, 199, 320), kBackgroundColor);

    rubiks::app::shutdown();
}
