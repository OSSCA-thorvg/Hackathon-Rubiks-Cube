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

/** One frame of elapsed time, at the rate a display would deliver them. */
inline constexpr double kFrameMs = 16.0;

/**
 * Runs frames until the application stops asking for them.
 *
 * Here rather than in each test file because every test that watches
 * something arrive needs it, and the two copies that grew up apart had the
 * same body down to the runaway guard. That guard is why this fails the case
 * rather than hanging it: an application that never settles is a bug in the
 * engine, and a test suite that hangs says so far less clearly.
 *
 * @return how many frames it took, for the tests that assert motion happened.
 */
inline int settle()
{
    int frames = 0;
    while (rubiks::app::advance(kFrameMs)) {
        ++frames;
        REQUIRE(frames < 1000);
    }
    return frames;
}

}  // namespace rubiks::test
