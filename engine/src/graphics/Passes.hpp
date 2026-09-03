#pragma once

#include <optional>
#include <vector>

#include "graphics/Camera.hpp"
#include "graphics/Light.hpp"
#include "graphics/Rect.hpp"
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

/** Applies the model transform to faces and casters, staying in world space. */
struct TransformPass {
    math::Mat4 model_matrix;

    [[nodiscard]] WorldScene operator()(WorldScene scene) const;
};

[[nodiscard]] TransformPass transform(const math::Transform& model) noexcept;

/**
 * Casts the casters onto the ground from the light, in world space.
 *
 * Fills `WorldScene::shadow` with one convex polygon per caster, the fade
 * anchors and an opacity already thinned by how close the eye is to the
 * ground. Leaves it empty when there is nothing to cast, when
 * the light is not above the cube, or when the eye is at or below the ground.
 *
 * Runs before `view` because the ground is a world-space plane. Reads the
 * camera only for the height of the eye.
 */
struct ShadowPass {
    /** The key light, which alone casts the shadow; absent when there is none. */
    std::optional<Light> key;
    float ground_y;
    math::Vec3 eye;

    [[nodiscard]] WorldScene operator()(WorldScene scene) const;
};

[[nodiscard]] ShadowPass shadow(const Lighting& lighting, const Camera& camera,
                                float ground_y = kGroundY) noexcept;

/** World space to view space, faces and shadow alike. */
struct ViewPass {
    math::Mat4 view_matrix;

    [[nodiscard]] ViewScene operator()(const WorldScene& scene) const;
};

[[nodiscard]] ViewPass view(const Camera& camera) noexcept;

/**
 * Blinn-Phong on every face, in view space.
 *
 * Diffuse is one value per plane: a face is flat, so its normal is one vector,
 * and the direction to the light is read at one point of the plane -- the
 * point nearest the cube's centre -- so that the stickers sharing a plane
 * share a shade. Specular is carried as a highlight -- where on the face the light
 * reflects into the eye and how brightly -- for the renderer to draw as a
 * falloff, because a single value across the face would not read as a glint.
 *
 * The light arrives in world space and is carried into view space here, once,
 * at construction; that is what keeps it fixed in the world when the camera
 * orbits rather than riding along with the eye.
 */
struct LightPass {
    Lighting lighting;
    /** The lamps' positions in view space, in the lighting's order. */
    std::vector<math::Vec3> lamps_in_view;
    /**
     * The cube's centre in view space. Diffuse is evaluated at the point of
     * each face's plane nearest this, so every sticker on one plane shades to
     * one byte value -- evaluated at each sticker's own centre they came out a
     * few units apart, and a face that is nine slightly different colours is
     * one the render contract cannot name.
     */
    math::Vec3 anchor_in_view;

    [[nodiscard]] ViewScene operator()(ViewScene scene) const;
};

[[nodiscard]] LightPass light(const Lighting& lighting, const Camera& camera) noexcept;

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

/**
 * NDC to screen-space pixels, flipping Y to the top-left origin.
 *
 * The target is a sub-rectangle rather than the whole buffer, because the 3D
 * cube shares the canvas with the net view. Keeping that rectangle square is
 * also what frees the camera aspect from the canvas aspect.
 *
 * The shadow gets the rectangle as its clip and a blur width sized to it,
 * and its casters become the convex silhouettes the renderer hides it behind.
 * This is the one pass that knows the rectangle, so it is the one that writes
 * them.
 */
struct ViewportPass {
    Rect rect;
    /** Where the shadow may be drawn; the viewport itself unless told otherwise. */
    Rect clip;

    [[nodiscard]] RenderScene operator()(const ClipScene& scene) const;
};

[[nodiscard]] ViewportPass viewport(const Rect& rect) noexcept;

/**
 * The same, with the shadow allowed onto a stage wider than the viewport.
 *
 * The cube's viewport is a square, but the canvas round it is not, and the
 * ground shadow leans out of the square into the margins. The stage is the
 * canvas less whatever flat views share it, so the shadow can use the room
 * that is there without reaching the drawing beside it.
 */
[[nodiscard]] ViewportPass viewport(const Rect& rect, const Rect& stage) noexcept;

}  // namespace rubiks::graphics
