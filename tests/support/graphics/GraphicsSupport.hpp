#pragma once

#include <cstdint>

#include "CubeSupport.hpp"
#include "graphics/3d/Camera.hpp"
#include "graphics/Layout.hpp"
#include "graphics/3d/OrbitCamera.hpp"
#include "graphics/Rect.hpp"

/**
 * What the tests share from graphics upwards.
 *
 * May include graphics/, math/ and the cube helpers below it, and nothing
 * above: tests/graphics, tests/interaction and tests/app are given this
 * directory, and tests/cube and tests/solver are not. What only the
 * application's tests need stays in tests/app/EngineLifecycle.hpp.
 *
 * No Catch2 in here either, for the same reason as below.
 */
namespace rubiks::test {

/** One frame of elapsed time, at the rate a display would deliver them. */
inline constexpr double kFrameMs = 16.0;

/**
 * The side of the square canvas most pointer tests work on.
 *
 * Large enough that a sticker of a small cube is many pixels across, so a
 * point a test aims at is nowhere near the edge of what it is aiming for.
 */
inline constexpr std::uint32_t kCanvas = 1024;

/** The region the 3D cube is drawn in, on a canvas showing both views. */
[[nodiscard]] inline graphics::Rect cube_rect(
    std::uint32_t width = kCanvas, std::uint32_t height = kCanvas) noexcept
{
    return graphics::layout(width, height).cube;
}

/**
 * The home viewpoint looking into that region.
 *
 * The same call the application makes for a cube nobody has turned the view
 * of, so a test presses through the camera a resting cube is drawn with.
 */
[[nodiscard]] inline graphics::Camera cube_camera(
    std::uint32_t width = kCanvas, std::uint32_t height = kCanvas) noexcept
{
    return graphics::camera_into(graphics::home_orbit(),
                                 cube_rect(width, height));
}

}  // namespace rubiks::test
