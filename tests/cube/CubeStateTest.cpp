#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "CubeSupport.hpp"
#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"
#include "cube/Cubie.hpp"
#include "cube/Scramble.hpp"

// The cube domain in isolation. This executable links the cube target alone,
// so anything that reaches for graphics, math or ThorVG fails to build here
// before it can fail in review.

namespace {

using rubiks::cube::Axis;
using rubiks::cube::CubeMove;
using rubiks::cube::CubeState;
using rubiks::cube::Face;
using rubiks::cube::FaceColor;
using rubiks::cube::inverse;
using rubiks::cube::layer;
using rubiks::cube::layers_through;
using rubiks::cube::solved_color;
using rubiks::test::apply_all;
using rubiks::test::inverse_of;

constexpr int kSize = 3;
constexpr int kLast = kSize - 1;

/** How long a scramble is here when its length is not what is being asked. */
constexpr std::size_t kMoves = 20;

/** Applies `move` `count` times to a fresh solved cube. */
CubeState after(const CubeMove& move, int count, int size = kSize)
{
    CubeState state(size);
    for (int i = 0; i < count; ++i) {
        state.apply(move);
    }
    return state;
}

/** How many layers a mask holds. */
int width_of(rubiks::cube::LayerMask layers)
{
    int count = 0;
    for (rubiks::cube::LayerMask bit = layers; bit != 0; bit >>= 1U) {
        count += static_cast<int>(bit & 1U);
    }
    return count;
}

/**
 * Stickers away from any face's border that are not the color they began.
 *
 * The pieces an outer-face turn can never reach: a face's border belongs to
 * the four layers around it, and everything inside it moves only when a move
 * reaches past the surface. Zero on a cube with no inside to speak of.
 */
int disturbed_inner_stickers(const CubeState& state)
{
    const int size = state.size();
    const int last = size - 1;
    int disturbed = 0;

    for (int a = 1; a < last; ++a) {
        for (int b = 1; b < last; ++b) {
            const std::pair<Face, FaceColor> checks[] = {
                {Face::Right, state.at(last, a, b).sticker(Face::Right)},
                {Face::Left, state.at(0, a, b).sticker(Face::Left)},
                {Face::Up, state.at(a, last, b).sticker(Face::Up)},
                {Face::Down, state.at(a, 0, b).sticker(Face::Down)},
                {Face::Front, state.at(a, b, last).sticker(Face::Front)},
                {Face::Back, state.at(a, b, 0).sticker(Face::Back)},
            };
            for (const auto& [face, color] : checks) {
                if (color != solved_color(face)) ++disturbed;
            }
        }
    }
    return disturbed;
}

/** The whole cube turning about an axis: every layer at once. */
CubeMove whole_cube(Axis axis, int size)
{
    return CubeMove{axis, layers_through(0, size - 1), 1};
}

}  // namespace

TEST_CASE("a fresh cube is solved")
{
    const CubeState state(kSize);

    REQUIRE(state.size() == kSize);
    REQUIRE(state.is_solved());
    REQUIRE(state.at(0, 0, 0).sticker(Face::Left) == FaceColor::Orange);
    REQUIRE(state.at(kLast, kLast, kLast).sticker(Face::Right) ==
            FaceColor::Red);
}

TEST_CASE("every sticker on a solved face shows that face's color")
{
    const CubeState state(kSize);

    // Walk the outer layer of each face and check all N x N stickers, which
    // is the domain-side counterpart of the rendered net verification.
    for (int a = 0; a < kSize; ++a) {
        for (int b = 0; b < kSize; ++b) {
            REQUIRE(state.at(kLast, a, b).sticker(Face::Right) ==
                    solved_color(Face::Right));
            REQUIRE(state.at(0, a, b).sticker(Face::Left) ==
                    solved_color(Face::Left));
            REQUIRE(state.at(a, kLast, b).sticker(Face::Up) ==
                    solved_color(Face::Up));
            REQUIRE(state.at(a, 0, b).sticker(Face::Down) ==
                    solved_color(Face::Down));
            REQUIRE(state.at(a, b, kLast).sticker(Face::Front) ==
                    solved_color(Face::Front));
            REQUIRE(state.at(a, b, 0).sticker(Face::Back) ==
                    solved_color(Face::Back));
        }
    }
}

