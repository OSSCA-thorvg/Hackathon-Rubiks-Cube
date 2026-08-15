#include "cube/PackedMove.hpp"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "cube/CubeMove.hpp"
#include "cube/MoveTimeline.hpp"

namespace {

constexpr int kSize = 3;

using rubiks::cube::Axis;
using rubiks::cube::CubeMove;
using rubiks::cube::layer;
using rubiks::cube::pack;
using rubiks::cube::unpack;

/** The word the reader on the other side has to arrive at for `R`. */
constexpr std::uint32_t kPackedR = 0x44;

}  // namespace

// The reader of these words lives in TypeScript, and the two sides agree by
// both being fixed to the same known answers rather than by sharing a header.
// So the numbers below are written out in full: a test that recomputed them
// from the shifts would pass however the shifts moved, and would let the two
// sides drift apart with both of their test suites green.
static_assert(pack(CubeMove{Axis::X, layer(2), 1}) == kPackedR,
              "R is axis X, one clockwise quarter, layer 2");

TEST_CASE("a move is written as axis, turns and layers in one word")
{
    using namespace rubiks::cube::moves;

    // Axis X = 0, turns +1 = code 1 at bit 2, layer 2 = bit 6.
    REQUIRE(pack(R(kSize)) == 0x44);

    // The same layer the other way: only the turns code changes.
    REQUIRE(pack(inverse(R(kSize))) == 0x40);

    // A negative face is a counter-clockwise turn about the positive axis, and
    // it is stored that way -- the notation letter is the reader's business.
    REQUIRE(pack(L(kSize)) == 0x10);

    // Axis Y = 1 and axis Z = 2 land in the low two bits.
    REQUIRE(pack(U(kSize)) == 0x45);
    REQUIRE(pack(F(kSize)) == 0x46);
    REQUIRE(pack(D(kSize)) == 0x11);
    REQUIRE(pack(B(kSize)) == 0x12);

    // The middle layer of each axis: one bit further down the mask.
    REQUIRE(pack(CubeMove{Axis::X, layer(1), -1}) == 0x20);
    REQUIRE(pack(CubeMove{Axis::Y, layer(1), -1}) == 0x21);
    REQUIRE(pack(CubeMove{Axis::Z, layer(1), 1}) == 0x26);
}

TEST_CASE("turns are normalized to the three a reader can tell apart")
{
    // A full revolution either way is no move at all, and there is no word for
    // one: nothing was turned, so nothing is written down.
    REQUIRE(pack(CubeMove{Axis::X, layer(2), 0}) == 0);
    REQUIRE(pack(CubeMove{Axis::X, layer(2), 4}) == 0);
    REQUIRE(pack(CubeMove{Axis::X, layer(2), -4}) == 0);

    // Three quarters one way is one quarter the other, and a revolution on top
    // of a turn is that turn.
    REQUIRE(pack(CubeMove{Axis::X, layer(2), 3}) ==
            pack(CubeMove{Axis::X, layer(2), -1}));
    REQUIRE(pack(CubeMove{Axis::X, layer(2), 5}) ==
            pack(CubeMove{Axis::X, layer(2), 1}));
    REQUIRE(pack(CubeMove{Axis::X, layer(2), -3}) ==
            pack(CubeMove{Axis::X, layer(2), 1}));

    // Both ways round to a half turn are one move on the cube and one word on
    // the page, which is the whole reason the direction may be dropped here.
    REQUIRE(pack(CubeMove{Axis::X, layer(2), 2}) ==
            pack(CubeMove{Axis::X, layer(2), -2}));
    REQUIRE(pack(CubeMove{Axis::X, layer(2), 2}) == 0x48);
}

TEST_CASE("normalizing a move to write it down does not reach the record")
{
    using rubiks::cube::MoveTimeline;
    using rubiks::cube::redo_plan;
    using rubiks::cube::rewind_plan;
    using rubiks::cube::TimelineEffect;

    // A half turn made the negative way round, which is what a counter-
    // clockwise double on a negative face comes to.
    const CubeMove played{Axis::X, layer(0), -2};

    MoveTimeline timeline;
    timeline.record(played);

    // Written down without its direction...
    REQUIRE(pack(timeline.at(0)) == pack(CubeMove{Axis::X, layer(0), 2}));

    // ...and kept with it. A replay turns the way the move was made, which is
    // the property the record exists for: normalizing on the way in would have
    // sent this one back the other way round.
    REQUIRE(timeline.at(0).quarter_turns == -2);

    timeline.step(TimelineEffect::Rewind);
    REQUIRE(redo_plan(timeline, 1)[0] == played);
    timeline.step(TimelineEffect::Advance);
    REQUIRE(rewind_plan(timeline, 0)[0] == inverse(played));
}

TEST_CASE("a word read back is the move it was written from")
{
    using namespace rubiks::cube::moves;

    // Every move a session can hold survives the round trip, direction and
    // all -- which is what makes a shared record the same cube on both sides.
    for (const CubeMove& move :
         {R(kSize), L(kSize), U(kSize), D(kSize), F(kSize), B(kSize),
          CubeMove{Axis::X, layer(1), -1}, CubeMove{Axis::Z, layer(1), 1}}) {
        REQUIRE(unpack(pack(move)) == move);
    }

    // Up to the normalizing the writing does, which is the one thing that does
    // not come back: a half turn has no direction on the page, so the word for
    // the two ways round is one word and reads back as the positive one.
    REQUIRE(unpack(pack(CubeMove{Axis::X, layer(0), -2})) ==
            CubeMove{Axis::X, layer(0), 2});
}

TEST_CASE("a word that is not a move reads back as nothing")
{
    // Zero, which is the engine's own answer for "no move at that index", so
    // it arrives here as a matter of course rather than as corruption.
    REQUIRE_FALSE(unpack(0).has_value());

    // Two bits for three axes and two bits for three turn counts leave one
    // invalid value in each field. Nothing pack() writes can take either, so
    // these are the words a stranger's link brings and nothing else does.
    REQUIRE_FALSE(unpack(kPackedR | 0x3).has_value());
    REQUIRE_FALSE(unpack(0x4C).has_value());

    // A turns code with no layers behind it is not a move either: the layer
    // set is what a move turns, and an empty one turns nothing.
    REQUIRE_FALSE(unpack(0x4).has_value());

    // A mask of several layers is a move, and a perfectly readable one. What
    // may be done with it is the caller's to decide, not this function's.
    REQUIRE(unpack(0x74) ==
            CubeMove{Axis::X, rubiks::cube::layers_through(0, 2), 1});
}

TEST_CASE("a move that cannot be written comes back as nothing")
{
    // Zero is the one word kept for "no move here", which is why an empty
    // layer set has to take it rather than packing to a bare turns code.
    REQUIRE(pack(CubeMove{Axis::X, 0, 1}) == 0);

    // Layers past the width of the field, which no cube this engine builds
    // can reach -- the word says so rather than losing the high bits quietly.
    REQUIRE(pack(CubeMove{Axis::X, rubiks::cube::layer(28), 1}) == 0);
    REQUIRE(pack(CubeMove{Axis::X, rubiks::cube::layer(31), 1}) == 0);

    // The widest cube that still fits, so the limit is a real edge and not an
    // off-by-one that refuses everything near it.
    REQUIRE(pack(CubeMove{Axis::X, rubiks::cube::layer(27), 1}) != 0);

    // Several layers at once has a word of its own: the mask is the mask, and
    // whether anybody can write notation for it is not decided here.
    REQUIRE(pack(CubeMove{Axis::X, rubiks::cube::layers_through(0, 2), 1}) ==
            0x74);
}
