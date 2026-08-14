#pragma once

#include <array>
#include <iterator>
#include <vector>

#include "graphics/Color.hpp"
#include "math/Types.hpp"

namespace rubiks::graphics {

/** Screen-space quad ready to be filled by a renderer. */
struct RenderFace {
    std::array<math::Vec2, 4> points{};
    Color color{};
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
}

}  // namespace rubiks::graphics
