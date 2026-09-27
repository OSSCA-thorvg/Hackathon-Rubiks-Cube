#include "graphics/OrbitCamera.hpp"

#include <algorithm>
#include <cmath>

namespace rubiks::graphics {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegreesPerRadian = 180.0f / kPi;
constexpr float kRadiansPerDegree = kPi / 180.0f;

/** The radius of the sphere every viewpoint sits on. */
[[nodiscard]] float orbit_radius() noexcept
{
    return math::length(kHomeEye);
}

/** An equivalent angle in [0, 360). */
[[nodiscard]] float wrapped_degrees(float degrees) noexcept
{
    const float remainder = std::fmod(degrees, 360.0f);
    if (remainder >= 0.0f) return remainder;
    // A hair below zero wraps to exactly 360 once rounded to a float, which
    // is outside the range this promises. It is zero.
    const float wrapped = remainder + 360.0f;
    return wrapped < 360.0f ? wrapped : 0.0f;
}

}  // namespace

void OrbitCamera::turn(float yaw_delta, float pitch_delta) noexcept
{
    yaw_degrees = wrapped_degrees(yaw_degrees + yaw_delta);
    pitch_degrees = std::clamp(pitch_degrees + pitch_delta,
                               -kPitchLimitDegrees, kPitchLimitDegrees);
}

Camera camera_into(const OrbitCamera& orbit, const Rect& region) noexcept
{
    const float aspect =
        region.height > 0.0f ? region.width / region.height : 1.0f;
    return orbit.to_camera(aspect);
}

Camera OrbitCamera::to_camera(float aspect) const noexcept
{
    const float yaw = yaw_degrees * kRadiansPerDegree;
    const float pitch = pitch_degrees * kRadiansPerDegree;

    const float radius = orbit_radius();
    const float ground = radius * std::cos(pitch);

    const math::Vec3 eye{ground * std::sin(yaw), radius * std::sin(pitch),
                         ground * std::cos(yaw)};

    return Camera{eye,    kCubeCenter, kWorldUp, kVerticalFov,
                  aspect, kNearPlane,  kFarPlane};
}

OrbitCamera home_orbit() noexcept
{
    return OrbitCamera{
        wrapped_degrees(std::atan2(kHomeEye.x, kHomeEye.z) * kDegreesPerRadian),
        std::asin(kHomeEye.y / orbit_radius()) * kDegreesPerRadian};
}

}  // namespace rubiks::graphics
