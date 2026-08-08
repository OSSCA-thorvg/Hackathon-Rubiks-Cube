#pragma once

#include "graphics/Camera.hpp"

namespace rubiks::graphics {

/**
 * How far above and below the horizon the viewpoint may travel.
 *
 * Short of the poles, where the world up vector would line up with the view
 * direction and look_at would have nothing left to orient the picture by. Far
 * enough to look comfortably at the top and bottom faces.
 */
inline constexpr float kPitchLimitDegrees = 80.0f;

/**
 * A viewpoint on the sphere around the cube, as a turntable.
 *
 * Two degrees of freedom on purpose. The third would be roll, which only tilts
 * the picture and would accumulate out of a circular drag; leaving it out is
 * what keeps up pointing up. A drag supplies two numbers anyway, and unlimited
 * yaw with a clamped pitch still reaches all six faces, so nothing is lost.
 *
 * That choice is also why no rotation is represented here. Composing a drag is
 * scalar addition, building a camera is a spherical-to-cartesian conversion
 * rather than a rotation, and clamping the pitch needs the pitch to be a
 * variable rather than something decomposed back out of a matrix.
 */
struct OrbitCamera {
    /** Angle about the world up axis; zero looks straight at the +Z face. */
    float yaw_degrees = 0.0f;
    /** Angle above the horizon; positive looks down at the cube. */
    float pitch_degrees = 0.0f;

    /**
     * Adds a drag's angles, wrapping the yaw and clamping the pitch.
     *
     * Clamping the stored pitch rather than the drag is what lets a viewpoint
     * resting against the limit move away on the very next opposite drag.
     */
    void turn(float yaw_delta, float pitch_delta) noexcept;

    /** The camera for this viewpoint; only the eye differs from the home. */
    [[nodiscard]] Camera to_camera(float aspect) const noexcept;
};

/**
 * The viewpoint the rendered scene contract was derived from.
 *
 * Derived from kHomeEye rather than written out again, so the home angles and
 * the orbit radius cannot drift apart.
 */
[[nodiscard]] OrbitCamera home_orbit() noexcept;

}  // namespace rubiks::graphics
