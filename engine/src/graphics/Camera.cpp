#include "graphics/Camera.hpp"

namespace rubiks::graphics {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kVerticalFov = kPi / 4.0f;  // 45 degrees
constexpr float kNearPlane = 1.0f;
constexpr float kFarPlane = 10.0f;

}  // namespace

Camera default_camera(float aspect) noexcept
{
    return Camera{math::Vec3{3.0f, 3.0f, 3.0f}, math::Vec3{0.0f, 0.0f, 0.0f},
                  math::Vec3{0.0f, 1.0f, 0.0f}, kVerticalFov,   aspect,
                  kNearPlane,                   kFarPlane};
}

}  // namespace rubiks::graphics
