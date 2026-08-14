#include <algorithm>
#include <set>
#include <tuple>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"
#include "cube/Cubie.hpp"
#include "cube/Surface.hpp"

// Sticker identity and the ring cycles, in the same isolation as the rest of
// the domain: this links the cube target alone.
//
// The permutation under test is not new code. `turned_position` and the face
// cycle are what `CubeState` has always turned cubies with, and they are
// already pinned to a physical cube by the known-answer test next door. So
// what is checked here is the exposure and the ring construction built on it:
// a handful of answers worked out by hand, then structure.

namespace {

using rubiks::cube::Axis;
using rubiks::cube::CubeMove;
using rubiks::cube::CubeState;
using rubiks::cube::CubiePosition;
using rubiks::cube::Face;
using rubiks::cube::SurfaceSticker;
using rubiks::cube::axis_of;
using rubiks::cube::coordinate_on;
using rubiks::cube::layer;
using rubiks::cube::outer_layer;
using rubiks::cube::ring_slots;
using rubiks::cube::turned_face;
using rubiks::cube::turned_position;
using rubiks::cube::turned_sticker;

constexpr int kSize = 3;
constexpr int kLast = kSize - 1;

/** Every sticker of the surface, once each. */
std::vector<SurfaceSticker> all_stickers(int size)
{
    std::vector<SurfaceSticker> stickers;

    for (int i = 0; i < 6; ++i) {
        const auto face = static_cast<Face>(i);
        const int fixed = outer_layer(face, size);

        for (int a = 0; a < size; ++a) {
            for (int b = 0; b < size; ++b) {
                CubiePosition p{a, a, b};
                switch (axis_of(face)) {
                    case Axis::X:
                        p = CubiePosition{fixed, a, b};
                        break;
                    case Axis::Y:
                        p = CubiePosition{a, fixed, b};
                        break;
                    case Axis::Z:
                        p = CubiePosition{a, b, fixed};
                        break;
                }
                stickers.push_back(SurfaceSticker{p.x, p.y, p.z, face});
            }
        }
    }
    return stickers;
}

/** Ordered form, so stickers can go into a set. */
std::tuple<int, int, int, int> key(const SurfaceSticker& s)
{
    return {s.x, s.y, s.z, static_cast<int>(s.face)};
}

/** Whether two stickers touch on the surface, either flat or across an edge. */
bool adjacent(const SurfaceSticker& a, const SurfaceSticker& b)
{
    if (a.face == b.face) {
        const int distance = std::abs(a.x - b.x) + std::abs(a.y - b.y) +
                             std::abs(a.z - b.z);
        return distance == 1;
    }

    // Two faces of one cubie fold onto each other along the edge between them,
    // which is the only way a ring leaves a face.
    return a.x == b.x && a.y == b.y && a.z == b.z &&
           axis_of(a.face) != axis_of(b.face);
}

/** Whether a sticker is one of those the given layer turn carries away. */
bool in_band(const SurfaceSticker& s, Axis axis, int layer_index)
{
    return axis_of(s.face) != axis && coordinate_on(axis, s) == layer_index;
}

}  // namespace

TEST_CASE("a quarter turn moves stickers to the faces worked out by hand")
{
    // Looking down the axis from its positive end, a positive turn is
    // clockwise, so about X the top face goes to the back.
    REQUIRE(turned_face(Axis::X, Face::Up) == Face::Back);
    REQUIRE(turned_face(Axis::X, Face::Back) == Face::Down);
    REQUIRE(turned_face(Axis::X, Face::Down) == Face::Front);
    REQUIRE(turned_face(Axis::X, Face::Front) == Face::Up);

    REQUIRE(turned_face(Axis::Y, Face::Front) == Face::Left);
    REQUIRE(turned_face(Axis::Z, Face::Up) == Face::Right);

    // The two faces on the axis stay where they are.
    REQUIRE(turned_face(Axis::X, Face::Right) == Face::Right);
    REQUIRE(turned_face(Axis::X, Face::Left) == Face::Left);
    REQUIRE(turned_face(Axis::Y, Face::Up) == Face::Up);
    REQUIRE(turned_face(Axis::Z, Face::Front) == Face::Front);
}

TEST_CASE("a quarter turn carries a corner sticker where the cube does")
{
    // The same answer the domain's known-answer test states: R sends the UFR
    // cubie to the UBR slot. Its Up sticker arrives facing Back.
    const SurfaceSticker up_of_ufr{kLast, kLast, kLast, Face::Up};
    const auto moved = turned_sticker(up_of_ufr, Axis::X, kSize);

    REQUIRE(moved == SurfaceSticker{kLast, kLast, 0, Face::Back});

    // And the position agrees with what CubeState turns cubies by.
    REQUIRE(turned_position(Axis::X, CubiePosition{kLast, kLast, kLast},
                            kSize) == CubiePosition{kLast, kLast, 0});
}

