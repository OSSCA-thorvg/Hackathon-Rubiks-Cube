#include "cube/solver/Projection.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <vector>

#include "cube/Cubie.hpp"
#include "cube/Surface.hpp"
#include "cube/solver/Turning.hpp"

namespace rubiks::cube::solver {
namespace {

/**
 * The eight corner slots and the twelve edge slots, in one fixed order.
 *
 * The order is arbitrary and it is the point: a piece is named by the slot it
 * belongs in, so as long as every size is read in the same order, a nine by
 * nine and a three by three turned the same way say the same thing.
 */
constexpr std::array<std::array<Face, 3>, kCornerSlots> kCorners{{
    {Face::Right, Face::Up, Face::Front},
    {Face::Right, Face::Up, Face::Back},
    {Face::Right, Face::Down, Face::Front},
    {Face::Right, Face::Down, Face::Back},
    {Face::Left, Face::Up, Face::Front},
    {Face::Left, Face::Up, Face::Back},
    {Face::Left, Face::Down, Face::Front},
    {Face::Left, Face::Down, Face::Back},
}};

constexpr std::array<std::array<Face, 2>, kEdgeSlots> kEdges{{
    {Face::Up, Face::Front},
    {Face::Up, Face::Back},
    {Face::Down, Face::Front},
    {Face::Down, Face::Back},
    {Face::Up, Face::Right},
    {Face::Up, Face::Left},
    {Face::Down, Face::Right},
    {Face::Down, Face::Left},
    {Face::Front, Face::Right},
    {Face::Front, Face::Left},
    {Face::Back, Face::Right},
    {Face::Back, Face::Left},
}};

/**
 * The facelet of an edge slot that its orientation is read from.
 *
 * The up or down one where there is one, and the front or back one otherwise.
 * This choice is not free. A turn of the up or down face carries every one of
 * its four slots' up facelets to another up facelet, so nothing changes; a
 * turn of the right or left face carries all four of its slots' chosen
 * facelets to chosen facelets as well; and a turn of the front or back face
 * misses on all four of its slots at once. Four is even, so however the cube
 * is turned the *number* of pairs wearing their colours the other way round
 * changes by an even number and its parity does not change at all. That
 * invariance is the whole value of the choice, and a different choice loses
 * it.
 */
[[nodiscard]] Face read_from(const std::array<Face, 2>& slot) noexcept
{
    for (const Face face : slot) {
        if (axis_of(face) == Axis::Y) return face;
    }
    for (const Face face : slot) {
        if (axis_of(face) == Axis::Z) return face;
    }
    assert(false);
    return slot.front();
}

/** The colour of a piece that belongs on the facelet `read_from` picks. */
[[nodiscard]] FaceColor read_colour(const std::array<Face, 2>& piece) noexcept
{
    return home_colour(read_from(piece));
}

[[nodiscard]] bool same_colours(const std::vector<FaceColor>& a,
                                const std::vector<FaceColor>& b) noexcept
{
    for (const auto colour : a) {
        bool found = false;
        for (const auto other : b) found = found || other == colour;
        if (!found) return false;
    }
    return a.size() == b.size();
}

/** Which of the numbered slots a piece of these colours belongs in. */
template <std::size_t N, std::size_t Slots>
[[nodiscard]] int home_slot(const std::array<std::array<Face, N>, Slots>& table,
                            const std::vector<FaceColor>& colours)
{
    for (std::size_t i = 0; i < Slots; ++i) {
        std::vector<FaceColor> home;
        for (const Face face : table[i]) home.push_back(home_colour(face));
        if (same_colours(colours, home)) return static_cast<int>(i);
    }
    assert(false);
    return 0;
}

/** The parity of a permutation: how many swaps it is, counted odd or even. */
template <std::size_t N>
[[nodiscard]] bool odd_permutation(const std::array<int, N>& where)
{
    std::array<bool, N> visited{};
    bool odd = false;
    for (std::size_t start = 0; start < N; ++start) {
        if (visited[start]) continue;

        // Every cycle of length L is L - 1 swaps, so a cycle of even length is
        // what makes a permutation odd.
        std::size_t length = 0;
        for (std::size_t at = start; !visited[at];
             at = static_cast<std::size_t>(where[at])) {
            visited[at] = true;
            ++length;
        }
        if (length % 2 == 0) odd = !odd;
    }
    return odd;
}

}  // namespace

bool reduced(const CubeState& cube)
{
    const int size = cube.size();

    for (const auto& slot : kEdges) {
        const auto row = edge_row(slot[0], slot[1], size);
        for (const auto& at : row) {
            if (sticker_at(cube, at, slot[0]) !=
                    sticker_at(cube, row.front(), slot[0]) ||
                sticker_at(cube, at, slot[1]) !=
                    sticker_at(cube, row.front(), slot[1])) {
                return false;
            }
        }
    }

    for (int x = 0; x < size; ++x) {
        for (int y = 0; y < size; ++y) {
            for (int z = 0; z < size; ++z) {
                const CubiePosition at{x, y, z};
                const auto showing = exposed_faces(at, size);
                if (showing.size() != 1) continue;
                if (sticker_at(cube, at, showing.front()) !=
                    home_colour(showing.front())) {
                    return false;
                }
            }
        }
    }
    return true;
}

Projection projected(const CubeState& cube)
{
    assert(cube.size() >= 3);
    assert(reduced(cube));

    Projection projection;

    for (std::size_t i = 0; i < kCornerSlots; ++i) {
        const auto& slot = kCorners[i];
        const auto at = corner_slot(slot[0], slot[1], slot[2], cube.size());

        std::vector<FaceColor> colours;
        for (const Face face : slot) colours.push_back(sticker_at(cube, at, face));
        projection.corners[i] = home_slot(kCorners, colours);
    }

    for (std::size_t i = 0; i < kEdgeSlots; ++i) {
        const auto& slot = kEdges[i];
        const auto at = edge_slot(slot[0], slot[1], cube.size());

        std::vector<FaceColor> colours;
        for (const Face face : slot) colours.push_back(sticker_at(cube, at, face));

        const int piece = home_slot(kEdges, colours);
        projection.edges[i] = piece;
        projection.turned_round[i] =
            sticker_at(cube, at, read_from(slot)) ==
                    read_colour(kEdges[static_cast<std::size_t>(piece)])
                ? 0
                : 1;
    }
    return projection;
}

Parities parities(const Projection& projection)
{
    int turned = 0;
    for (const int one : projection.turned_round) turned += one;

    return Parities{turned % 2 != 0,
                    odd_permutation(projection.corners) !=
                        odd_permutation(projection.edges)};
}

}  // namespace rubiks::cube::solver
