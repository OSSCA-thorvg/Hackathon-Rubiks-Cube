#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "graphics/Color.hpp"
#include "math/Types.hpp"

/**
 * Intermediate representations flowing between pipeline passes.
 *
 * Each stage has its own type so a pass cannot be applied to the wrong space
 * by accident; the compiler rejects `view(camera)` applied to view-space data.
 */
namespace rubiks::graphics {

/**
 * A box that casts a shadow but is never drawn.
 *
 * The stickers cannot cast the shadow themselves: they have seams between
 * them and no body behind them, so their shadow would be a grid. A caster is
 * a whole run of layers -- the whole cube at rest -- and its eight corners are
 * enough, because the shadow of a convex box is the convex hull of the shadows
 * of its corners.
 */
struct Caster {
    std::array<math::Vec3, 8> corners{};
};

/**
 * The shadow as one thing, carried through the passes beside the faces.
 *
 * Fade, opacity and the occluding silhouettes belong to the shadow as a
 * whole rather than to any one polygon of it, which is why this is a group
 * with a few points of its own and not a list of shadow faces. Every point in
 * it goes through the same transforms as a face corner.
 */
template <typename Point>
struct ShadowGroup {
    /** One convex polygon on the ground per caster. */
    std::vector<std::vector<Point>> polygons;
    /** The casters again, whose silhouettes the shadow must not show through. */
    std::vector<std::array<Point, 8>> occluders;
    /** Where the shadow is darkest: the cube's centre dropped onto the ground. */
    Point fade_start{};
    /** Where it has thinned to its floor: the centre cast from the light. */
    Point fade_end{};
    /** Points round the ambient patch directly under the cube. */
    std::vector<Point> contact;
    /** Strength before the renderer's own fade and blur, 0 meaning none. */
    std::uint8_t opacity = 0;
};

/**
 * The glint on one face, as geometry rather than as a colour.
 *
 * `centre` is where the light reflects straight into the eye, pulled onto the
 * face if it falls outside; `rim` is one radius away from it along the face,
 * so that projecting both gives the renderer a screen radius that shrinks with
 * distance the way the face does. Peak strength is `alpha`.
 */
template <typename Point>
struct HighlightOf {
    /** On the plane; outside the face when the glint only reaches into it. */
    Point centre{};
    Point rim{};
    /** Strength at the centre and half way to the rim, to shape the lobe. */
    std::uint8_t alpha = 0;
    std::uint8_t mid = 0;
};

/**
 * How a face's brightness runs across it.
 *
 * The brightness function belongs to the plane: one axis from the key
 * light's foot through the middle of the cube face, spanning every face on
 * the plane, and one set of evenly spaced stops with the true brightness at
 * each. Every sticker on the plane carries that same axis and those same
 * stops; only the colours differ, each sticker's own colour at each stop's
 * brightness. That is what makes the shading run on across the seams -- the
 * same function under a red sticker and the green one beside it.
 */
template <typename Point>
struct ShadingOf {
    Point from{};
    Point to{};
    /** This face's colour at each stop, evenly spaced from `from` to `to`. */
    std::vector<Color> colors;
};

/** Quad in world space. */
struct WorldFace {
    std::array<math::Vec3, 4> points{};
    Color color{};
};

struct WorldScene {
    std::vector<WorldFace> faces;
    /** Shadow casters, read by the shadow pass and drawn by nobody. */
    std::vector<Caster> casters;
    /** Filled in by the shadow pass; empty until then and when there is none. */
    std::optional<ShadowGroup<math::Vec3>> shadow;
};

/** Quad in view space, where the camera sits at the origin looking down -Z. */
struct ViewFace {
    std::array<math::Vec3, 4> points{};
    /** After lighting: the plane's colour where it comes nearest the cube's centre. */
    Color color{};
    /** Set by the lighting pass; absent for a face no lamp reaches. */
    std::optional<ShadingOf<math::Vec3>> shading;
    /** Set by the lighting pass: one per lamp whose glint reaches this face. */
    std::vector<HighlightOf<math::Vec3>> highlights;
};

struct ViewScene {
    std::vector<ViewFace> faces;
    std::optional<ShadowGroup<math::Vec3>> shadow;
};

/**
 * Quad after the perspective divide.
 *
 * Culling reads `ndc`, which still has +Y up so a front face has a positive
 * signed area. Depth sorting reads `depth`, the mean view-space z of the four
 * corners; it is negative in front of the camera and decreases with distance.
 */
struct ClipFace {
    std::array<math::Vec2, 4> ndc{};
    float depth = 0.0f;
    Color color{};
    std::optional<ShadingOf<math::Vec2>> shading;
    std::vector<HighlightOf<math::Vec2>> highlights;
};

struct ClipScene {
    std::vector<ClipFace> faces;
    std::optional<ShadowGroup<math::Vec2>> shadow;
};

}  // namespace rubiks::graphics
