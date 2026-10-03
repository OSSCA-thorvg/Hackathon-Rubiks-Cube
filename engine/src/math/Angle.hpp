#pragma once

#include <cmath>

/**
 * Pi, the conversion between degrees and radians, and the way a value that
 * goes round is brought back into one period.
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

/**
 * The equivalent of a value in [0, period), for a positive period: an angle
 * in degrees with a period of 360, a place on a loop of slots with the
 * number of slots.
 *
 * A value a hair below zero wraps to exactly the period once the sum is
 * rounded to a float -- 12 - 1e-7 is 12 -- which is outside the range this
 * promises and, for a caller that reads a table by the result, one past its
 * end. It is zero. So is a value that is not finite, which has no place in
 * the period to be brought back to.
 */
[[nodiscard]] inline float wrapped(float value, float period) noexcept
{
    const float remainder = std::fmod(value, period);
    if (remainder >= 0.0f) return remainder;
    const float lifted = remainder + period;
    return lifted < period ? lifted : 0.0f;
}

/**
 * The equivalent of a value in (-period / 2, period / 2]. Given how far one
 * place on a cycle is from another, this is the shorter way round between
 * them, with its sign.
 *
 * A value that is not finite comes back as NaN.
 */
[[nodiscard]] inline float wrapped_centred(float value, float period) noexcept
{
    const float half = 0.5f * period;
    float remainder = std::fmod(value, period);
    if (remainder <= -half) remainder += period;
    if (remainder > half) remainder -= period;
    return remainder;
}

}  // namespace rubiks::math
