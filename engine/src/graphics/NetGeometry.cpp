#include "graphics/NetGeometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "graphics/Layout.hpp"
#include "graphics/NetRing.hpp"
#include "graphics/SlotRing.hpp"
#include "graphics/Palette.hpp"
#include "math/Types.hpp"

namespace rubiks::graphics {
namespace {

using cube::Axis;
using cube::Face;
using cube::axis_of;
using cube::coordinate_on;
using cube::outer_layer;
using math::Vec2;

constexpr float kDegreesPerQuarterTurn = 90.0f;
constexpr float kPi = 3.14159265358979323846f;

/** Where one cell of the net lands, given the rectangle the net fills. */
struct NetMetrics {
    float face_side;
    float cell;
    float inset;
    float sticker;
};

[[nodiscard]] NetMetrics net_metrics(const Rect& rect, int size) noexcept
{
    const float cell = net_cell_side(rect, size);

    return NetMetrics{net_face_side(rect), cell,
                      cell * (1.0f - kNetStickerScale) * 0.5f,
                      cell * kNetStickerScale};
}

/**
 * Where a face's block sits in the cross.
 *
 * Every placement in this file starts from one of these two. The middle in
 * particular is what a Z turn and the guide circle it promises both turn
 * about, and they stay concentric because they ask the same function rather
 * than because two copies of the arithmetic happen to agree.
 */
[[nodiscard]] Vec2 net_face_origin(Face face, const Rect& rect) noexcept
{
    const auto block = net_block(face);
    const float side = net_face_side(rect);

    return Vec2{rect.x + static_cast<float>(block.column) * side,
                rect.y + static_cast<float>(block.row) * side};
}

[[nodiscard]] Vec2 net_face_center(Face face, const Rect& rect) noexcept
{
    const Vec2 origin = net_face_origin(face, rect);
    const float half = 0.5f * net_face_side(rect);

    return Vec2{origin.x + half, origin.y + half};
}

/** The square one cell of an unfolded face occupies, without its color. */
[[nodiscard]] RenderFace sticker_quad(const Vec2& origin, int col, int row,
                                      const NetMetrics& metrics) noexcept
{
    const float left =
        origin.x + static_cast<float>(col) * metrics.cell + metrics.inset;
    const float top =
        origin.y + static_cast<float>(row) * metrics.cell + metrics.inset;

    RenderFace quad;
    quad.points = {Vec2{left, top}, Vec2{left + metrics.sticker, top},
                   Vec2{left + metrics.sticker, top + metrics.sticker},
                   Vec2{left, top + metrics.sticker}};
    return quad;
}

/** The sticker a cell shows, drawn as the quad it rests in. */
[[nodiscard]] RenderFace resting_quad(const cube::CubeState& state,
                                      const NetCell& cell, const Vec2& origin,
                                      int col, int row,
                                      const NetMetrics& metrics,
                                      Palette palette)
{
    RenderFace quad = sticker_quad(origin, col, row, metrics);
    quad.color =
        to_color(state.at(cell.x, cell.y, cell.z).sticker(cell.face), palette);
    return quad;
}

[[nodiscard]] bool in_layers(cube::LayerMask layers, int index) noexcept
{
    return (layers & cube::layer(index)) != 0;
}

/** Turns a quad about a point, clockwise on screen for a positive angle. */
void rotate_quad(RenderFace& quad, const Vec2& center, float degrees) noexcept
{
    const float radians = degrees * kPi / 180.0f;
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);

