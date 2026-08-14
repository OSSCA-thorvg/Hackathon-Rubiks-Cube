#include "interaction/NetPicking.hpp"

#include <array>
#include <limits>

#include <catch2/catch_test_macros.hpp>

#include "cube/CubeMove.hpp"
#include "cube/Cubie.hpp"
#include "graphics/Layout.hpp"
#include "graphics/NetGeometry.hpp"
#include "graphics/Rect.hpp"

// Picking a net cell and working out which turn a drag from it means. The
// derivation is checked against a frame per face written out by hand below,
// so a mistake in the mapping the engine derives its own from cannot agree
// with this file by construction.

namespace {

using rubiks::cube::Axis;
using rubiks::cube::Face;
using rubiks::graphics::Rect;
using rubiks::interaction::NetPick;
using rubiks::graphics::net_step_turn;
using rubiks::interaction::pick_net;

constexpr int kSize = 3;
constexpr int kLast = kSize - 1;

/** An integer vector in the cube's centered coordinates. */
struct Vec {
    int x = 0;
    int y = 0;
    int z = 0;
};

constexpr bool operator==(const Vec& a, const Vec& b) noexcept
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

Vec cross(const Vec& a, const Vec& b)
{
    return Vec{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
               a.x * b.y - a.y * b.x};
}

int dot(const Vec& a, const Vec& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec scaled(const Vec& v, int factor)
{
    return Vec{v.x * factor, v.y * factor, v.z * factor};
}

Vec added(const Vec& a, const Vec& b)
{
    return Vec{a.x + b.x, a.y + b.y, a.z + b.z};
}

/** How one unfolded face is oriented on the cube, written out by hand. */
struct FaceFrame {
    Vec col;
    Vec row;
    Vec normal;
};

FaceFrame frame_of(Face face)
{
    switch (face) {
        case Face::Up:
            return {Vec{1, 0, 0}, Vec{0, 0, 1}, Vec{0, 1, 0}};
        case Face::Left:
            return {Vec{0, 0, 1}, Vec{0, -1, 0}, Vec{-1, 0, 0}};
        case Face::Front:
            return {Vec{1, 0, 0}, Vec{0, -1, 0}, Vec{0, 0, 1}};
        case Face::Right:
            return {Vec{0, 0, -1}, Vec{0, -1, 0}, Vec{1, 0, 0}};
        case Face::Back:
            return {Vec{-1, 0, 0}, Vec{0, -1, 0}, Vec{0, 0, -1}};
        case Face::Down:
            break;
    }
    return {Vec{1, 0, 0}, Vec{0, 0, -1}, Vec{0, -1, 0}};
}

/** The pressed sticker's position, measured from the middle of the cube. */
Vec centered(Face face, int col, int row)
{
    const auto cell = rubiks::graphics::net_cell(face, col, row, kSize);
    return Vec{cell.x - 1, cell.y - 1, cell.z - 1};
}

Vec axis_vector(Axis axis, int sign)
{
    switch (axis) {
        case Axis::X:
            return Vec{sign, 0, 0};
        case Axis::Y:
            return Vec{0, sign, 0};
        case Axis::Z:
            break;
    }
    return Vec{0, 0, sign};
}

/** The four directions a drag can take, as steps in the drawn grid. */
constexpr std::array<std::array<int, 2>, 4> kDirections{
    {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

Rect test_rect()
{
    return rubiks::graphics::layout(1024, 1024).net;
}

/** The center of one net cell, in drawing-buffer pixels. */
std::array<float, 2> cell_center(Face face, int col, int row, const Rect& rect)
{
    const float face_side =
        rect.width / static_cast<float>(rubiks::graphics::kNetColumns);
    const float cell = face_side / static_cast<float>(kSize);
    const auto block = rubiks::graphics::net_block(face);

    return {rect.x + static_cast<float>(block.column) * face_side +
                (static_cast<float>(col) + 0.5f) * cell,
            rect.y + static_cast<float>(block.row) * face_side +
                (static_cast<float>(row) + 0.5f) * cell};
}

}  // namespace

TEST_CASE("every net cell is picked at its own center")
{
    const Rect rect = test_rect();

    for (const auto face : rubiks::graphics::net_faces()) {
        for (int row = 0; row < kSize; ++row) {
            for (int col = 0; col < kSize; ++col) {
                const auto point = cell_center(face, col, row, rect);
                const auto pick = pick_net(point[0], point[1], rect, kSize);

                INFO("face " << static_cast<int>(face) << " cell " << col << ","
                             << row);
                REQUIRE(pick);
                REQUIRE(pick->face == face);
                REQUIRE(pick->col == col);
                REQUIRE(pick->row == row);
            }
        }
    }
}

TEST_CASE("the empty corners of the cross are misses")
{
    const Rect rect = test_rect();
    const float face_side =
        rect.width / static_cast<float>(rubiks::graphics::kNetColumns);

    // The four blocks of the 4 x 3 cross that no face occupies, plus the two
    // beside Back that the second row does fill on the middle row only.
    constexpr std::array<std::array<int, 2>, 6> kEmpty{
        {{0, 0}, {2, 0}, {3, 0}, {0, 2}, {2, 2}, {3, 2}}};

    for (const auto& block : kEmpty) {
        const float x =
            rect.x + (static_cast<float>(block[0]) + 0.5f) * face_side;
        const float y =
            rect.y + (static_cast<float>(block[1]) + 0.5f) * face_side;

        INFO("empty block " << block[0] << "," << block[1]);
        REQUIRE_FALSE(pick_net(x, y, rect, kSize));
    }
}

TEST_CASE("points off the net are misses")
{
    const Rect rect = test_rect();
    const float nan = std::numeric_limits<float>::quiet_NaN();

    REQUIRE_FALSE(pick_net(rect.x - 1.0f, rect.y + 1.0f, rect, kSize));
    REQUIRE_FALSE(pick_net(rect.x + 1.0f, rect.y - 1.0f, rect, kSize));
    REQUIRE_FALSE(pick_net(rect.x + rect.width, rect.y + 1.0f, rect, kSize));
    REQUIRE_FALSE(pick_net(rect.x + 1.0f, rect.y + rect.height, rect, kSize));
    REQUIRE_FALSE(pick_net(nan, rect.y + 1.0f, rect, kSize));
    REQUIRE_FALSE(pick_net(rect.x + 1.0f, nan, rect, kSize));

    // A rectangle with no area cannot hold a cell.
    REQUIRE_FALSE(pick_net(0.0f, 0.0f, Rect{0.0f, 0.0f, 0.0f, 0.0f}, kSize));
}

TEST_CASE("every cell and direction names a turn that moves it that way")
{
    // The whole derivation at once: 54 cells by 4 directions, including the
    // 24 that step off the edge of the cross, where there is no neighbouring
    // block in the drawing but there is always one on the cube.
    for (const auto face : rubiks::graphics::net_faces()) {
        const auto frame = frame_of(face);

        for (int row = 0; row < kSize; ++row) {
            for (int col = 0; col < kSize; ++col) {
                for (const auto& direction : kDirections) {
                    INFO("face " << static_cast<int>(face) << " cell " << col
                                 << "," << row << " step " << direction[0]
                                 << "," << direction[1]);

                    const auto turn =
                        net_step_turn(face, col, row, direction[0],
                                      direction[1], kSize);
                    REQUIRE(turn);
                    REQUIRE((turn->sign == 1 || turn->sign == -1));

                    const Vec position = centered(face, col, row);
                    const Vec travel =
                        added(scaled(frame.col, direction[0]),
                              scaled(frame.row, direction[1]));

                    // A positive turn is clockwise seen from the positive end
                    // of the axis, which is a right-handed rotation the other
                    // way, so the sticker sets off along -(axis x position).
                    const Vec axis = axis_vector(turn->axis, turn->sign);
                    const Vec motion = scaled(cross(axis, position), -1);

                    // Off the face as well as along it: a sticker at the edge
                    // of a face also lifts away from it. What has to match the
                    // drag is the part along the face.
                    const Vec tangential =
                        added(motion,
                              scaled(frame.normal, -dot(motion, frame.normal)));
                    REQUIRE(tangential == travel);

                    // And the layer it turns is the one the pressed cell is
                    // in, so the drag moves the cell it started on.
                    const auto cell =
                        rubiks::graphics::net_cell(face, col, row, kSize);
                    const int index = turn->axis == Axis::X   ? cell.x
                                      : turn->axis == Axis::Y ? cell.y
                                                              : cell.z;
                    REQUIRE(turn->layer == index);
                }
            }
        }
    }
}

TEST_CASE("the four gestures on a corner cell name four different turns")
{
    // The same cell dragged four ways must not collapse onto fewer turns, or
    // one of the directions is being read as another.
    const auto right = net_step_turn(Face::Front, 0, 0, 1, 0, kSize);
    const auto left = net_step_turn(Face::Front, 0, 0, -1, 0, kSize);
    const auto down = net_step_turn(Face::Front, 0, 0, 0, 1, kSize);
    const auto up = net_step_turn(Face::Front, 0, 0, 0, -1, kSize);

    REQUIRE(right);
    REQUIRE(left);
    REQUIRE(down);
    REQUIRE(up);

    // Opposite drags are the same layer turned opposite ways.
    REQUIRE(right->axis == left->axis);
    REQUIRE(right->layer == left->layer);
    REQUIRE(right->sign == -left->sign);
    REQUIRE(down->axis == up->axis);
    REQUIRE(down->layer == up->layer);
    REQUIRE(down->sign == -up->sign);

    // And the two screen axes are different turns entirely.
    REQUIRE(right->axis != down->axis);
}

TEST_CASE("dragging the front face's top row sideways is the U layer")
{
    // The one case worth pinning by name: it is the gesture the browser test
    // performs, and the Up layer is the one a person expects to move.
    const auto left = net_step_turn(Face::Front, 1, 0, -1, 0, kSize);

    REQUIRE(left);
    REQUIRE(left->axis == Axis::Y);
    REQUIRE(left->layer == kLast);
    REQUIRE(left->sign == 1);
}

TEST_CASE("a direction that is not a single step names no turn")
{
    REQUIRE_FALSE(net_step_turn(Face::Front, 1, 1, 0, 0, kSize));
    REQUIRE_FALSE(net_step_turn(Face::Front, 1, 1, 1, 1, kSize));
    REQUIRE_FALSE(net_step_turn(Face::Front, 1, 1, 2, 0, kSize));

    // And a cell that is not on the face it claims to be on.
    REQUIRE_FALSE(net_step_turn(Face::Front, kSize, 0, 1, 0, kSize));
    REQUIRE_FALSE(net_step_turn(Face::Front, 0, -1, 1, 0, kSize));
}
