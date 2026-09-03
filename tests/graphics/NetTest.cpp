#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"
#include "graphics/Layout.hpp"
#include "graphics/NetGeometry.hpp"
#include "graphics/NetRing.hpp"
#include "graphics/SlotRing.hpp"
#include "graphics/Palette.hpp"

namespace {

using Catch::Approx;
using namespace rubiks::graphics;
using rubiks::cube::CubeState;
using rubiks::cube::Face;
using rubiks::cube::FaceColor;
using rubiks::math::Vec2;

constexpr int kSize = 3;
constexpr int kLast = kSize - 1;

/** A turn with a finger still on it, which is when the drawing is open. */
constexpr float kHeldOpen = 1.0f;

std::size_t count_color(const RenderScene& scene, const Color& color)
{
    std::size_t total = 0;
    for (const auto& face : scene.faces) {
        if (face.color == color) ++total;
    }
    return total;
}

/** The cubie position a net cell reads, ignoring which face it shows. */
std::tuple<int, int, int> position_of(Face face, int col, int row)
{
    const auto cell = net_cell(face, col, row, kSize);
    return {cell.x, cell.y, cell.z};
}

Rect test_rect()
{
    return layout(1024, 1024).net;
}

float face_side()
{
    return test_rect().width / static_cast<float>(kNetColumns);
}

float cell_side()
{
    return face_side() / static_cast<float>(kSize);
}

/** A cube whose faces are all mixed, so a cell moving shows up as a color. */
CubeState mixed_state()
{
    CubeState state(kSize);
    state.apply(rubiks::cube::moves::R(kSize));
    state.apply(rubiks::cube::moves::U(kSize));
    state.apply(rubiks::cube::moves::F(kSize));
    return state;
}

/** One drawn cell, reduced to where it sits on the net's grid. */
struct GridCell {
    long column;
    long row;
    float x;
    float y;
    Color color;
};

/** Every quad of a scene, placed on the grid and put in a fixed order. */
std::vector<GridCell> grid_of(const RenderScene& scene)
{
    const Rect rect = test_rect();
    const float cell = cell_side();

    std::vector<GridCell> cells;
    cells.reserve(scene.faces.size());

    for (const auto& face : scene.faces) {
        float x = 0.0f;
        float y = 0.0f;
        for (const auto& point : face.points) {
            x += point.x;
            y += point.y;
        }
        x *= 0.25f;
        y *= 0.25f;

        cells.push_back(GridCell{
            std::lround((x - rect.x) / cell - 0.5f),
            std::lround((y - rect.y) / cell - 0.5f), x, y, face.color});
    }

    std::sort(cells.begin(), cells.end(),
              [](const GridCell& a, const GridCell& b) {
                  return std::tie(a.row, a.column, a.y, a.x) <
                         std::tie(b.row, b.column, b.y, b.x);
              });
    return cells;
}

/** Two nets showing the same colors in the same places. */
void require_same_net(const RenderScene& actual, const RenderScene& expected)
{
    const auto drawn = grid_of(actual);
    const auto wanted = grid_of(expected);

    REQUIRE(drawn.size() == wanted.size());
    for (std::size_t i = 0; i < drawn.size(); ++i) {
        INFO("cell " << i << " at grid " << drawn[i].column << ","
                     << drawn[i].row);
        REQUIRE(drawn[i].column == wanted[i].column);
        REQUIRE(drawn[i].row == wanted[i].row);
        REQUIRE(drawn[i].x == Approx(wanted[i].x).margin(0.01 * cell_side()));
        REQUIRE(drawn[i].y == Approx(wanted[i].y).margin(0.01 * cell_side()));
        REQUIRE(drawn[i].color == wanted[i].color);
    }
}

/**
 * Checks a turn drawn at both ends against the cube at both ends.
 *
 * The whole point of the rings: however the loop wanders in between, at rest
 * and at a quarter turn the drawing has to be exactly the settled cube, or
 * the commit that follows would be visible as a jump.
 */
void require_ends_match(const rubiks::cube::CubeMove& move)
{
    const CubeState before = mixed_state();
    CubeState after = before;
    after.apply(move);

    const float angle = 90.0f * static_cast<float>(move.quarter_turns);
    const Rect rect = test_rect();

    INFO("axis " << static_cast<int>(move.axis) << " layers " << move.layers
                 << " turns " << move.quarter_turns);

    require_same_net(
        build_net_scene(before, rect,
                        ActiveRotation{move.axis, move.layers, 0.0f}),
        build_net_scene(before, rect));
    require_same_net(
        build_net_scene(before, rect,
                        ActiveRotation{move.axis, move.layers, angle}),
        build_net_scene(after, rect));
}

rubiks::math::Vec2 center_of(const RenderFace& face)
{
    float x = 0.0f;
    float y = 0.0f;
    for (const auto& point : face.points) {
        x += point.x;
        y += point.y;
    }
    return rubiks::math::Vec2{0.25f * x, 0.25f * y};
}

/**
 * Every face in the order the renderer lays them down: the drawing at rest,
 * then whatever a turn is holding off it, lowest storey first.
 */
std::vector<RenderFace> drawn_faces(const RenderScene& scene)
{
    std::vector<RenderFace> faces = scene.faces;
    for (const auto& group : scene.groups) {
        faces.insert(faces.end(), group.faces.begin(), group.faces.end());
    }
    return faces;
}

/**
 * The sticker on top at a point, within a fraction of a cell of it.
 *
 * From the back of the list forwards, so what comes back is the one that would
 * actually be seen there rather than whatever happens to lie underneath.
 */
std::optional<RenderFace> quad_at(const RenderScene& scene, float x, float y)
{
    const float limit = 0.15f * cell_side();

    const auto faces = drawn_faces(scene);
    for (auto face = faces.rbegin(); face != faces.rend(); ++face) {
        const auto center = center_of(*face);
        if (std::abs(center.x - x) < limit && std::abs(center.y - y) < limit) {
            return *face;
        }
    }
    return std::nullopt;
}

/** The sticker's color, leaving the alpha to the test that pins it whole. */
bool same_color(const Color& drawn, FaceColor face_color)
{
    const Color wanted = to_color(face_color);
    return drawn.r == wanted.r && drawn.g == wanted.g && drawn.b == wanted.b;
}

/** The stickers a layer turn carries to another face. */
std::vector<rubiks::cube::SurfaceSticker> band_of(rubiks::cube::Axis axis,
                                                  int layer)
{
    return rubiks::cube::ring_slots(axis, layer, kSize);
}

}  // namespace

