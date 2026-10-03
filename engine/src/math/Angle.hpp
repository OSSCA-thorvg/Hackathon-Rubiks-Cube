#pragma once

/**
 * Pi and the conversion between degrees and radians.
 *
 * The cube's turns and the viewpoint are kept in degrees, which is what a
 * quarter turn and a drag are naturally measured in; the trigonometry wants
 * radians. This is the one place the two meet.
 */
namespace rubiks::math {

inline constexpr float kPi = 3.14159265358979323846f;

/**
 * An angle in degrees, in radians.
 *
 * The order of the arithmetic is part of the contract: times pi, then
 * divided by 180. Float arithmetic does not associate, so multiplying by a
 * ready-made pi / 180 can differ in the last bit, and the drawings are
 * pinned to the pixels this order gives.
 */
[[nodiscard]] constexpr float to_radians(float degrees) noexcept
{
    return degrees * kPi / 180.0f;
}

/**
 * An angle in radians, in degrees: times 180, then divided by pi, for the
 * reason to_radians() gives.
 */
[[nodiscard]] constexpr float to_degrees(float radians) noexcept
{
    return radians * 180.0f / kPi;
}

}  // namespace rubiks::math