    for (auto& point : quad.points) {
        const Vec2 offset{point.x - center.x, point.y - center.y};
        point = Vec2{center.x + offset.x * cosine - offset.y * sine,
                     center.y + offset.x * sine + offset.y * cosine};
    }
}

/** A circle as a closed path of four cubic quarters. */
[[nodiscard]] RenderStroke circle_stroke(const Vec2& center, float radius,
                                         float width, const Color& color)
{
    // The same arm the rings round their corners with: a quarter of a circle
    // turns through a right angle, and the straight line across it is the
    // radius times root two.
    const float arm = arc_arm(radius * std::sqrt(2.0f), 0.5f * kPi);

    const std::array<Vec2, 4> around{
        Vec2{center.x, center.y - radius}, Vec2{center.x + radius, center.y},
        Vec2{center.x, center.y + radius}, Vec2{center.x - radius, center.y}};
    const std::array<Vec2, 4> along{Vec2{1.0f, 0.0f}, Vec2{0.0f, 1.0f},
                                    Vec2{-1.0f, 0.0f}, Vec2{0.0f, -1.0f}};

    RenderStroke path;
    path.start = around[0];
    path.closed = true;
    path.width = width;
    path.color = color;

    for (std::size_t quarter = 0; quarter < around.size(); ++quarter) {
        const std::size_t next = (quarter + 1) % around.size();
        path.segments.push_back(RenderSegment{
            Vec2{around[quarter].x + along[quarter].x * arm,
                 around[quarter].y + along[quarter].y * arm},
            Vec2{around[next].x - along[next].x * arm,
                 around[next].y - along[next].y * arm},
            around[next]});
    }
    return path;
}

/**
 * How far a cell has turned, taken against the way its ring ran where it set
 * off rather than against the screen.
 *
 * At rest this is exactly zero, so a resting cell is axis-aligned however the
 * loop is shaped, and along a straight run it stays zero, so a slide still
 * looks like a slide. Both tangents are one of the drawn directions, so a
 * finished quarter turn lands on a multiple of a right angle, which a square
 * sticker cannot show.
 */
[[nodiscard]] float degrees_between(const Vec2& from, const Vec2& to) noexcept
{
    const float turn = std::atan2(to.y, to.x) - std::atan2(from.y, from.x);
    return turn * 180.0f / kPi;
}

/** Grows a quad about its own middle. */
void scale_quad(RenderFace& quad, float factor) noexcept
{
    const Vec2 center{0.25f * (quad.points[0].x + quad.points[1].x +
                              quad.points[2].x + quad.points[3].x),
                      0.25f * (quad.points[0].y + quad.points[1].y +
                               quad.points[2].y + quad.points[3].y)};

    for (auto& point : quad.points) {
        point = Vec2{center.x + (point.x - center.x) * factor,
                     center.y + (point.y - center.y) * factor};
    }
}

void translate_quad(RenderFace& quad, float dx, float dy) noexcept
{
    for (auto& point : quad.points) {
        point.x += dx;
        point.y += dy;
    }
}

/** A turn about a point followed by a shift: what carries a whole group. */
struct RigidMove {
    Vec2 about{};
    float degrees = 0.0f;
    Vec2 shift{};
};

void apply_move(RenderFace& quad, const RigidMove& move) noexcept
{
    rotate_quad(quad, move.about, move.degrees);
    translate_quad(quad, move.shift.x, move.shift.y);
}

/**
 * How each face's worth of a band moves at this point in the turn.
 *
 * A band travels as N groups of N rather than as 4N loose cells. A group rests
 * in a straight line on one face and lands in a straight line on the next, so
 * one rigid move takes it the whole way: read the ring at the group's middle,
 * and turn the group by however far the loop has turned under it. What the eye
 * gets is a strip of a face travelling, which it can follow, rather than a
 * queue of stickers each rounding the bends on its own.
 *
 * Reading the ring only at the middle is also what keeps the strip straight
 * through a bend, where the slots the cells would ride separately are not in
 * line with each other at all.
 */
[[nodiscard]] std::vector<RigidMove> group_moves(const SlotRing& ring, int size,
                                                 float progress)
{
    std::vector<RigidMove> moves;
    if (ring.empty() || size < 1) return moves;

    const auto span = static_cast<std::size_t>(size);
    moves.reserve(ring.slot_count() / span);

    for (std::size_t first = 0; first + span <= ring.slot_count();
         first += span) {
        // The middle of the group: a slot itself where the size is odd, and
        // halfway between the two middle ones where it is even.
        const float middle = static_cast<float>(first) +
                             0.5f * static_cast<float>(size - 1);

        const auto resting = ring.at(middle);
        const auto now = ring.at(middle + progress * static_cast<float>(size));

        moves.push_back(
            RigidMove{resting.position,
                      degrees_between(resting.tangent, now.tangent),
                      Vec2{now.position.x - resting.position.x,
                           now.position.y - resting.position.y}});
    }
    return moves;
}

/**
 * One turning layer's ring, together with how its groups are moving now.
 *
 * The two are one thing: a ring is only read to find which group a sticker is
 * in, and a group's move is only meaningful against the ring it was read from.
 * Kept side by side in two arrays they were free to fall out of step.
 */
struct TurningBand {
    SlotRing ring;
    std::vector<RigidMove> moves;