TEST_CASE("every net cell reads a sticker that is actually on the surface")
{
    for (const auto face : net_faces()) {
        for (int row = 0; row < kSize; ++row) {
            for (int col = 0; col < kSize; ++col) {
                const auto cell = net_cell(face, col, row, kSize);

                REQUIRE(cell.face == face);
                REQUIRE(cell.x >= 0);
                REQUIRE(cell.x <= kLast);
                REQUIRE(cell.y >= 0);
                REQUIRE(cell.y <= kLast);
                REQUIRE(cell.z >= 0);
                REQUIRE(cell.z <= kLast);

                // The cubie has to sit in the outer layer along the axis of
                // the face it is showing, or that sticker is buried inside.
                switch (face) {
                    case Face::Right:
                        REQUIRE(cell.x == kLast);
                        break;
                    case Face::Left:
                        REQUIRE(cell.x == 0);
                        break;
                    case Face::Up:
                        REQUIRE(cell.y == kLast);
                        break;
                    case Face::Down:
                        REQUIRE(cell.y == 0);
                        break;
                    case Face::Front:
                        REQUIRE(cell.z == kLast);
                        break;
                    case Face::Back:
                        REQUIRE(cell.z == 0);
                        break;
                }
            }
        }
    }
}

TEST_CASE("the net covers each surface sticker exactly once")
{
    std::set<std::tuple<int, int, int, int>> seen;

    for (const auto face : net_faces()) {
        for (int row = 0; row < kSize; ++row) {
            for (int col = 0; col < kSize; ++col) {
                const auto cell = net_cell(face, col, row, kSize);
                seen.insert({cell.x, cell.y, cell.z,
                             static_cast<int>(cell.face)});
            }
        }
    }

    // 54 cells onto 54 distinct cubie faces: no sticker is drawn twice and
    // none is missing.
    REQUIRE(seen.size() == 54);
}

TEST_CASE("net cells that meet at a fold share a cubie edge")
{
    // Where two faces touch in the unfolded drawing they must be the two
    // faces of one physical cubie, or the net is a scrambled picture of a
    // solved cube.
    for (int i = 0; i < kSize; ++i) {
        // Front's right column against Right's left column.
        REQUIRE(position_of(Face::Front, kLast, i) ==
                position_of(Face::Right, 0, i));
        // Left's right column against Front's left column.
        REQUIRE(position_of(Face::Left, kLast, i) ==
                position_of(Face::Front, 0, i));
        // Right's right column against Back's left column.
        REQUIRE(position_of(Face::Right, kLast, i) ==
                position_of(Face::Back, 0, i));
        // Back wraps around to Left.
        REQUIRE(position_of(Face::Back, kLast, i) ==
                position_of(Face::Left, 0, i));
        // Up's bottom row against Front's top row.
        REQUIRE(position_of(Face::Up, i, kLast) ==
                position_of(Face::Front, i, 0));
        // Front's bottom row against Down's top row.
        REQUIRE(position_of(Face::Front, i, kLast) ==
                position_of(Face::Down, i, 0));
    }
}

TEST_CASE("the six faces form a cross with Up on top and Down below")
{
    REQUIRE(net_block(Face::Up).column == 1);
    REQUIRE(net_block(Face::Up).row == 0);
    REQUIRE(net_block(Face::Down).column == 1);
    REQUIRE(net_block(Face::Down).row == 2);

    // The four side faces sit in one row, left to right.
    REQUIRE(net_block(Face::Left).row == 1);
    REQUIRE(net_block(Face::Front).row == 1);
    REQUIRE(net_block(Face::Right).row == 1);
    REQUIRE(net_block(Face::Back).row == 1);
    REQUIRE(net_block(Face::Left).column == 0);
    REQUIRE(net_block(Face::Front).column == 1);
    REQUIRE(net_block(Face::Right).column == 2);
    REQUIRE(net_block(Face::Back).column == 3);
}

TEST_CASE("a solved net shows nine stickers of every color")
{
    const RenderScene scene = build_net_scene(CubeState(kSize), test_rect());

    REQUIRE(scene.faces.size() == 54);
    for (const auto color : {FaceColor::Red, FaceColor::Orange,
                             FaceColor::White, FaceColor::Yellow,
                             FaceColor::Green, FaceColor::Blue}) {
        REQUIRE(count_color(scene, to_color(color)) == 9);
    }
}

