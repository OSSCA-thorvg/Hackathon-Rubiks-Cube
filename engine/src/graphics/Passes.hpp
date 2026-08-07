#pragma once

#include <cstdint>

#include "graphics/Camera.hpp"
#include "graphics/RenderScene.hpp"
#include "graphics/Scene.hpp"
#include "math/Transform.hpp"
#include "math/Types.hpp"

/**
 * Stateless pipeline passes, each a plain Input -> Output callable.
 *
 * The factory functions below return the callable so passes compose with
 * operator| (see Pipeline.hpp), but every pass is also usable on its own —
 * `cull()(scene)` — which is how the unit tests exercise them.
 */
namespace rubiks::graphics {

/** Applies the model transform, staying in world space. */
struct TransformPass {
    math::Mat4 model_matrix;

    [[nodiscard]] WorldScene operator()(WorldScene scene) const;
};

[[nodiscard]] TransformPass transform(const math::Transform& model) noexcept;

/** World space to view space. */
struct ViewPass {
    math::Mat4 view_matrix;

    [[nodiscard]] ViewScene operator()(const WorldScene& scene) const;
};

[[nodiscard]] ViewPass view(const Camera& camera) noexcept;

/**
 * View space to normalized device coordinates, carrying the depth key.
 *
 * Faces with any vertex at or in front of the near plane are dropped whole
 * rather than clipped: the perspective divide diverges there, and polygon
 * clipping is not needed while the camera is fixed.
 */
struct ProjectPass {
    math::Mat4 projection_matrix;
    float near_plane;

    [[nodiscard]] ClipScene operator()(const ViewScene& scene) const;
};

[[nodiscard]] ProjectPass project(const Camera& camera) noexcept;

/**
 * Drops back faces.
 *
 * Judged in NDC, where +Y is still up and a counter-clockwise front face has
 * a positive signed area; doing it after the viewport flip would invert the
 * sign for no benefit.
 */
struct CullPass {
    [[nodiscard]] ClipScene operator()(ClipScene scene) const;
};

[[nodiscard]] CullPass cull() noexcept;

/**
 * Orders faces back to front by mean view-space z.
 *
 * A single convex cube needs only culling, but a layer mid-rotation is not
 * convex, so the ordering is established and tested now. The sort is stable,
 * so faces at equal depth keep their input order and the output is
 * deterministic.
 */
struct DepthSortPass {
    [[nodiscard]] ClipScene operator()(ClipScene scene) const;
};

[[nodiscard]] DepthSortPass depth_sort() noexcept;

/** NDC to screen-space pixels, flipping Y to the top-left origin. */
struct ViewportPass {
    float width;
    float height;

    [[nodiscard]] RenderScene operator()(const ClipScene& scene) const;
};

[[nodiscard]] ViewportPass viewport(std::uint32_t width,
                                    std::uint32_t height) noexcept;

}  // namespace rubiks::graphics
