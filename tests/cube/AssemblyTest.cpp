#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cube/Assembly.hpp"
#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"
#include "cube/Scramble.hpp"
#include "cube/Surface.hpp"

// Reading a painting of stickers back as a cube, and refusing the ones that
// are not cubes.

namespace {

using rubiks::cube::assembled;
using rubiks::cube::Axis;
using rubiks::cube::CubeMove;
using rubiks::cube::CubeState;
using rubiks::cube::FaceColor;
using rubiks::cube::layers_through;
using rubiks::cube::make_scramble;
using rubiks::cube::PaintFault;
using rubiks::cube::painting_of;
using rubiks::cube::read_painting;
using rubiks::cube::surface_stickers;

CubeState scrambled(std::uint32_t seed, int size, std::size_t moves = 40)
{
    CubeState cube(size);
    cube.apply(make_scramble(size, seed, moves));
    return cube;
}

/** The same cube held a different way up, which is not a move the puzzle has. */
CubeState turned_over(const CubeState& cube, Axis axis, int quarters)
{
    CubeState out = cube;
    out.apply(CubeMove{axis, layers_through(0, cube.size() - 1), quarters});
    return out;
}

/** A painting of a cube, with one sticker written down wrong. */
std::vector<FaceColor> with_slip(const CubeState& cube, int slot,
                                 FaceColor instead)
{
    auto painting = painting_of(cube);
    painting[static_cast<std::size_t>(slot)] = instead;
    return painting;
}

}  // namespace

TEST_CASE("a cube painted as it stands is read back as itself")
{
    // The necessary direction of the rule: whatever turning can reach, the
    // gate has to let through. A rule that refuses one of these refuses
    // somebody's real cube.
    for (const int size : {2, 3, 4, 5, 6, 7}) {
        for (std::uint32_t seed = 0; seed < 8; ++seed) {
            const auto cube = scrambled(seed, size);
            const auto painting = painting_of(cube);

            INFO("size " << size << " seed " << seed);
            const auto reading = read_painting(size, painting);
            REQUIRE(reading.fault == PaintFault::None);

            const auto built = assembled(size, painting);
            REQUIRE(built.has_value());
            CHECK(painting_of(*built) == painting);
        }
    }
}

TEST_CASE("a cube held any way up is read all the same")
{
    // Nobody is going to be told which face to keep on top. A painting made
    // from a cube turned over is not reachable by any move -- turning the
    // whole cube is not a move -- so this passes only because every way of
    // holding it is tried.
    for (const int size : {2, 3, 4, 5, 6, 7}) {
        const auto cube = scrambled(3, size);

        for (const Axis axis : {Axis::X, Axis::Y, Axis::Z}) {
            for (const int quarters : {1, 2, 3}) {
                const auto held = turned_over(cube, axis, quarters);
                INFO("size " << size << " axis " << static_cast<int>(axis)
                             << " quarters " << quarters);

                const auto reading = read_painting(size, painting_of(held));
                CHECK(reading.fault == PaintFault::None);
                CHECK(assembled(size, painting_of(held)).has_value());
            }
        }
    }
}

TEST_CASE("an odd cube read from a different way up comes back the same cube")
{
    // An odd cube has a fixed centre on every face, so which way it was held
    // is written on it and the reading can put it back. An even cube has no
    // such mark and this is not asked of it: the orientation of a four by four
    // is genuinely not in the painting, and pretending otherwise would be
    // inventing an answer.
    for (const int size : {3, 5, 7}) {
        const auto cube = scrambled(5, size);
        for (const Axis axis : {Axis::X, Axis::Y, Axis::Z}) {
            const auto held = turned_over(cube, axis, 1);
            INFO("size " << size << " axis " << static_cast<int>(axis));
            const auto built = assembled(size, painting_of(held));
            REQUIRE(built.has_value());
            CHECK(painting_of(*built) == painting_of(cube));
        }
    }
}