TEST_CASE("net stickers are squares inside the net rectangle")
{
    const Rect rect = test_rect();
    const RenderScene scene = build_net_scene(CubeState(kSize), rect);

    const float cell = rect.width / static_cast<float>(kNetColumns) /
                       static_cast<float>(kSize);
    const float side = cell * kNetStickerScale;

    for (const auto& face : scene.faces) {
        REQUIRE(face.points[1].x - face.points[0].x == Approx(side));
        REQUIRE(face.points[3].y - face.points[0].y == Approx(side));

        for (const auto& point : face.points) {
            REQUIRE(point.x >= rect.x);
            REQUIRE(point.x <= rect.x + rect.width);
            REQUIRE(point.y >= rect.y);
            REQUIRE(point.y <= rect.y + rect.height);
        }
    }

    // A gap remains between neighbouring stickers, so seams show through.
    REQUIRE(side < cell);
}

TEST_CASE("a move is visible in the net")
{
    CubeState state(kSize);
    state.apply(rubiks::cube::moves::R(kSize));

    const RenderScene scene = build_net_scene(state, test_rect());

    // The net always shows all 54 stickers, so the colors stay in balance
    // however the cube is turned.
    REQUIRE(scene.faces.size() == 54);
    for (const auto color : {FaceColor::Red, FaceColor::Orange,
                             FaceColor::White, FaceColor::Yellow,
                             FaceColor::Green, FaceColor::Blue}) {
        REQUIRE(count_color(scene, to_color(color)) == 9);
    }

    // R lifts the front face's right column onto the top face, so the up
    // face's right column turns green.
    for (int row = 0; row < kSize; ++row) {
        const auto cell = net_cell(Face::Up, kLast, row, kSize);
        REQUIRE(state.at(cell.x, cell.y, cell.z).sticker(cell.face) ==
                FaceColor::Green);
    }
}

TEST_CASE("the net scales with its rectangle")
{
    const RenderScene small =
        build_net_scene(CubeState(kSize), Rect{0.0f, 0.0f, 40.0f, 30.0f});
    const RenderScene large =
        build_net_scene(CubeState(kSize), Rect{0.0f, 0.0f, 80.0f, 60.0f});

    REQUIRE(small.faces.size() == large.faces.size());
    REQUIRE(large.faces[0].points[1].x - large.faces[0].points[0].x ==
            Approx(2.0f * (small.faces[0].points[1].x -
                           small.faces[0].points[0].x)));
}

TEST_CASE("a net without a turn is drawn by the resting path")
{
    const CubeState state = mixed_state();
    require_same_net(build_net_scene(state, test_rect(), std::nullopt),
                     build_net_scene(state, test_rect()));
}

TEST_CASE("every turn's drawing meets the cube at both ends")
{
    using rubiks::cube::Axis;
    using rubiks::cube::CubeMove;
    using rubiks::cube::layer;
    using rubiks::cube::layers_through;

    SECTION("the horizontal rings")
    {
        require_ends_match(CubeMove{Axis::Y, layer(2), 1});
        require_ends_match(CubeMove{Axis::Y, layer(2), -1});
        require_ends_match(CubeMove{Axis::Y, layer(0), 1});
        require_ends_match(CubeMove{Axis::Y, layer(0), -1});
        require_ends_match(CubeMove{Axis::Y, layer(1), 1});
        require_ends_match(CubeMove{Axis::Y, layers_through(0, 2), 1});
    }

    SECTION("the front ring")
    {
        require_ends_match(CubeMove{Axis::Z, layer(2), 1});
        require_ends_match(CubeMove{Axis::Z, layer(2), -1});
    }

    SECTION("the rings the cross cuts")
    {
        require_ends_match(CubeMove{Axis::X, layer(2), 1});
        require_ends_match(CubeMove{Axis::X, layer(2), -1});
        require_ends_match(CubeMove{Axis::X, layer(0), 1});
        require_ends_match(CubeMove{Axis::X, layer(1), 1});
        require_ends_match(CubeMove{Axis::Z, layer(0), 1});
        require_ends_match(CubeMove{Axis::Z, layer(1), -1});
        require_ends_match(CubeMove{Axis::Z, layers_through(1, 2), 1});
    }
}

TEST_CASE("a turn carried past a quarter keeps going round its loop")
{
    using rubiks::cube::Axis;
    using rubiks::cube::layer;

    // The net used to wait at 90 degrees: it only ever drew one quarter, so a
    // drag carried further would have settled on a turn the drawing never
    // showed. The bands go round their loops as far as they are taken now, so
    // a half turn is drawn as a half turn, and a whole one comes back to
    // exactly where it started.
    require_ends_match(rubiks::cube::CubeMove{Axis::Y, layer(2), 2});
    require_ends_match(rubiks::cube::CubeMove{Axis::X, layer(0), -2});
    require_ends_match(rubiks::cube::CubeMove{Axis::Z, layer(2), 3});
    require_ends_match(rubiks::cube::CubeMove{Axis::X, layer(1), 4});
}