    /** The move carrying a sticker, or nothing when this band has it not. */
    [[nodiscard]] const RigidMove* move_for(const NetCell& cell,
                                            int size) const noexcept
    {
        const auto slot = ring.slot_of(cell);
        if (!slot) return nullptr;

        // Slots run in group order, so a slot's group is its index over N.
        return &moves[*slot / static_cast<std::size_t>(size)];
    }
};

/** A step between two cells, in the cube's index coordinates. */
struct Step {
    int x;
    int y;
    int z;
};

[[nodiscard]] Step step_between(const NetCell& from, const NetCell& to) noexcept
{
    return Step{to.x - from.x, to.y - from.y, to.z - from.z};
}

[[nodiscard]] Step cross_product(const Step& a, const Step& b) noexcept
{
    return Step{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
}

/**
 * How far through its quarter turn the net draws a rotation.
 *
 * Not capped: the rings wrap, so a drag carried past a quarter simply keeps
 * going round the loop it is on.
 */
[[nodiscard]] float net_progress(float angle_degrees) noexcept
{
    if (!std::isfinite(angle_degrees)) return 0.0f;

    return angle_degrees / kDegreesPerQuarterTurn;
}

}  // namespace

std::array<Face, 6> net_faces() noexcept
{
    return {Face::Up,    Face::Left, Face::Front,
            Face::Right, Face::Back, Face::Down};
}

NetBlock net_block(Face face) noexcept
{
    // Up on top, the four side faces in a row, Down below:
    //     U
    //   L F R B
    //     D
    switch (face) {
        case Face::Up:
            return NetBlock{1, 0};
        case Face::Left:
            return NetBlock{0, 1};
        case Face::Front:
            return NetBlock{1, 1};
        case Face::Right:
            return NetBlock{2, 1};
        case Face::Back:
            return NetBlock{3, 1};
        case Face::Down:
            break;
    }
    return NetBlock{1, 2};
}

NetCell net_cell(Face face, int col, int row, int size) noexcept
{
    const int last = size - 1;

    switch (face) {
        case Face::Up:
            return NetCell{col, last, row, Face::Up};
        case Face::Left:
            return NetCell{0, last - row, col, Face::Left};
        case Face::Front:
            return NetCell{col, last - row, last, Face::Front};
        case Face::Right:
            return NetCell{last, last - row, last - col, Face::Right};
        case Face::Back:
            return NetCell{last - col, last - row, 0, Face::Back};
        case Face::Down:
            break;
    }
    return NetCell{col, 0, last - row, Face::Down};
}

std::optional<NetPosition> net_position(const NetCell& cell, int size) noexcept
{
    // The inverse by search rather than by a second set of formulas: six
    // faces of N squared cells is nothing to walk, and one mapping cannot
    // disagree with itself.
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) {
            if (net_cell(cell.face, col, row, size) == cell) {
                return NetPosition{cell.face, col, row};
            }
        }
    }
    return std::nullopt;
}

float net_face_side(const Rect& rect) noexcept
{
    return rect.width / static_cast<float>(kNetColumns);
}

float net_cell_side(const Rect& rect, int size) noexcept
{
    return net_face_side(rect) / static_cast<float>(size);
}

math::Vec2 net_cell_center(Face face, int col, int row, const Rect& rect,
                           int size) noexcept
{
    const Vec2 origin = net_face_origin(face, rect);
    const float cell = net_cell_side(rect, size);

    return Vec2{origin.x + (static_cast<float>(col) + 0.5f) * cell,
                origin.y + (static_cast<float>(row) + 0.5f) * cell};
}

