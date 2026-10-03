#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include "graphics/Color.hpp"
#include "graphics/Rect.hpp"
#include "math/Types.hpp"

namespace rubiks::graphics {

/** Stops of a glint's falloff, centre to rim. */
inline constexpr std::size_t kGlintStops = 5;

/**
 * The glint on one face, in pixels.
 *
 * Drawn over the face it belongs to and nowhere else: the renderer lays it
 * down straight after that face, so a highlight on a face at the back cannot
 * land on a face drawn later in front of it. Rendered as an elliptical falloff
 * from the middle of the four `rims` out to them, screened onto the face.
 */
struct RenderHighlight {
    /** The ends of the footprint's two axes on screen: +u, -u, +v, -v. */
    std::array<math::Vec2, 4> rims{};
    /** Strength at evenly spaced distances from centre to rim; the rim is 0. */
    std::array<std::uint8_t, kGlintStops> stops{};
};

/**
 * A face's colour running from one point to another.
 *
 * The lit 3D view fills a sticker with this rather than one colour: a point
 * light shades a flat face brighter near its foot and darker away from it.
 * The axis and the stop spacing are the plane's, shared by every sticker on
 * it; the colours are this sticker's own at each stop.
 */
struct RenderShading {
    math::Vec2 from{};
    math::Vec2 to{};
    std::vector<Color> colors;
    /** The stops' fractions along `from` to `to`; evenly spaced when empty. */
    std::vector<float> offsets;
};

/** Screen-space quad ready to be filled by a renderer. */
struct RenderFace {
    std::array<math::Vec2, 4> points{};
    /** The fill when there is no shading; the reference shade when there is. */
    Color color{};
    std::optional<RenderShading> shading;
    /** One per lamp whose glint reaches this face; drawn straight after it. */
    std::vector<RenderHighlight> highlights;
};

/**
 * The shadow's colour. Darker and cooler than either ground rather than grey,
 * and multiplied into the ground's colour rather than painted over it, so the
 * same constant holds on the light theme and the dark one. The renderer does
 * the multiplication itself, once per frame.
 */
inline constexpr Color kShadowColor{40, 48, 64, 255};

/**
 * The quality of the blur on the shadow's edge. Quality here is how many box
 * passes approximate the Gaussian; one pass reads as a soft edge on a shadow
 * and costs half of two.
 */
inline constexpr int kShadowBlurQuality = 30;

/**
 * How much of the shadow is left at its far tip. Nothing: a shadow that still
 * has a quarter of its strength where the clip cuts it shows the cut.
 */
inline constexpr float kShadowFadeFloor = 0.0f;

/** How much is left half way to the tip: the fade bends rather than runs straight. */
inline constexpr float kShadowFadeMidway = 0.35f;

/** A fade shorter than this in pixels has no direction and falls off radially. */
inline constexpr float kShadowFadeMinLength = 4.0f;

/**
 * The ground shadow, ready for the renderer to compose.
 *
 * The pipeline has decided where it lies; what is left is how it is made to
 * look like a shadow -- flattened so overlapping polygons do not darken twice,
 * blurred at the edge, thinned from `fade_start` towards `fade_end`, kept out
 * of the cube's own silhouette, and cut to `clip` so it cannot reach into the
 * drawing beside it. All of that is the renderer's, and all of the numbers it
 * needs are here without a renderer type among them.
 */
struct RenderShadow {
    std::vector<std::vector<math::Vec2>> polygons;
    math::Vec2 fade_start{};
    math::Vec2 fade_end{};
    std::uint8_t opacity = 0;
    /** Gaussian sigma of the edge, in pixels. */
    float blur_sigma = 0.0f;
    /** The region the shadow may be drawn in. */
    Rect clip;
};

/**
 * The shadow under a piece lifted off the flat drawing, in pixels.
 *
 * Not a lit shadow: the net is a diagram, and this says "this is held up"
 * rather than "the light is over there". So it has a distance and a softness
 * but no direction of its own -- every lift shadow falls the same way, down
 * and to the right, by `kLiftShadowAngleDegrees`.
 */
struct LiftShadow {
    /** How far the silhouette is pushed, in pixels. */
    float distance = 0.0f;
    std::uint8_t alpha = 0;
    /** Gaussian sigma of the edge, in pixels. */
    float sigma = 0.0f;
};

/**
 * Where a lift shadow falls: straight down and to the right.
 *
 * ThorVG's convention for a drop shadow's angle, which is the one the renderer
 * has to speak -- 0 degrees is up and the angle grows clockwise.
 */
inline constexpr float kLiftShadowAngleDegrees = 135.0f;

/**
 * Faces held together off the drawing, casting one shadow between them.
 *
 * The pieces a turn carries are drawn as a group rather than one by one, so
 * the shadow is of their combined silhouette: joined pieces do not shade each
 * other, and the shadow falls only on what lies below the group. A group
 * exists only while it is lifted; a piece at rest is an ordinary face.
 */
struct RenderGroup {
    std::vector<RenderFace> faces;
    LiftShadow shadow;
};

/** One cubic section of a path, continuing from the point before it. */
struct RenderSegment {
    math::Vec2 control_a{};
    math::Vec2 control_b{};
    math::Vec2 to{};
};

/**
 * A path drawn as a line rather than filled.
 *
 * Curves are carried as cubics because that is what they are, and because the
 * renderer has real curves and real strokes: approximating them with a chain
 * of thin quads would throw away anti-aliasing, joins and caps to work around
 * a boundary that had no reason to be that narrow.
 */
struct RenderStroke {
    math::Vec2 start{};
    std::vector<RenderSegment> segments;
    bool closed = false;
    float width = 1.0f;
    Color color{};
};

/**
 * Color of the cubie body: the cut surfaces exposed while a layer is turning,
 * and the slab silhouettes the renderer paints under the stickers so that the
 * seams between them show plastic rather than whatever is behind the cube.
 * Kept clearly apart from the background and from all six sticker colors so
 * a test can tell them apart.
 */
inline constexpr Color kBodyColor{70, 74, 82, 255};

/**
 * Final output of the graphics pipeline and the only input a renderer takes.
 *
 * Deliberately free of ThorVG types and of depth information: faces are
 * already ordered back to front, so a renderer draws them in order and needs
 * to know nothing about the 3D scene they came from.
 */
struct RenderScene {
    /**
     * Drawn before the faces, for a line the stickers sit on rather than over.
     *
     * The net's guides belong on top: they answer "where would this go", so
     * they have to be legible across whatever they cross. The ring diagram's
     * nine loops are the opposite -- they are the thing the stickers are
     * threaded onto, and a loop drawn over its own beads reads as a line ruled
     * across them.
     */
    std::vector<RenderStroke> underlays;
    /** Drawn after the underlays and before the faces: it lies on the ground. */
    std::optional<RenderShadow> shadow;
    /**
     * The cubies' bodies: each slab's convex screen silhouette, drawn after the
     * shadow and before the faces in the body colour. It is what shows in the
     * seams between stickers, and it hides the shadow behind the cube.
     */
    std::vector<std::vector<math::Vec2>> bodies;
    std::vector<RenderFace> faces;
    /**
     * Drawn after the faces and before the strokes: pieces lifted off the
     * drawing, each group with its shadow, lowest storey first. Whatever is
     * lifted is over everything at rest, by construction rather than by the
     * order the geometry happened to produce.
     */
    std::vector<RenderGroup> groups;
    /** Drawn after the faces, so guide lines read on top of the stickers. */
    std::vector<RenderStroke> strokes;
};

/** Moves everything in `source` onto the end of `target`. */
inline void append_scene(RenderScene& target, RenderScene&& source)
{
    target.underlays.insert(target.underlays.end(),
                            std::make_move_iterator(source.underlays.begin()),
                            std::make_move_iterator(source.underlays.end()));
    target.faces.insert(target.faces.end(),
                        std::make_move_iterator(source.faces.begin()),
                        std::make_move_iterator(source.faces.end()));
    target.groups.insert(target.groups.end(),
                         std::make_move_iterator(source.groups.begin()),
                         std::make_move_iterator(source.groups.end()));
    target.strokes.insert(target.strokes.end(),
                          std::make_move_iterator(source.strokes.begin()),
                          std::make_move_iterator(source.strokes.end()));
    // Only the 3D view casts one, so there is never a second to merge with.
    if (!target.shadow && source.shadow) target.shadow = std::move(source.shadow);
    target.bodies.insert(target.bodies.end(),
                         std::make_move_iterator(source.bodies.begin()),
                         std::make_move_iterator(source.bodies.end()));
}

}  // namespace rubiks::graphics