TEST_CASE("a colour on too many stickers is counted and named")
{
    const auto cube = scrambled(1, 3);
    const auto painting = painting_of(cube);

    // Find a sticker that is not already white and make it white.
    int slot = -1;
    for (std::size_t i = 0; i < painting.size(); ++i) {
        if (painting[i] != FaceColor::White) slot = static_cast<int>(i);
        if (slot >= 0) break;
    }
    REQUIRE(slot >= 0);

    const auto reading =
        read_painting(3, with_slip(cube, slot, FaceColor::White));
    REQUIRE(reading.fault == PaintFault::ColourCount);
    CHECK(reading.expected == 9);
    CHECK(reading.found != 9);
    CHECK_FALSE(reading.blamed.empty());
    CHECK_FALSE(assembled(3, with_slip(cube, slot, FaceColor::White)));
}

TEST_CASE("centres traded between orbits are refused though the totals agree")
{
    // The check the plan nearly left out. A five by five has three orbits of
    // centres, and a white one swapped with a red one across two of them
    // leaves every total exactly where it was.
    const CubeState cube(5);
    auto painting = painting_of(cube);
    const auto slots = surface_stickers(5);

    // On a five by five a face's centre block is the three by three of cells
    // whose two in-face coordinates are 1, 2 or 3. The middle of an edge of
    // that block and the corner of it are in different orbits.
    int plus_slot = -1;
    int cross_slot = -1;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const auto& s = slots[i];
        if (s.face != rubiks::cube::Face::Up) continue;
        if (s.y != 4) continue;
        const bool plus = (s.x == 2 && s.z == 1);
        const bool cross = (s.x == 1 && s.z == 1);
        if (plus) plus_slot = static_cast<int>(i);
        if (cross) cross_slot = static_cast<int>(i);
    }
    REQUIRE(plus_slot >= 0);
    REQUIRE(cross_slot >= 0);

    // Give the two slots each other's colours by way of a red face, so that
    // the totals stay put and only the orbits move.
    painting[static_cast<std::size_t>(plus_slot)] = FaceColor::Red;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        if (painting[i] != FaceColor::Red) continue;
        if (static_cast<int>(i) == plus_slot) continue;
        painting[i] = FaceColor::White;
        break;
    }

    const auto reading = read_painting(5, painting);
    INFO("fault " << static_cast<int>(reading.fault));
    CHECK(reading.fault != PaintFault::None);
    CHECK_FALSE(assembled(5, painting));
}

TEST_CASE("a cube built to another scheme of opposites is told so")
{
    // A cube with blue opposite white is a real cube somebody owns; it is not
    // one this build solves, and the difference is worth saying out loud
    // rather than calling every one of its pieces impossible.
    const auto cube = scrambled(2, 3);
    auto painting = painting_of(cube);
    for (auto& colour : painting) {
        if (colour == FaceColor::Yellow) {
            colour = FaceColor::Blue;
        } else if (colour == FaceColor::Blue) {
            colour = FaceColor::Yellow;
        }
    }

    const auto reading = read_painting(3, painting);
    CHECK(reading.fault == PaintFault::OppositePairs);
    CHECK_FALSE(assembled(3, painting));
}

TEST_CASE("a corner that no turn could make is refused")
{
    // Two of a corner's three stickers swapped. The three colours are still a
    // corner's colours, so nothing is miscounted -- what is wrong is that the
    // corner now reads round the other way, and no turn has ever reversed one.
    const CubeState cube(3);
    auto painting = painting_of(cube);
    const auto slots = surface_stickers(3);

    std::vector<int> corner;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const auto& s = slots[i];
        if (s.x == 2 && s.y == 2 && s.z == 2) corner.push_back(static_cast<int>(i));
    }
    REQUIRE(corner.size() == 3);
    std::swap(painting[static_cast<std::size_t>(corner[0])],
              painting[static_cast<std::size_t>(corner[1])]);

    const auto reading = read_painting(3, painting);
    INFO("fault " << static_cast<int>(reading.fault));
    CHECK(reading.fault != PaintFault::None);
    CHECK_FALSE(assembled(3, painting));
}