std::optional<NetTurn> net_step_turn(Face face, int col, int row, int col_step,
                                     int row_step, int size) noexcept
{
    // The two cell positions a step apart have to be distinguishable, which
    // needs a face at least two cells across.
    if (size < 2) return std::nullopt;
    if (std::abs(col_step) + std::abs(row_step) != 1) return std::nullopt;
    if (col < 0 || col >= size || row < 0 || row >= size) return std::nullopt;

    // What one step along each of the face's drawn directions is in cube
    // space. Taken as a difference so it comes from the same mapping that
    // decides which sticker each cell shows.
    const auto origin = net_cell(face, 0, 0, size);
    const Step along_col = step_between(origin, net_cell(face, 1, 0, size));
    const Step along_row = step_between(origin, net_cell(face, 0, 1, size));

    const Step travel{along_col.x * col_step + along_row.x * row_step,
                      along_col.y * col_step + along_row.y * row_step,
                      along_col.z * col_step + along_row.z * row_step};

    // The face's outward normal, from those same two steps rather than from a
    // table: every block is drawn as its face looks from outside the cube, so
    // the drawn row and column always wind that way round.
    const Step normal = cross_product(along_row, along_col);

    // A rotation about this vector carries the sticker along `travel`. Only
    // its direction matters, and it is always one of the six axis directions,
    // because both inputs are.
    const Step turn = cross_product(travel, normal);
    const auto cell = net_cell(face, col, row, size);

    if (turn.x != 0) return NetTurn{Axis::X, cell.x, turn.x > 0 ? 1 : -1};
    if (turn.y != 0) return NetTurn{Axis::Y, cell.y, turn.y > 0 ? 1 : -1};
    if (turn.z != 0) return NetTurn{Axis::Z, cell.z, turn.z > 0 ? 1 : -1};

    return std::nullopt;
}

RenderScene build_net_guides(const std::vector<NetGuide>& guides,
                             const Rect& rect, int size)
{
    RenderScene scene;
    scene.strokes.reserve(guides.size());

    const Vec2 pivot = net_face_center(Face::Front, rect);
    const float width = kNetGuideWidthCells * net_cell_side(rect, size);
    for (const auto& guide : guides) {
        const auto color = guide_color(guide.axis);

        if (guide.axis == Axis::Z) {
            // Drawn where it happens, so this cell's path is its own circle
            // about the front block's middle rather than a loop it shares.
            const auto placed = net_position(guide.cell, size);
            if (!placed) continue;

            const auto center = net_cell_center(placed->face, placed->col,
                                                placed->row, rect, size);
            const float radius =
                std::sqrt((center.x - pivot.x) * (center.x - pivot.x) +
                          (center.y - pivot.y) * (center.y - pivot.y));
            scene.strokes.push_back(circle_stroke(pivot, radius, width, color));
            continue;
        }

        const auto ring = net_ring(guide.axis, guide.layer, rect, size);
        if (ring.empty()) continue;
        scene.strokes.push_back(ring.stroke(width, color));
    }
    return scene;
}

RenderScene build_net_scene(const cube::CubeState& state, const Rect& rect,
                            Palette palette)
{
    const int size = state.size();
    const NetMetrics metrics = net_metrics(rect, size);

    RenderScene scene;
    scene.faces.reserve(static_cast<std::size_t>(6 * size * size));

    for (const auto face : net_faces()) {
        const Vec2 origin = net_face_origin(face, rect);

        for (int row = 0; row < size; ++row) {
            for (int col = 0; col < size; ++col) {
                const auto source = net_cell(face, col, row, size);
                scene.faces.push_back(resting_quad(state, source, origin, col,
                                                   row, metrics, palette));
            }
        }
    }
    return scene;
}

