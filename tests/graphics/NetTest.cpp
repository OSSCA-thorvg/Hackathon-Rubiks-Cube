#include <cstddef>
#include <set>
#include <tuple>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"
#include "graphics/Layout.hpp"
#include "graphics/NetGeometry.hpp"
#include "graphics/Palette.hpp"

namespace {

using Catch::Approx;
using namespace rubiks::graphics;
using rubiks::cube::CubeState;
using rubiks::cube::Face;
using rubiks::cube::FaceColor;

constexpr int kSize = 3;
constexpr int kLast = kSize - 1;

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