TEST_CASE("turning stickers is a permutation of the surface that repeats at 4")
{
    const auto stickers = all_stickers(kSize);
    REQUIRE(stickers.size() == 54);

    for (const auto axis : {Axis::X, Axis::Y, Axis::Z}) {
        std::set<std::tuple<int, int, int, int>> images;

        for (const auto& sticker : stickers) {
            const auto moved = turned_sticker(sticker, axis, kSize);
            images.insert(key(moved));

            // A turn never takes a sticker out of its own layer, which is what
            // makes the band a closed set.
            REQUIRE(coordinate_on(axis, moved) ==
                    coordinate_on(axis, sticker));

            SurfaceSticker walked = sticker;
            for (int i = 0; i < 4; ++i) {
                walked = turned_sticker(walked, axis, kSize);
            }
            REQUIRE(walked == sticker);
        }

        // Onto as well as into: 54 distinct images of 54 stickers.
        REQUIRE(images.size() == stickers.size());
    }
}

TEST_CASE("a ring is the layer's whole band, once each")
{
    for (const auto axis : {Axis::X, Axis::Y, Axis::Z}) {
        for (int index = 0; index < kSize; ++index) {
            const auto slots = ring_slots(axis, index, kSize);
            REQUIRE(slots.size() == 4 * kSize);

            std::set<std::tuple<int, int, int, int>> seen;
            for (const auto& slot : slots) {
                REQUIRE(in_band(slot, axis, index));
                seen.insert(key(slot));
            }
            REQUIRE(seen.size() == slots.size());

            // The band has no other members: every sticker that belongs to
            // this layer turn is in the ring.
            std::size_t band_size = 0;
            for (const auto& sticker : all_stickers(kSize)) {
                if (in_band(sticker, axis, index)) ++band_size;
            }
            REQUIRE(band_size == slots.size());
        }
    }
}

TEST_CASE("a ring is a closed walk over neighbouring stickers")
{
    for (const auto axis : {Axis::X, Axis::Y, Axis::Z}) {
        for (int index = 0; index < kSize; ++index) {
            const auto slots = ring_slots(axis, index, kSize);

            for (std::size_t i = 0; i < slots.size(); ++i) {
                const auto& here = slots[i];
                const auto& next = slots[(i + 1) % slots.size()];
                REQUIRE(adjacent(here, next));
            }
        }
    }
}

TEST_CASE("a quarter turn is exactly one face's worth along the ring")
{
    for (const auto axis : {Axis::X, Axis::Y, Axis::Z}) {
        for (int index = 0; index < kSize; ++index) {
            const auto slots = ring_slots(axis, index, kSize);

            for (std::size_t i = 0; i < slots.size(); ++i) {
                REQUIRE(turned_sticker(slots[i], axis, kSize) ==
                        slots[(i + kSize) % slots.size()]);
            }
        }
    }
}

TEST_CASE("where the ring says a sticker goes, the cube shows its color")
{
    // Not a second reading of the same rule: this compares the ring against
    // the colors CubeState actually ends up holding. It cannot tell stickers
    // of one face apart, which is why the checks above carry the identity.
    for (const auto axis : {Axis::X, Axis::Y, Axis::Z}) {
        for (int index = 0; index < kSize; ++index) {
            CubeState before(kSize);
            before.apply(CubeMove{axis, layer(index), 5});  // scrambles a little

            CubeState after = before;
            after.apply(CubeMove{axis, layer(index), 1});

            const auto slots = ring_slots(axis, index, kSize);
            for (std::size_t i = 0; i < slots.size(); ++i) {
                const auto& from = slots[i];
                const auto& to = slots[(i + kSize) % slots.size()];

                REQUIRE(after.at(to.x, to.y, to.z).sticker(to.face) ==
                        before.at(from.x, from.y, from.z).sticker(from.face));
            }
        }
    }
}

TEST_CASE("the surface is six faces of N by N stickers, each named once")
{
    for (const int size : {2, 3, 4}) {
        const auto stickers = rubiks::cube::surface_stickers(size);
        REQUIRE(stickers.size() ==
                static_cast<std::size_t>(6 * size * size));

        std::set<std::tuple<int, int, int, int>> seen;
        for (const auto& sticker : stickers) {
            // On the cube, and on the outer layer of its own face's axis.
            REQUIRE(rubiks::cube::coordinate_on(
                        rubiks::cube::axis_of(sticker.face), sticker) ==
                    rubiks::cube::outer_layer(sticker.face, size));
            seen.insert({sticker.x, sticker.y, sticker.z,
                         static_cast<int>(sticker.face)});
        }
        REQUIRE(seen.size() == stickers.size());
    }

    REQUIRE(rubiks::cube::surface_stickers(0).empty());
}