RenderScene build_net_painting(const std::vector<cube::FaceColor>& painting,
                               int size, const Rect& rect, Palette palette,
                               const std::vector<int>& blamed)
{
    const auto all = cube::surface_stickers(size);
    if (painting.size() != all.size()) return {};

    // Where each sticker sits in the painting, laid out flat so that a cell of
    // the net can be turned into a colour without a search per square.
    std::vector<int> at(static_cast<std::size_t>(size) * size * size *
                            cube::kFaceCount,
                        -1);
    const auto slot_of = [size](const cube::SurfaceSticker& s) {
        return (static_cast<std::size_t>((s.x * size + s.y) * size + s.z)) *
                   cube::kFaceCount +
               cube::face_index(s.face);
    };
    for (std::size_t i = 0; i < all.size(); ++i) {
        at[slot_of(all[i])] = static_cast<int>(i);
    }

    const NetMetrics metrics = net_metrics(rect, size);

    RenderScene scene;
    scene.faces.reserve(static_cast<std::size_t>(6 * size * size));

    for (const auto face : net_faces()) {
        const Vec2 origin = net_face_origin(face, rect);

        for (int row = 0; row < size; ++row) {
            for (int col = 0; col < size; ++col) {
                const auto cell = net_cell(face, col, row, size);
                const int index = at[slot_of(cell)];
                if (index < 0) continue;

                RenderFace quad = sticker_quad(origin, col, row, metrics);
                quad.color = to_color(
                    painting[static_cast<std::size_t>(index)], palette);
                scene.faces.push_back(quad);
            }
        }
    }

    // The squares a refusal points at, ringed in the colour the axis guides do
    // not use, so a complaint reads as a complaint and not as another axis.
    const float width = kNetBlameWidthCells * net_cell_side(rect, size);
    for (const int index : blamed) {
        if (index < 0 || static_cast<std::size_t>(index) >= all.size()) continue;

        const auto placed = net_position(all[static_cast<std::size_t>(index)],
                                         size);
        if (!placed) continue;

        const Vec2 middle =
            net_cell_center(placed->face, placed->col, placed->row, rect, size);
        const float half = 0.5f * kNetStickerScale * net_cell_side(rect, size);

        RenderStroke ring;
        ring.start = Vec2{middle.x - half, middle.y - half};
        ring.closed = true;
        ring.width = width;
        ring.color = blame_color();
        for (const auto& corner :
             {Vec2{middle.x + half, middle.y - half},
              Vec2{middle.x + half, middle.y + half},
              Vec2{middle.x - half, middle.y + half},
              Vec2{middle.x - half, middle.y - half}}) {
            ring.segments.push_back(RenderSegment{corner, corner, corner});
        }
        scene.strokes.push_back(std::move(ring));
    }
    return scene;
}

