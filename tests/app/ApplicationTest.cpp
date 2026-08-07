#include "app/Application.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("calls before initialization fail")
{
    REQUIRE(!rubiks::app::is_initialized());
    REQUIRE(!rubiks::app::render());
    REQUIRE(!rubiks::app::resize(64, 64));
    REQUIRE(rubiks::app::pixel_buffer() == 0);
    REQUIRE(rubiks::app::pixel_byte_length() == 0);
}

TEST_CASE("initialize and shutdown are idempotent")
{
    REQUIRE(rubiks::app::initialize(64, 64));
    REQUIRE(rubiks::app::initialize(64, 64));
    REQUIRE(rubiks::app::is_initialized());
    REQUIRE(rubiks::app::pixel_buffer() != 0);
    REQUIRE(rubiks::app::pixel_byte_length() == 64u * 64u * 4u);
    REQUIRE(rubiks::app::render());

    rubiks::app::shutdown();
    rubiks::app::shutdown();
    REQUIRE(!rubiks::app::is_initialized());
    REQUIRE(!rubiks::app::render());
    REQUIRE(rubiks::app::pixel_buffer() == 0);
    REQUIRE(rubiks::app::pixel_byte_length() == 0);
}

TEST_CASE("the application can be reinitialized after shutdown")
{
    REQUIRE(rubiks::app::initialize(32, 32));
    REQUIRE(rubiks::app::render());
    rubiks::app::shutdown();
    REQUIRE(!rubiks::app::is_initialized());
}
