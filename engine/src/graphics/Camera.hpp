#pragma once

#include "math/Projection.hpp"
#include "math/Types.hpp"

namespace rubiks::graphics {

/**
 * Perspective camera producing the view and projection matrices.
 *
 * Perspective rather than orthographic: depth cues make the face orientations
 * readable, and the pointer interaction of later phases is designed against a
 * perspective view.
 */
class Camera {
public:
    /**
     * @param vertical_fov Vertical field of view in radians.
     * @param aspect Viewport width divided by height.
     */
    Camera(const math::Vec3& eye, const math::Vec3& target,
           const math::Vec3& up, float vertical_fov, float aspect,
           float near_plane, float far_plane) noexcept
        : eye_(eye),
          target_(target),
          up_(up),
          vertical_fov_(vertical_fov),
          aspect_(aspect),
          near_plane_(near_plane),
          far_plane_(far_plane)
    {
    }

    [[nodiscard]] math::Mat4 view() const noexcept
    {
        return math::look_at(eye_, target_, up_);
    }

    [[nodiscard]] math::Mat4 projection() const noexcept
    {
        return math::perspective(vertical_fov_, aspect_, near_plane_,
                                 far_plane_);
    }

    [[nodiscard]] float near_plane() const noexcept { return near_plane_; }

    [[nodiscard]] const math::Vec3& eye() const noexcept { return eye_; }

private:
    math::Vec3 eye_;
    math::Vec3 target_;
    math::Vec3 up_;
    float vertical_fov_;
    float aspect_;
    float near_plane_;
    float far_plane_;
};

/**
 * Lens and depth range shared by every viewpoint of the cube.
 *
 * Named here rather than kept in the implementation because a viewpoint that
 * moves has to build cameras differing from the home one in the eye alone.
 */
inline constexpr float kVerticalFov = 3.14159265358979323846f / 4.0f;
inline constexpr float kNearPlane = 1.0f;
inline constexpr float kFarPlane = 10.0f;

/** Where the cube sits, and which way is up from every viewpoint. */
inline constexpr math::Vec3 kCubeCenter{0.0f, 0.0f, 0.0f};
inline constexpr math::Vec3 kWorldUp{0.0f, 1.0f, 0.0f};

/**
 * The eye of the home viewpoint, which also fixes the orbit radius.
 *
 * On the (1, 1, 1) diagonal so the +X, +Y and +Z faces are visible and
 * symmetric, far enough that the cube silhouette stays inside a square
 * viewport with margin. The rendered scene contract samples are derived from
 * this viewpoint, so moving it means changing the contract too.
 */
inline constexpr math::Vec3 kHomeEye{3.0f, 3.0f, 3.0f};

/**
 * The camera of the home viewpoint.
 *
 * Delegates to the orbit at home rather than building its own camera, so the
 * viewpoint the contract was derived from has one definition instead of two
 * that agree only as far as floating point lets them.
 */
[[nodiscard]] Camera default_camera(float aspect) noexcept;

}  // namespace rubiks::graphics