RenderScene build_net_scene(const cube::CubeState& state, const Rect& rect,
                            const std::optional<ActiveRotation>& active,
                            Palette palette)
{
    if (!active) return build_net_scene(state, rect, palette);

    const int size = state.size();
    const NetMetrics metrics = net_metrics(rect, size);
    const float progress = net_progress(active->angle_degrees);

    // The cross draws a Z turn where it happens. The front block's border is
    // that layer's band, and the four blocks around it are laid out four-fold
    // about that block's middle, so every Z band lands on its destination
    // under one rotation about one point -- the same rotation the front face
    // is already making. Drawn that way the band never comes away from the
    // face it is turning with, which is what the eye is watching for.
    //
    // No other axis is drawn in one place: the cross has to cut those bands,
    // which is what the rings below are for.
    const bool pivots = active->axis == Axis::Z;
    const Vec2 pivot = net_face_center(Face::Front, rect);

    // One ring per turning layer otherwise: a wide move carries several bands
    // at once, and every cell rides the ring of the layer it is in.
    std::vector<TurningBand> bands(static_cast<std::size_t>(size));
    for (int index = 0; index < size && !pivots; ++index) {
        if (!in_layers(active->layers, index)) continue;

        auto& carried = bands[static_cast<std::size_t>(index)];
        carried.ring = net_ring(active->axis, index, rect, size);
        carried.moves = group_moves(carried.ring, size, progress);
    }

    // How far the piece is held off the page. It comes from the hand rather
    // than from the angle: a lift that followed the angle would rise by the
    // middle of a drag and settle again by the end of it, while the turn was
    // still being made.
    const float opening = std::clamp(active->opening, 0.0f, 1.0f);

    RenderScene scene;
    scene.faces.reserve(static_cast<std::size_t>(6 * size * size));

    // Pieces the turn is carrying wait here until every resting cell has been
    // drawn, kept apart by the storey they are held on: the face that spins in
    // place, and the pieces of the band that pass over it.
    std::vector<RenderFace> turning;
    std::vector<RenderFace> band_pieces;

    for (const auto face : net_faces()) {
        const Vec2 origin = net_face_origin(face, rect);
        const Vec2 center = net_face_center(face, rect);

        const bool turning_face =
            axis_of(face) == active->axis &&
            in_layers(active->layers, outer_layer(face, size));

        // Every block is drawn as its face looks from outside the cube, so the
        // face at the positive end of the axis turns the way the angle names
        // it and the one at the far end sees that same turn mirrored.
        const float face_degrees =
            progress * kDegreesPerQuarterTurn *
            (outer_layer(face, size) == size - 1 ? 1.0f : -1.0f);

        for (int row = 0; row < size; ++row) {
            for (int col = 0; col < size; ++col) {
                const auto source = net_cell(face, col, row, size);

                // The band is every cell the turn carries to another face:
                // those off the turning axis whose layer the turn takes.
                const bool band =
                    axis_of(face) != active->axis &&
                    in_layers(active->layers,
                              coordinate_on(active->axis, source));

                RenderFace quad = resting_quad(state, source, origin, col, row,
                                               metrics, palette);

                // The turn carries it away, or spins it in place, or leaves it
                // alone. Every cell is exactly one of the three.
                if (band) {
                    if (pivots) {
                        rotate_quad(quad, pivot,
                                    progress * kDegreesPerQuarterTurn);
                        band_pieces.push_back(quad);
                        continue;
                    }

                    // A quarter turn is one face's width along the ring,
                    // whichever axis it is, so the same line moves every band.
                    const auto layer_index = static_cast<std::size_t>(
                        coordinate_on(active->axis, source));
                    const auto* move =
                        bands[layer_index].move_for(source, size);

                    if (move) {
                        apply_move(quad, *move);
                        band_pieces.push_back(quad);
                    } else {
                        // The layer turns nothing the net draws as a ring.
                        scene.faces.push_back(quad);
                    }
                    continue;
                }

                if (turning_face) {
                    // The face on the axis turns on the spot. For the front
                    // face that is the very rotation the band beside it is
                    // making, about the very same point.
                    rotate_quad(quad, center, face_degrees);
                    turning.push_back(quad);
                    continue;
                }

                scene.faces.push_back(quad);
            }
        }
    }

    // A storey at a time, from the ground up. A lifted storey is one group
    // with one shadow -- of the pieces' combined outline, so a piece does not
    // shade the piece it is joined to, and all of the shadow falls on what is
    // below. The renderer draws every group over every resting face, and the
    // groups in this order, so what is higher shades what is lower and covers
    // it. A storey that is not lifted is not a group: its pieces go in with
    // the resting faces, and the frame is the same frame it always was.
    const auto raise = [&](std::vector<RenderFace>& pieces, float storey) {
        if (pieces.empty()) return;

        const auto shade = static_cast<std::uint8_t>(
            std::lround(kNetShadowAlpha * opening));
        for (auto& quad : pieces) {
            scale_quad(quad, 1.0f + kNetLiftScale * opening * storey);
        }

        if (shade == 0) {
            scene.faces.insert(scene.faces.end(), pieces.begin(), pieces.end());
            return;
        }

        // So far right and so far down, in cells but never under a pixel a
        // storey: the renderer moves the shadow by whole pixels, and the band
        // has to fall further than the face at the largest cube as well as at
        // the smallest. The shadow's distance is along the diagonal.
        const float along_axis =
            storey * opening *
            std::max(kNetLiftCells * metrics.cell, kNetLiftMinPixels);
        const float distance = along_axis * kNetLiftDiagonal;

        RenderGroup group;
        group.faces = std::move(pieces);
        group.shadow = LiftShadow{distance, shade, kNetShadowSigmaShare * distance};
        scene.groups.push_back(std::move(group));
    };

    raise(turning, kNetFaceStorey);
    raise(band_pieces, kNetBandStorey);
    return scene;
}

}  // namespace rubiks::graphics
