#pragma once

#include <array>
#include <cstdint>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include "graphics/Color.hpp"
#include "graphics/Rect.hpp"
#include "math/Types.hpp"

namespace rubiks::graphics {

/**
 * The glint on one face, in pixels.
 *
 * Drawn over the face it belongs to and nowhere else: the renderer lays it
 * down straight after that face, so a highlight on a face at the back cannot
 * land on a face drawn later in front of it. Rendered as a radial falloff from
 * `centre` to the distance of `rim`, screened onto the face.
 */
struct RenderHighlight {
    math::Vec2 centre{};
    math::Vec2 rim{};
    std::uint8_t alpha = 0;
    /** Strength half way from centre to rim, so the falloff has a shape. */
    std::uint8_t mid = 0;
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
    /** Points round the soft patch directly under the cube. */
    std::vector<math::Vec2> contact;
    std::uint8_t opacity = 0;
    /** Gaussian sigma of the edge, in pixels. */
    float blur_sigma = 0.0f;
    /** The region the shadow may be drawn in. */
    Rect clip;
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
