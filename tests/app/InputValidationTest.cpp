#include "app/Application.hpp"
#include "render/ThorVGSoftwareRenderer.hpp"

#include <cstdint>
#include <limits>

#include <catch2/catch_test_macros.hpp>

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
    REQUIRE(!rubiks::app::is_initialized());
}

TEST_CASE("repeated initialize validates dimensions before the no-op path")
{
    REQUIRE(rubiks::app::initialize(64, 64));
    const auto pointer = rubiks::app::pixel_buffer();
    const auto length = rubiks::app::pixel_byte_length();

    REQUIRE(!rubiks::app::initialize(0, 0));
    REQUIRE(!rubiks::app::initialize(kOversized, 64));
    REQUIRE(rubiks::app::is_initialized());

    // A repeated initialize with valid dimensions is a no-op success and
    // never reconfigures the active buffer.
    REQUIRE(rubiks::app::initialize(32, 32));
    REQUIRE(rubiks::app::pixel_buffer() == pointer);
    REQUIRE(rubiks::app::pixel_byte_length() == length);
    REQUIRE(rubiks::app::render());

    rubiks::app::shutdown();
}

TEST_CASE("failed resizes preserve the previous buffer and target")
{
    REQUIRE(rubiks::app::initialize(64, 64));
    const auto pointer = rubiks::app::pixel_buffer();
    const auto length = rubiks::app::pixel_byte_length();
    REQUIRE(pointer != 0);
    REQUIRE(length == 64u * 64u * 4u);

    REQUIRE(!rubiks::app::resize(0, 64));
    REQUIRE(!rubiks::app::resize(64, 0));
    REQUIRE(!rubiks::app::resize(kOversized, 64));
    REQUIRE(!rubiks::app::resize(kUint32Max, kUint32Max));
    REQUIRE(rubiks::app::pixel_buffer() == pointer);
    REQUIRE(rubiks::app::pixel_byte_length() == length);
    REQUIRE(rubiks::app::render());

    // Resizing to the current size succeeds without replacing the buffer.
    REQUIRE(rubiks::app::resize(64, 64));
    REQUIRE(rubiks::app::pixel_buffer() == pointer);

    rubiks::app::shutdown();
}
