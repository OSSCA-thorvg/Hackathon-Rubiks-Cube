#pragma once

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "app/Application.hpp"
#include "cube/Cubie.hpp"

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
 * The guard is well past the longest sequence the application can play, which
 * is a solve: a hundred and something moves, each of them several frames of
 * animation.
 *
 * @return how many frames it took, for the tests that assert motion happened.
 */
inline int settle()
{
    int frames = 0;
    while (rubiks::app::advance(kFrameMs)) {
        ++frames;
        REQUIRE(frames < 4000);
    }
    return frames;
}

/**
 * A copy of the frame just drawn, so two of them can be compared.
 *
 * Here for the same reason settle() is: three files had grown the same body,
 * and it is the only way a test tells one cube from another without keeping a
 * second copy of the domain to check against.
 */
[[nodiscard]] inline std::vector<std::uint8_t> drawn_frame()
{
    REQUIRE(rubiks::app::render());

    const auto* pixels =
        reinterpret_cast<const std::uint8_t*>(rubiks::app::pixel_buffer());
    const std::uint32_t length = rubiks::app::pixel_byte_length();
    REQUIRE(pixels != nullptr);
    REQUIRE(length > 0);

    return std::vector<std::uint8_t>(pixels, pixels + length);
}

/** Scrambles and waits for the whole sequence to arrive on the cube. */
inline void arrive(std::uint32_t seed, std::uint32_t moves)
{
    REQUIRE(rubiks::app::scramble(seed, moves));
    settle();
    REQUIRE(rubiks::app::timeline_cursor() == moves);
}

/** Turns one face and waits for it to settle, the way a user's move does. */
inline void turn(cube::Face face, int face_turns)
{
    REQUIRE(rubiks::app::turn_face(face, 1, 1, face_turns));
    settle();
}

}  // namespace rubiks::test