TEST_CASE("R moves the up-front-right cubie to the up-back-right slot")
{
    // Known answer taken from a physical cube: after R the corner that was at
    // the top front right sits at the top back right, showing the old front
    // color on top and the old top color at the back.
    CubeState state(kSize);
    state.apply(rubiks::test::moves::R(kSize));

    const auto& corner = state.at(kLast, kLast, 0);
    REQUIRE(corner.sticker(Face::Up) == FaceColor::Green);
    REQUIRE(corner.sticker(Face::Back) == FaceColor::White);
    REQUIRE(corner.sticker(Face::Right) == FaceColor::Red);
}

TEST_CASE("every face move has order four")
{
    using namespace rubiks::test::moves;
    const CubeState solved(kSize);

    for (const auto& move : {R(kSize), L(kSize), U(kSize), D(kSize), F(kSize),
                             B(kSize)}) {
        REQUIRE_FALSE(after(move, 1) == solved);
        REQUIRE_FALSE(after(move, 1).is_solved());
        REQUIRE_FALSE(after(move, 2) == solved);
        REQUIRE_FALSE(after(move, 3) == solved);
        REQUIRE(after(move, 4) == solved);
        REQUIRE(after(move, 4).is_solved());
    }
}

TEST_CASE("a move followed by its inverse leaves the cube untouched")
{
    using namespace rubiks::test::moves;
    const CubeState solved(kSize);

    for (const auto& move : {R(kSize), L(kSize), U(kSize), D(kSize), F(kSize),
                             B(kSize)}) {
        CubeState state(kSize);
        state.apply(move);
        state.apply(inverse(move));
        REQUIRE(state == solved);
    }
}

TEST_CASE("quarter turns are taken modulo four")
{
    const auto base = rubiks::test::moves::R(kSize);

    CubeState by_five(kSize);
    by_five.apply(CubeMove{base.axis, base.layers, 5});
    REQUIRE(by_five == after(base, 1));

    CubeState by_zero(kSize);
    by_zero.apply(CubeMove{base.axis, base.layers, 0});
    REQUIRE(by_zero == CubeState(kSize));

    CubeState negative(kSize);
    negative.apply(CubeMove{base.axis, base.layers, -1});
    REQUIRE(negative == after(base, 3));
}

TEST_CASE("the sexy move returns to solved after six repetitions")
{
    using namespace rubiks::test::moves;

    // (R U R' U') has order exactly six. A wrong sticker permutation that
    // still happens to have order four per face will not survive this.
    const std::vector<CubeMove> sequence{R(kSize), U(kSize),
                                         inverse(R(kSize)),
                                         inverse(U(kSize))};
    CubeState state(kSize);

    for (int i = 1; i <= 6; ++i) {
        apply_all(state, sequence);
        if (i < 6) REQUIRE_FALSE(state == CubeState(kSize));
    }
    REQUIRE(state == CubeState(kSize));
}

TEST_CASE("a whole-cube rotation has order four on every axis")
{
    const CubeState solved(kSize);

    for (const auto axis : rubiks::cube::axes()) {
        const auto rotation = whole_cube(axis, kSize);
        REQUIRE_FALSE(after(rotation, 1) == solved);
        REQUIRE(after(rotation, 4) == solved);
    }
}

TEST_CASE("wide and slice moves are built by naming layers")
{
    const CubeState solved(kSize);

    // Rw on a 3x3x3: the right face plus the middle slice.
    const CubeMove wide{Axis::X, layers_through(1, kLast), 1};
    // M: the middle slice alone.
    const CubeMove slice{Axis::X, layer(1), 1};

    REQUIRE(after(wide, 4) == solved);
    REQUIRE(after(slice, 4) == solved);

    // Widening the layer set has to change the result, or `layers` is being
    // ignored and every move is secretly a face turn.
    REQUIRE_FALSE(after(wide, 1) == after(rubiks::test::moves::R(kSize), 1));
    REQUIRE_FALSE(after(slice, 1) == after(wide, 1));
}

