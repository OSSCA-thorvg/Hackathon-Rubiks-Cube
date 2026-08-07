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

    /** Updates the viewport aspect ratio; other parameters are unchanged. */
    void set_aspect(float aspect) noexcept { aspect_ = aspect; }

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
 * Camera pinned by the Phase 3 spec.
 *
 * The eye sits on the (1, 1, 1) diagonal so the +X, +Y, and +Z faces are
 * visible and symmetric, far enough that the cube silhouette stays inside a
 * square viewport with margin. The rendered scene contract samples are derived
 * from these values, so changing them means changing the contract too.
 */
[[nodiscard]] Camera default_camera(float aspect) noexcept;

}  // namespace rubiks::graphics