TEST_CASE("every band is one closed ring through its twelve resting slots")
{
    using rubiks::cube::Axis;

    const Rect rect = test_rect();

    for (const auto axis : {Axis::X, Axis::Y, Axis::Z}) {
        for (int index = 0; index < kSize; ++index) {
            const auto ring = net_ring(axis, index, rect, kSize);
            INFO("axis " << static_cast<int>(axis) << " layer " << index);
            REQUIRE(ring.slot_count() == 4 * kSize);

            for (std::size_t slot = 0; slot < ring.slot_count(); ++slot) {
                const auto sticker = ring.sticker_at(slot);
                const auto placed = net_position(sticker, kSize);
                REQUIRE(placed);

                // The curve is fitted to the drawing, not the other way round:
                // at rest every slot is exactly the middle of its cell.
                const auto centre = net_cell_center(placed->face, placed->col,
                                                    placed->row, rect, kSize);
                const auto point = ring.at(static_cast<float>(slot));
                REQUIRE(point.position.x == Approx(centre.x).margin(0.001));
                REQUIRE(point.position.y == Approx(centre.y).margin(0.001));
                REQUIRE(ring.slot_of(sticker) == slot);
            }
        }
    }
}

TEST_CASE("a ring runs along a drawn direction wherever a cell rests")
{
    using rubiks::cube::Axis;

    const Rect rect = test_rect();

    for (const auto axis : {Axis::X, Axis::Y, Axis::Z}) {
        for (int index = 0; index < kSize; ++index) {
            const auto ring = net_ring(axis, index, rect, kSize);

            for (std::size_t slot = 0; slot < ring.slot_count(); ++slot) {
                const auto here = ring.at(static_cast<float>(slot)).tangent;
                INFO("axis " << static_cast<int>(axis) << " layer " << index
                             << " slot " << slot);

                // Axis-aligned at every slot, so a bend always falls between
                // two of them and never on one.
                REQUIRE(std::min(std::abs(here.x), std::abs(here.y)) < 1e-4f);

                // Which is what makes the far end of a quarter turn a whole
                // number of right angles from this one.
                const auto landed =
                    ring.at(static_cast<float>(slot + kSize)).tangent;
                const float turn = std::atan2(landed.y, landed.x) -
                                   std::atan2(here.y, here.x);
                const float quarters = turn / (0.5f * 3.14159265f);
                REQUIRE(quarters == Approx(std::lround(quarters)).margin(1e-3));
            }
        }
    }
}

/** The sharpest turn between neighbouring samples of a whole loop. */
float worst_turn(const SlotRing& ring, int steps)
{
    const auto span = static_cast<float>(ring.slot_count());

    float worst = 0.0f;
    auto previous = ring.at(0.0f).tangent;
    for (int step = 1; step <= steps; ++step) {
        const auto here =
            ring.at(span * static_cast<float>(step) / static_cast<float>(steps))
                .tangent;
        const float dot =
            std::min(1.0f, previous.x * here.x + previous.y * here.y);
        worst = std::max(worst, std::acos(dot));
        previous = here;
    }
    return worst;
}

TEST_CASE("a ring's direction never jumps")
{
    using rubiks::cube::Axis;

    const Rect rect = test_rect();

    // A cell's tilt follows the tangent, so a tangent that jumped at a join
    // would spin the cell on the spot there. Sampling alone cannot tell a jump
    // from a tight bend, so this looks at what happens when the sampling is
    // refined: across a bend the step shrinks with it, across a jump it does
    // not shrink at all.
    for (const auto axis : {Axis::X, Axis::Y, Axis::Z}) {
        for (int index = 0; index < kSize; ++index) {
            const auto ring = net_ring(axis, index, rect, kSize);
            const float coarse = worst_turn(ring, 4000);
            const float fine = worst_turn(ring, 16000);

            INFO("axis " << static_cast<int>(axis) << " layer " << index
                         << " coarse " << coarse << " fine " << fine);
            REQUIRE(fine < 0.5f * coarse);
        }
    }
}

TEST_CASE("a turning band rides its ring one face's worth at a time")
{
    using rubiks::cube::Axis;
    using rubiks::cube::layer;

    const Rect rect = test_rect();
    const float cell = cell_side();
    const CubeState state = mixed_state();

    // A band travels as three-cell pieces, not as twelve loose stickers: the
    // ring is read at each piece's middle, and the rest of the piece is carried
    // along rigidly beside it. So a piece stays a straight line one cell apart
    // however the loop bends underneath it, which is what makes a turn
    // followable, and it still lands exactly on its slots at the quarter.
    //
    // Z is not among the axes: the cross draws that turn where it happens, so
    // it is drawn as the turn itself rather than as travel along a loop.
    for (const auto axis : {Axis::X, Axis::Y}) {
        for (int index = 0; index < kSize; ++index) {
            const auto ring = net_ring(axis, index, rect, kSize);

            for (const float angle : {15.0f, 30.0f, 45.0f, 70.0f}) {
                const RenderScene scene = build_net_scene(
                    state, rect, ActiveRotation{axis, layer(index), angle});
                const float advance =
                    static_cast<float>(kSize) * angle / 90.0f;

                for (std::size_t first = 0; first < ring.slot_count();
                     first += kSize) {
                    const float middle = static_cast<float>(first) +
                                         0.5f * static_cast<float>(kLast);
                    const auto carried = ring.at(middle + advance);

                    std::vector<rubiks::math::Vec2> drawn_at;
                    for (std::size_t step = 0; step < kSize; ++step) {
                        const std::size_t slot = first + step;
                        const auto sticker = ring.sticker_at(slot);

                        // Where the piece holds this cell: along the loop's
                        // direction from its middle, by as many cells as the
                        // cell rests from it.
                        const float away =
                            (static_cast<float>(slot) - middle) * cell;
                        const float x =
                            carried.position.x + away * carried.tangent.x;
                        const float y =
                            carried.position.y + away * carried.tangent.y;

                        INFO("axis " << static_cast<int>(axis) << " layer "
                                     << index << " angle " << angle << " slot "
                                     << slot);
                        const auto quad = quad_at(scene, x, y);
                        REQUIRE(quad);
                        REQUIRE(same_color(
                            quad->color,
                            state.at(sticker.x, sticker.y, sticker.z)
                                .sticker(sticker.face)));
                        drawn_at.push_back(center_of(*quad));
                    }

                    // Rigid, read off what was drawn rather than off the ring:
                    // the piece's cells are a cell apart and in one line, all
                    // the way round a bend where the slots they would ride
                    // separately are not in line at all.
                    for (std::size_t step = 1; step < drawn_at.size(); ++step) {
                        const float dx =
                            drawn_at[step].x - drawn_at[step - 1].x;
                        const float dy =
                            drawn_at[step].y - drawn_at[step - 1].y;

                        INFO("axis " << static_cast<int>(axis) << " layer "
                                     << index << " angle " << angle
                                     << " piece at " << first << " step "
                                     << step);
                        REQUIRE(std::sqrt(dx * dx + dy * dy) ==
                                Approx(cell).margin(0.01 * cell));

                        const float first_dx = drawn_at[1].x - drawn_at[0].x;
                        const float first_dy = drawn_at[1].y - drawn_at[0].y;
                        REQUIRE(dx * first_dy - dy * first_dx ==
                                Approx(0.0f).margin(0.01 * cell * cell));
                    }
                }
            }
        }
    }
}