TEST_CASE("a scramble is undone by the reversed inverse sequence")
{
    using namespace rubiks::test::moves;

    const std::vector<CubeMove> scramble{
        R(kSize), U(kSize),          F(kSize),  inverse(L(kSize)),
        D(kSize), inverse(B(kSize)), R(kSize),  U(kSize),
        CubeMove{Axis::X, layers_through(1, kLast), 1},  // Rw
        CubeMove{Axis::Y, layer(1), -1},                 // E'
    };

    CubeState state(kSize);
    apply_all(state, scramble);
    REQUIRE_FALSE(state == CubeState(kSize));
    REQUIRE_FALSE(state.is_solved());

    apply_all(state, inverse_of(scramble));
    REQUIRE(state == CubeState(kSize));
    REQUIRE(state.is_solved());
}

TEST_CASE("seeded scrambles are reproducible and structurally valid")
{
    constexpr std::uint32_t kSeed = 0x12345678U;
    const auto first = rubiks::cube::make_scramble(kSize, kSeed, kMoves);
    const auto again = rubiks::cube::make_scramble(kSize, kSeed, kMoves);
    const auto different =
        rubiks::cube::make_scramble(kSize, kSeed + 1U, kMoves);

    REQUIRE(first == again);
    REQUIRE(first != different);
    REQUIRE(first.size() == kMoves);

    // The count is the caller's, and asking for fewer moves gives the same
    // sequence cut short rather than a different one.
    const auto shorter = rubiks::cube::make_scramble(kSize, kSeed, 5);
    REQUIRE(shorter.size() == 5);
    REQUIRE(std::equal(shorter.begin(), shorter.end(), first.begin()));

    for (std::size_t index = 0; index < first.size(); ++index) {
        const CubeMove& move = first[index];
        REQUIRE((move.layers == layer(0) || move.layers == layer(kLast)));
        REQUIRE((move.quarter_turns == -1 || move.quarter_turns == 1 ||
                 move.quarter_turns == 2));
        if (index > 0) REQUIRE(move.axis != first[index - 1].axis);
    }
}

TEST_CASE("a scramble reaches the layers inside a larger cube")
{
    using rubiks::cube::in_layers;
    using rubiks::cube::is_layer_run;

    for (int size = 2; size <= 9; ++size) {
        const auto scramble = rubiks::cube::make_scramble(size, 0xabcdef01U, 60);
        REQUIRE(scramble.size() == 60);

        // Every move is a face turn or a wide move from one of the two faces
        // of its axis, which is one unbroken run of layers reaching an edge
        // and never reaching past the middle.
        for (std::size_t index = 0; index < scramble.size(); ++index) {
            const CubeMove& move = scramble[index];
            REQUIRE(is_layer_run(move.layers, size));
            REQUIRE((in_layers(move.layers, 0) ||
                     in_layers(move.layers, size - 1)));
            REQUIRE(width_of(move.layers) <= size / 2);
            REQUIRE((move.quarter_turns == -1 || move.quarter_turns == 1 ||
                     move.quarter_turns == 2));
            if (index > 0) REQUIRE(move.axis != scramble[index - 1].axis);
        }

        CubeState state(size);
        apply_all(state, scramble);
        REQUIRE_FALSE(state.is_solved());

        // The point of the depths, read off the cube rather than off the
        // moves: the pieces away from a face's border are the ones only a
        // move reaching inside can disturb, and on a cube that has any they
        // have been disturbed. A scramble of outer faces alone would leave
        // every one of them where it started however long it ran.
        if (size >= 4) REQUIRE(disturbed_inner_stickers(state) > 0);

        apply_all(state, inverse_of(scramble));
        REQUIRE(state.is_solved());
    }
}

TEST_CASE("a run of layers is told apart from a set with a gap in it")
{
    using rubiks::cube::is_layer_run;

    REQUIRE(is_layer_run(layer(0), 3));
    REQUIRE(is_layer_run(layer(2), 3));
    REQUIRE(is_layer_run(layers_through(0, 2), 3));
    REQUIRE(is_layer_run(layers_through(1, 2), 3));

    // A gap, an empty set, and layers the cube does not have.
    REQUIRE_FALSE(is_layer_run(layer(0) | layer(2), 3));
    REQUIRE_FALSE(is_layer_run(0, 3));
    REQUIRE_FALSE(is_layer_run(layer(3), 3));
    REQUIRE_FALSE(is_layer_run(layers_through(0, 3), 3));

    // A size that would shift the mask off the end answers rather than wraps.
    REQUIRE_FALSE(is_layer_run(layer(0), 32));
    REQUIRE_FALSE(is_layer_run(layer(0), 0));
}

