#pragma once

#include "graphics/Rect.hpp"
#include "math/Angle.hpp"
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
inline constexpr float kVerticalFov = math::kPi / 4.0f;
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
 * Where a point in normalized device coordinates lands in a viewport, in
 * pixels.
 *
 * NDC runs from -1 to 1 with +Y up; pixels count from the top left with y
 * down. So (-1, 1) is the viewport's top-left corner and (1, -1) its
 * bottom-right. The viewport pass draws by this and interaction projects a
 * point of the cube to the screen by it, and a drag only follows what is
 * drawn while the two are the same arithmetic.
 */
[[nodiscard]] inline math::Vec2 ndc_to_pixels(const Rect& viewport,
                                              const math::Vec2& ndc) noexcept
{
    return math::Vec2{viewport.x + (ndc.x + 1.0f) * 0.5f * viewport.width,
                      viewport.y + (1.0f - ndc.y) * 0.5f * viewport.height};
}

/**
 * The inverse of ndc_to_pixels(): a pixel of the buffer as a point in NDC.
 *
 * Divides by the viewport's width and height, so the caller has to have
 * turned away a viewport without either.
 */
[[nodiscard]] inline math::Vec2 pixels_to_ndc(const Rect& viewport,
                                              const math::Vec2& point) noexcept
{
    return math::Vec2{
        (point.x - viewport.x) / viewport.width * 2.0f - 1.0f,
        1.0f - (point.y - viewport.y) / viewport.height * 2.0f};
}

}  // namespace rubiks::graphics