TEST_CASE("a piece on the move is drawn over the cells it passes")
{
    using rubiks::cube::Axis;
    using rubiks::cube::layer;

    const Rect rect = test_rect();
    const CubeState state = mixed_state();

    // The front layer, whose band and face together are every cell the turn
    // moves: nine of the face and twelve of the band.
    constexpr std::size_t kMoving = 21;

    const RenderScene scene = build_net_scene(
        state, rect, ActiveRotation{Axis::Z, layer(kLast), 45.0f, kHeldOpen});

    // Every resting cell is an ordinary face, and every moving one is held
    // off the drawing in a group -- one a storey -- which the renderer draws
    // after all of the faces, so nothing a piece crosses can be drawn back
    // over it. The face that spins in place is the lower storey.
    REQUIRE(scene.faces.size() == 6 * kSize * kSize - kMoving);
    REQUIRE(scene.groups.size() == 2);
    REQUIRE(scene.groups[0].faces.size() == kSize * kSize);
    REQUIRE(scene.groups[1].faces.size() == 4 * kSize);

    // No shadow is a face any more: the shadow is the group's, and nothing
    // drawn is anything but a sticker at full strength.
    for (const auto& face : drawn_faces(scene)) {
        REQUIRE(face.color.a == 255);
        REQUIRE_FALSE((face.color.r == 0 && face.color.g == 0 &&
                       face.color.b == 0));
    }

    // The band is held a storey above the face, so its shadow falls twice as
    // far, and each shadow's edge is as soft as its distance says.
    const LiftShadow& under_face = scene.groups[0].shadow;
    const LiftShadow& under_band = scene.groups[1].shadow;
    REQUIRE(under_face.distance == Approx(kNetLiftCells * cell_side() *
                                          kNetFaceStorey * kNetLiftDiagonal));
    REQUIRE(under_band.distance == Approx(2.0f * under_face.distance));
    REQUIRE(under_face.alpha == static_cast<std::uint8_t>(kNetShadowAlpha));
    REQUIRE(under_band.alpha == under_face.alpha);
    REQUIRE(under_face.sigma ==
            Approx(kNetShadowSigmaShare * under_face.distance));
    REQUIRE(under_band.sigma ==
            Approx(kNetShadowSigmaShare * under_band.distance));

    // And a piece is drawn a little larger than it rests -- the size and the
    // shadow together are what say it is off the page. A piece of the band
    // is a storey higher than the face and so that much larger again.
    const auto lifted = scene.groups[1].faces.back();
    const float side = std::sqrt(
        (lifted.points[1].x - lifted.points[0].x) *
            (lifted.points[1].x - lifted.points[0].x) +
        (lifted.points[1].y - lifted.points[0].y) *
            (lifted.points[1].y - lifted.points[0].y));
    REQUIRE(side == Approx((1.0f + kNetLiftScale * kNetBandStorey) *
                           kNetStickerScale * cell_side())
                        .epsilon(0.01));

    // The storeys are in that order: on a solved cube the face turning in
    // place is nine green, and the band around it is the four other colors.
    const RenderScene solved = build_net_scene(
        CubeState(kSize), rect,
        ActiveRotation{Axis::Z, layer(kLast), 45.0f, kHeldOpen});
    REQUIRE(solved.groups.size() == 2);
    for (const auto& face : solved.groups[0].faces) {
        REQUIRE(same_color(face.color, FaceColor::Green));
    }
    for (const auto& face : solved.groups[1].faces) {
        REQUIRE_FALSE(same_color(face.color, FaceColor::Green));
    }

    // And gone at both ends of the turn, so the settled net is untouched: no
    // group at all, every cell an ordinary face.
    for (const float angle : {0.0f, 90.0f}) {
        const RenderScene ends = build_net_scene(
            state, rect, ActiveRotation{Axis::Z, layer(kLast), angle, 0.0f});

        INFO("angle " << angle);
        REQUIRE(ends.groups.empty());
        REQUIRE(ends.faces.size() == 6 * kSize * kSize);
    }
}