TEST_CASE("a generated scramble is restored by its inverse")
{
    const auto scramble = rubiks::cube::make_scramble(kSize, 42U, kMoves);
    CubeState state(kSize);

    apply_all(state, scramble);
    REQUIRE_FALSE(state.is_solved());

    apply_all(state, inverse_of(scramble));
    REQUIRE(state.is_solved());
}

TEST_CASE("zero seed is deterministic and invalid scramble sizes are empty")
{
    REQUIRE(rubiks::cube::make_scramble(kSize, 0U, kMoves) ==
            rubiks::cube::make_scramble(kSize, 0U, kMoves));
    REQUIRE(rubiks::cube::make_scramble(0, 7U, kMoves).empty());
    REQUIRE(rubiks::cube::make_scramble(-1, 7U, kMoves).empty());
    REQUIRE(rubiks::cube::make_scramble(kSize, 7U, 0).empty());
}

TEST_CASE("the move engine holds for an even cube size")
{
    // N = 2 is the size where signed {-1, 0, +1} coordinates would have gone
    // half-integer, and it is the smallest size the application offers.
    constexpr int kSmall = 2;
    const CubeState solved(kSmall);
    REQUIRE(solved.size() == kSmall);

    const auto move = rubiks::test::moves::R(kSmall);
    REQUIRE(move.layers == layer(1));

    REQUIRE_FALSE(after(move, 1, kSmall) == solved);
    REQUIRE(after(move, 4, kSmall) == solved);

    CubeState state(kSmall);
    state.apply(move);
    state.apply(inverse(move));
    REQUIRE(state == solved);

    // Both layers on a 2x2x2 make it a whole-cube rotation, which still has
    // order four but is not the same permutation as the face turn.
    REQUIRE(after(CubeMove{Axis::X, layers_through(0, 1), 1}, 4, kSmall) ==
            solved);
}

TEST_CASE("cubes of different sizes are never equal")
{
    REQUIRE_FALSE(CubeState(2) == CubeState(3));
}

TEST_CASE("the axes, and the two beside any one, come in the enum's order")
{
    using rubiks::cube::axes;
    using rubiks::cube::axis_index;
    using rubiks::cube::other_axes;

    // Written out by hand rather than looped over: a seeded scramble draws
    // its axis by this order and the solver builds its tools in it, so the
    // order is part of what a seed and a solution mean.
    const auto listed = axes();
    REQUIRE(listed.size() == 3);
    REQUIRE(listed[0] == Axis::X);
    REQUIRE(listed[1] == Axis::Y);
    REQUIRE(listed[2] == Axis::Z);

    REQUIRE(axis_index(Axis::X) == 0);
    REQUIRE(axis_index(Axis::Y) == 1);
    REQUIRE(axis_index(Axis::Z) == 2);

    // The same order again for the two that are left: a face's stickers are
    // walked across the first and down the second, which is the order a
    // painting is indexed in.
    REQUIRE(other_axes(Axis::X) == std::array<Axis, 2>{Axis::Y, Axis::Z});
    REQUIRE(other_axes(Axis::Y) == std::array<Axis, 2>{Axis::X, Axis::Z});
    REQUIRE(other_axes(Axis::Z) == std::array<Axis, 2>{Axis::X, Axis::Y});
}

TEST_CASE("a turn count wraps into 0 to 3 and a mask answers for each layer")
{
    using rubiks::cube::in_layers;
    using rubiks::cube::normalized_turns;

    // A quarter back is three forward, a quarter past a whole turn is one,
    // and a whole turn back is none.
    REQUIRE(normalized_turns(-1) == 3);
    REQUIRE(normalized_turns(5) == 1);
    REQUIRE(normalized_turns(-4) == 0);

    // The two inner slices of a cube of four, and neither of its faces.
    const auto inner = layers_through(1, 2);
    REQUIRE_FALSE(in_layers(inner, 0));
    REQUIRE(in_layers(inner, 1));
    REQUIRE(in_layers(inner, 2));
    REQUIRE_FALSE(in_layers(inner, 3));
}
