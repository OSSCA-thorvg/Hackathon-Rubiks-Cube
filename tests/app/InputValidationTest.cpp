#include "app/Application.hpp"
#include "render/ThorVGSoftwareRenderer.hpp"

#include <cstdint>
#include <limits>

#include <catch2/catch_test_macros.hpp>

#include "EngineLifecycle.hpp"

namespace {

constexpr std::uint32_t kOversized =
    rubiks::render::ThorVGSoftwareRenderer::kMaxDimension + 1;
constexpr std::uint32_t kUint32Max =
    std::numeric_limits<std::uint32_t>::max();

}  // namespace

TEST_CASE("invalid dimensions are rejected before initialization")
{
    REQUIRE(!rubiks::app::initialize(0, 64));
    REQUIRE(!rubiks::app::initialize(64, 0));
    REQUIRE(!rubiks::app::initialize(kOversized, 64));
    REQUIRE(!rubiks::app::initialize(64, kOversized));
    // Dimensions that would overflow width * height * 4 are also rejected.
    REQUIRE(!rubiks::app::initialize(kUint32Max, kUint32Max));
    REQUIRE(!rubiks::test::is_initialized());
}

TEST_CASE("repeated initialize validates dimensions before the no-op path")
{
    REQUIRE(rubiks::app::initialize(64, 64));
    const auto pointer = rubiks::app::surface_pixel_buffer(0);
    const auto length = rubiks::app::surface_pixel_byte_length(0);

    REQUIRE(!rubiks::app::initialize(0, 0));
    REQUIRE(!rubiks::app::initialize(kOversized, 64));
    REQUIRE(rubiks::test::is_initialized());

    // A repeated initialize with valid dimensions is a no-op success and
    // never reconfigures the active buffer.
    REQUIRE(rubiks::app::initialize(32, 32));
    REQUIRE(rubiks::app::surface_pixel_buffer(0) == pointer);
    REQUIRE(rubiks::app::surface_pixel_byte_length(0) == length);
    REQUIRE(rubiks::app::render());

    rubiks::app::shutdown();
}

TEST_CASE("failed resizes preserve the previous buffer and target")
{
    REQUIRE(rubiks::app::initialize(64, 64));
    const auto pointer = rubiks::app::surface_pixel_buffer(0);
    const auto length = rubiks::app::surface_pixel_byte_length(0);
    REQUIRE(pointer != 0);
    REQUIRE(length == 64u * 64u * 4u);

    REQUIRE(!rubiks::app::resize_surface(0, 0, 64));
    REQUIRE(!rubiks::app::resize_surface(0, 64, 0));
    REQUIRE(!rubiks::app::resize_surface(0, kOversized, 64));
    REQUIRE(!rubiks::app::resize_surface(0, kUint32Max, kUint32Max));
    REQUIRE(rubiks::app::surface_pixel_buffer(0) == pointer);
    REQUIRE(rubiks::app::surface_pixel_byte_length(0) == length);
    REQUIRE(rubiks::app::render());

    // Resizing to the current size succeeds without replacing the buffer.
    REQUIRE(rubiks::app::resize_surface(0, 64, 64));
    REQUIRE(rubiks::app::surface_pixel_buffer(0) == pointer);

    rubiks::app::shutdown();
}