TEST_CASE("the lift follows the opening, and never by less than a pixel a storey")
{
    using rubiks::cube::Axis;
    using rubiks::cube::layer;

    const CubeState state = mixed_state();
    const auto turn = [](float opening) {
        return ActiveRotation{Axis::Z, layer(kLast), 45.0f, opening};
    };

    // Half open is half the distance, half the darkness, half the growth.
    const RenderScene full = build_net_scene(state, test_rect(), turn(1.0f));
    const RenderScene half = build_net_scene(state, test_rect(), turn(0.5f));
    REQUIRE(half.groups.size() == 2);
    for (std::size_t storey = 0; storey < 2; ++storey) {
        INFO("storey " << storey);
        REQUIRE(half.groups[storey].shadow.distance ==
                Approx(0.5f * full.groups[storey].shadow.distance));
        REQUIRE(half.groups[storey].shadow.alpha ==
                static_cast<std::uint8_t>(std::lround(0.5f * kNetShadowAlpha)));
        REQUIRE(half.groups[storey].shadow.sigma ==
                Approx(kNetShadowSigmaShare *
                       half.groups[storey].shadow.distance));
    }

    // The renderer moves a shadow by whole pixels. Where a cell is a pixel
    // across, a distance in cells alone would round to nothing for both
    // storeys and the band would fall no further than the face; the floor
    // keeps them a pixel apart, so the higher storey is still the further.
    const Rect tiny{0.0f, 0.0f, 12.0f, 9.0f};
    REQUIRE(net_cell_side(tiny, kSize) == Approx(1.0f));
    const RenderScene small = build_net_scene(state, tiny, turn(1.0f));
    REQUIRE(small.groups.size() == 2);
    REQUIRE(small.groups[0].shadow.distance ==
            Approx(kNetLiftMinPixels * kNetFaceStorey * kNetLiftDiagonal));
    REQUIRE(small.groups[1].shadow.distance ==
            Approx(kNetLiftMinPixels * kNetBandStorey * kNetLiftDiagonal));

    // What the renderer will do with them: the push along each axis, cut to
    // whole pixels, is one for the face and two for the band.
    const auto pixels = [](const LiftShadow& lift) {
        return static_cast<int>(lift.distance / kNetLiftDiagonal);
    };
    REQUIRE(pixels(small.groups[0].shadow) == 1);
    REQUIRE(pixels(small.groups[1].shadow) == 2);

    // At the usual size the floor is far below the cell distance and changes
    // nothing.
    REQUIRE(full.groups[0].shadow.distance >
            kNetLiftMinPixels * kNetLiftDiagonal * 2.0f);
}

TEST_CASE("the front ring stays inside the net rectangle")
{
    using rubiks::cube::Axis;
    using rubiks::cube::layer;

    // The front layer's band is the border of its own block, so its loop has
    // the cross's empty corners to bend through and never leaves the drawing
    // at all -- the one ring of which that is true, and nothing else the turn
    // does moves a cell out of the rectangle either.
    const Rect rect = test_rect();
    const float room = 0.0f;

    for (int step = 0; step <= 18; ++step) {
        const auto angle = static_cast<float>(step) * 5.0f;
        const RenderScene scene = build_net_scene(
            CubeState(kSize), rect,
            ActiveRotation{Axis::Z, layer(2), angle, kHeldOpen});

        INFO("angle " << angle);
        for (const auto& face : drawn_faces(scene)) {
            for (const auto& point : face.points) {
                REQUIRE(point.x >= rect.x - room);
                REQUIRE(point.x <= rect.x + rect.width + room);
                REQUIRE(point.y >= rect.y - room);
                REQUIRE(point.y <= rect.y + rect.height + room);
            }
        }
    }
}

TEST_CASE("a turning cell keeps the color of its sticker, whole")
{
    using rubiks::cube::Axis;
    using rubiks::cube::layer;

    const Rect rect = test_rect();
    const CubeState state = mixed_state();

    // Nothing dims a sticker. A ring carries a band outside the drawing where
    // the cross cuts it, and the drawing used to fade a cell out by how far it
    // had gone -- which reads as the cubie changing color rather than as it
    // leaving. Whatever is drawn is drawn in the color the cube says, at full
    // strength, and the only quads that are not are the shadows.
    for (const auto axis : {Axis::X, Axis::Y, Axis::Z}) {
        for (int index = 0; index < kSize; ++index) {
            for (int step = 0; step <= 18; ++step) {
                const auto angle = static_cast<float>(step) * 5.0f;
                const RenderScene scene = build_net_scene(
                    state, rect, ActiveRotation{axis, layer(index), angle});

                INFO("axis " << static_cast<int>(axis) << " layer " << index
                             << " angle " << angle);

                std::size_t stickers = 0;
                for (const auto& face : drawn_faces(scene)) {
                    ++stickers;
                    REQUIRE(face.color.a == 255);

                    bool known = false;
                    for (const auto sticker :
                         {FaceColor::Red, FaceColor::Orange, FaceColor::White,
                          FaceColor::Yellow, FaceColor::Green,
                          FaceColor::Blue}) {
                        if (face.color == to_color(sticker)) known = true;
                    }
                    REQUIRE(known);
                }

                // Every one of them, every time: none is dropped either.
                REQUIRE(stickers == 6 * kSize * kSize);
            }
        }
    }
}

