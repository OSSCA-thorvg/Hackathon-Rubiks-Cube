#pragma once

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "app/Application.hpp"

namespace rubiks::test {

/**
 * Owns one application lifecycle for a test case.
 *
 * The destructor shuts the engine down even when an assertion fails the
 * case, so the next test never starts on top of a leaked initialization.
 * Lifecycle tests that call initialize() and shutdown() deliberately keep
 * doing so directly.
 */
struct EngineLifecycle {
    EngineLifecycle(std::uint32_t width, std::uint32_t height)
    {
        REQUIRE(rubiks::app::initialize(width, height));
    }

    ~EngineLifecycle() { rubiks::app::shutdown(); }

    EngineLifecycle(const EngineLifecycle&) = delete;
    EngineLifecycle& operator=(const EngineLifecycle&) = delete;
};

}  // namespace rubiks::test
