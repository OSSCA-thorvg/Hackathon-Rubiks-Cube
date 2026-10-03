#pragma once

#include "CubeSupport.hpp"

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

}  // namespace rubiks::test