TEST_CASE("a turn leaves the drawing it is passing over exactly where it is")
{
    using rubiks::cube::Axis;
    using rubiks::cube::layer;

    const Rect rect = test_rect();
    const CubeState state = mixed_state();

    // A turn moves what it carries and nothing else. Opening a margin around
    // one was tried at some length -- pushing the cells beside it aside so a
    // piece had room to pass -- and every arrangement of it cost more than it
    // paid: the cross came apart into strips, or the net slid sideways under
    // the finger, or the ground moved under the very piece being made room
    // for. A piece passing over the drawing, drawn last and over its shadow,
    // reads better than any of it.
    //
    // Resting cells are the ones drawn first; the pieces the turn carries and
    // their shadows are held back to the end.
    for (const auto axis : {Axis::X, Axis::Y, Axis::Z}) {
        for (int index = 0; index < kSize; ++index) {
            const std::size_t carried =
                4 * kSize +
                (index == 0 || index == kLast ? kSize * kSize : 0);
            const std::size_t resting = 6 * kSize * kSize - carried;

            for (int step = 0; step <= 18; ++step) {
                const auto angle = static_cast<float>(step) * 5.0f;
                const RenderScene scene = build_net_scene(
                    state, rect,
                    ActiveRotation{axis, layer(index), angle, kHeldOpen});

                REQUIRE(scene.faces.size() >= resting);

                // Every one of them is still on a cell of the settled net.
                // Which cell does not matter here -- the drawing is the same
                // drawing -- so the nearest is the one to measure against.
                const RenderScene settled = build_net_scene(state, rect);

                float strayed = 0.0f;
                for (std::size_t cell = 0; cell < resting; ++cell) {
                    const auto turning = center_of(scene.faces[cell]);

                    float nearest = std::numeric_limits<float>::max();
                    for (const auto& face : settled.faces) {
                        const auto at_rest = center_of(face);
                        nearest = std::min(
                            nearest, std::hypot(turning.x - at_rest.x,
                                                turning.y - at_rest.y));
                    }
                    strayed = std::max(strayed, nearest);
                }

                INFO("axis " << static_cast<int>(axis) << " layer " << index
                             << " angle " << angle);
                REQUIRE(strayed == Approx(0.0f).margin(1e-3));
            }
        }
    }
}

TEST_CASE("a Z turn takes the front block and its band round together")
{
    using rubiks::cube::Axis;
    using rubiks::cube::layer;

    const Rect rect = test_rect();
    const float cell = cell_side();
    const CubeState state = mixed_state();

    // The one turn the cross draws in place. Everything it moves goes round
    // the front block's middle at once -- the face and the band it is joined
    // to -- so nothing comes away from anything else along the way.
    //
    // Held off the page as well as settled: what a turn is carrying is drawn
    // larger while it is on the move, and it still has to go round the one
    // circle.
    const float pivot_x = rect.x + 4.5f * cell;
    const float pivot_y = rect.y + 4.5f * cell;

    for (const float open : {0.0f, kHeldOpen}) {
        for (int index = 0; index < kSize; ++index) {
            for (const float angle : {30.0f, 45.0f, 70.0f}) {
                const auto turning =
                    ActiveRotation{Axis::Z, layer(index), angle, open};
                const RenderScene scene =
                    build_net_scene(state, rect, turning);
                const float radians = angle * 3.14159265f / 180.0f;

                for (const auto& sticker : band_of(Axis::Z, index)) {
                    const auto placed = net_position(sticker, kSize);
                    REQUIRE(placed);
                    const auto rest = net_cell_center(
                        placed->face, placed->col, placed->row, rect, kSize);

                    const float dx = rest.x - pivot_x;
                    const float dy = rest.y - pivot_y;
                    const float x = pivot_x + dx * std::cos(radians) -
                                    dy * std::sin(radians);
                    const float y = pivot_y + dx * std::sin(radians) +
                                    dy * std::cos(radians);

                    INFO("layer " << index << " angle " << angle << " open "
                                  << open);
                    const auto drawn = quad_at(scene, x, y);
                    REQUIRE(drawn);
                    REQUIRE(same_color(
                        drawn->color, state.at(sticker.x, sticker.y, sticker.z)
                                          .sticker(sticker.face)));
                }
            }
        }
    }

    // The middle of the front face is the pivot, so it does not move at all.
    const RenderScene scene = build_net_scene(
        state, rect, ActiveRotation{Axis::Z, layer(2), 45.0f});
    REQUIRE(quad_at(scene, pivot_x, pivot_y));
}

TEST_CASE("view modes place only the regions they render")
{
    const CanvasLayout both = layout(1000, 800, ViewMode::Both);
    REQUIRE(both.cube.width > 0.0f);
    REQUIRE(both.net.width > 0.0f);
    REQUIRE(both.cube.width == Approx(kCubeRegionSide * 800.0f));

    const CanvasLayout cube = layout(1000, 800, ViewMode::Cube3D);
    REQUIRE(cube.cube.width == Approx(kCubeOnlyRegionSide * 800.0f));
    REQUIRE(cube.cube.width == Approx(cube.cube.height));
    REQUIRE(cube.cube.x == Approx((1000.0f - cube.cube.width) * 0.5f));
    REQUIRE(cube.cube.y == Approx((800.0f - cube.cube.height) * 0.5f));
    REQUIRE(cube.net.width == 0.0f);
    REQUIRE(cube.net.height == 0.0f);

    const CanvasLayout net = layout(1000, 800, ViewMode::Flat);
    REQUIRE(net.cube.width == 0.0f);
    REQUIRE(net.cube.height == 0.0f);
    REQUIRE(net.net.width > 0.0f);
    REQUIRE(net.net.height > 0.0f);
    REQUIRE(net.net.width / net.net.height ==
            Approx(static_cast<float>(kNetColumns) /
                   static_cast<float>(kNetRows)));
    REQUIRE(net.net.x == Approx((1000.0f - net.net.width) * 0.5f));
    REQUIRE(net.net.y == Approx((800.0f - net.net.height) * 0.5f));
}

