#include "app/Application.hpp"

#include <cstddef>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

namespace {

// Rendered scene contract colors as RGBA bytes on little-endian memory.
constexpr std::uint8_t kBackgroundColor[4] = {32, 32, 32, 255};
constexpr std::uint8_t kRectangleColor[4] = {230, 57, 70, 255};

const std::uint8_t* pixel_at(std::uint32_t x, std::uint32_t y,
                             std::uint32_t width)
{
    const auto* bytes =
        reinterpret_cast<const std::uint8_t*>(rubiks::app::pixel_buffer());
    return bytes + (static_cast<std::size_t>(y) * width + x) * 4;
}

void require_pixel(const std::uint8_t* pixel, const std::uint8_t (&color)[4])
{
    REQUIRE(pixel[0] == color[0]);
    REQUIRE(pixel[1] == color[1]);
    REQUIRE(pixel[2] == color[2]);
    REQUIRE(pixel[3] == color[3]);
}

void require_scene(std::uint32_t width, std::uint32_t height)
{
    REQUIRE(rubiks::app::pixel_buffer() != 0);
    REQUIRE(rubiks::app::pixel_byte_length() == width * height * 4);
    REQUIRE(rubiks::app::render());

    require_pixel(pixel_at(width / 2, height / 2, width), kRectangleColor);
    require_pixel(pixel_at(0, 0, width), kBackgroundColor);
    require_pixel(pixel_at(width - 1, 0, width), kBackgroundColor);
    require_pixel(pixel_at(0, height - 1, width), kBackgroundColor);
    require_pixel(pixel_at(width - 1, height - 1, width), kBackgroundColor);
}

}  // namespace

TEST_CASE("center and corner pixels match the rendered scene contract")
{
    REQUIRE(rubiks::app::initialize(64, 64));
    require_scene(64, 64);

    SECTION("after resizing to a larger non-square buffer")
    {
        REQUIRE(rubiks::app::resize(128, 96));
        require_scene(128, 96);
    }

    SECTION("after resizing to odd dimensions")
    {
        REQUIRE(rubiks::app::resize(63, 47));
        require_scene(63, 47);
    }

    rubiks::app::shutdown();
}
