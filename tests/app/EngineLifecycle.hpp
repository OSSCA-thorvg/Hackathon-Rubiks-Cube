#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "GraphicsSupport.hpp"
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

/** One pixel as the four bytes it is in memory: red, green, blue, alpha. */
using Rgba = std::array<std::uint8_t, 4>;

/**
 * Whether initialize() has succeeded and shutdown() has not followed it.
 *
 * Asked through the cube: there is a cube exactly while there is an engine,
 * and no cube has a size of zero.
 */
[[nodiscard]] inline bool is_initialized()
{
    return rubiks::app::cube_size() != 0;
}

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
 * Runs exactly `count` frames, whatever the application says of each.
 *
 * For a test that wants to be part of the way through something, or to give
 * something that should not happen the time it would need to happen in.
 */
inline void run_frames(int count)
{
    for (int frame = 0; frame < count; ++frame) {
        static_cast<void>(rubiks::app::advance(kFrameMs));
    }
}

/**
 * Runs frames for as long as the application is busy, and requires each of
 * them to ask for the next.
 *
 * settle() stops when a frame says no more are needed; this one asks the
 * question a caller outside the frame loop asks, and so pins that the two
 * answers agree while a sequence plays.
 *
 * @return how many frames it took, under the same guard as settle().
 */
inline int run_while_busy()
{
    int frames = 0;
    while (rubiks::app::is_busy()) {
        REQUIRE(rubiks::app::advance(kFrameMs));
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

    const auto* pixels = reinterpret_cast<const std::uint8_t*>(
        rubiks::app::surface_pixel_buffer(0));
    const std::uint32_t length = rubiks::app::surface_pixel_byte_length(0);
    REQUIRE(pixels != nullptr);
    REQUIRE(length > 0);

    return std::vector<std::uint8_t>(pixels, pixels + length);
}

/**
 * The four bytes a surface's last frame holds at a point, given in pixels.
 *
 * The fraction of each coordinate is dropped, so a point anywhere inside a
 * pixel reads that pixel. `width` is the width the test gave the surface: the
 * buffer is rows of that many pixels.
 */
[[nodiscard]] inline const std::uint8_t* pixel_at(float x, float y,
                                                  std::uint32_t width,
                                                  std::uint32_t surface = 0)
{
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(
        rubiks::app::surface_pixel_buffer(surface));
    REQUIRE(bytes != nullptr);

    return bytes +
           (static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) *
               4;
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