TEST_CASE("a guide line takes the color of the axis it turns about")
{
    using rubiks::cube::Axis;

    const Rect rect = test_rect();
    // The Z guide is the pressed cell's own circle, so it needs the cell.
    const auto pressed = net_cell(Face::Up, 1, 2, kSize);
    const RenderScene scene =
        build_net_guides({NetGuide{Axis::X, 1, pressed},
                          NetGuide{Axis::Y, 2, pressed},
                          NetGuide{Axis::Z, 2, pressed}},
                         rect, kSize);

    REQUIRE(scene.faces.empty());
    REQUIRE(scene.strokes.size() == 3);
    REQUIRE(scene.strokes[0].color == guide_color(Axis::X));
    REQUIRE(scene.strokes[1].color == guide_color(Axis::Y));
    REQUIRE(scene.strokes[2].color == guide_color(Axis::Z));

    // Three colors that can actually be told apart, and none of them a
    // sticker's.
    for (const auto axis : {Axis::X, Axis::Y, Axis::Z}) {
        for (const auto sticker : {FaceColor::Red, FaceColor::Orange,
                                   FaceColor::White, FaceColor::Yellow,
                                   FaceColor::Green, FaceColor::Blue}) {
            REQUIRE_FALSE(same_color(guide_color(axis), sticker));
        }
    }

    // Every one of them is a closed path made of curves, not a chain of
    // straight bits.
    for (const auto& stroke : scene.strokes) {
        REQUIRE(stroke.closed);
        REQUIRE(stroke.width > 0.0f);
        REQUIRE(stroke.segments.size() >= 4);
    }
}

TEST_CASE("a turning cell never strays far outside the drawing")
{
    using rubiks::cube::Axis;
    using rubiks::cube::layer;

    const Rect rect = test_rect();
    const float cell = cell_side();

    // A loop leaves the net by `kNetBreakReachCells`, and a piece rounding a
    // cut hangs out by that, plus its own half-diagonal, plus how wide of its
    // middle the ends of a rigid piece swing through the bend. Under two cells
    // in all, whichever ring it is.
    //
    // Which is more than the margin between the two views in Both mode, so a
    // piece crossing a cut does reach a little way into the cube's half of the
    // canvas. What this pins is that it is a little way and not a wander.
    const float margin = 2.0f * cell;

    for (const auto axis : {Axis::X, Axis::Y, Axis::Z}) {
        for (int index = 0; index < kSize; ++index) {
            for (int step = 0; step <= 18; ++step) {
                const auto angle = static_cast<float>(step) * 5.0f;
                const RenderScene scene =
                    build_net_scene(CubeState(kSize), rect,
                                    ActiveRotation{axis, layer(index), angle});

                INFO("axis " << static_cast<int>(axis) << " layer " << index
                             << " angle " << angle);
                for (const auto& face : scene.faces) {
                    for (const auto& point : face.points) {
                        REQUIRE(point.x >= rect.x - margin);
                        REQUIRE(point.x <= rect.x + rect.width + margin);
                        REQUIRE(point.y >= rect.y - margin);
                        REQUIRE(point.y <= rect.y + rect.height + margin);
                    }
                }
            }
        }
    }
}

TEST_CASE("a ring runs dead straight between slots that share a face")
{
    using rubiks::cube::Axis;

    const Rect rect = test_rect();

    // What makes a ring read as one clean shape: the slots sit on its straight
    // parts and only the corners between faces are curved. It is also what
    // stops a cell rocking -- along a run there is nothing for its tilt to
    // follow but the run itself.
    const auto require_straight = [&](const SlotRing& ring) {
        for (std::size_t slot = 0; slot < ring.slot_count(); ++slot) {
            const auto next = (slot + 1) % ring.slot_count();
            if (ring.sticker_at(slot).face != ring.sticker_at(next).face) {
                continue;
            }

            const auto from = ring.at(static_cast<float>(slot)).position;
            const auto to = ring.at(static_cast<float>(slot) + 1.0f).position;

            for (int step = 1; step < 20; ++step) {
                const float part = static_cast<float>(step) / 20.0f;
                const auto here =
                    ring.at(static_cast<float>(slot) + part).position;

                INFO("slot " << slot << " at " << part);
                REQUIRE(here.x == Approx(from.x + part * (to.x - from.x))
                                      .margin(0.01 * cell_side()));
                REQUIRE(here.y == Approx(from.y + part * (to.y - from.y))
                                      .margin(0.01 * cell_side()));
            }
        }
    };

    for (const auto axis : {Axis::X, Axis::Y, Axis::Z}) {
        for (int index = 0; index < kSize; ++index) {
            require_straight(net_ring(axis, index, rect, kSize));
        }
    }
}
