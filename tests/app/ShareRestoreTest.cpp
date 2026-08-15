#include "app/Application.hpp"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "EngineLifecycle.hpp"
#include "cube/CubeMove.hpp"
#include "cube/PackedMove.hpp"

// What a shared link carries, and what comes back out of it. The record is
// read through the queries a link is built from and written back through the
// engine-owned buffer, so nothing here knows the encoding -- that half lives
// in TypeScript, and the two meet at the packed words.

namespace {

using rubiks::test::settle;

constexpr std::uint32_t kCanvas = 256;

/** The two stretches a shared state is made of, as packed words. */
struct Session {
    std::vector<std::uint32_t> scramble;
    std::vector<std::uint32_t> user;
};

/** A copy of the frame just drawn, so two cubes can be compared. */
[[nodiscard]] std::vector<std::uint8_t> drawn_frame()
{
    REQUIRE(rubiks::app::render());

    const auto* pixels =
        reinterpret_cast<const std::uint8_t*>(rubiks::app::pixel_buffer());
    const std::uint32_t length = rubiks::app::pixel_byte_length();
    REQUIRE(pixels != nullptr);
    REQUIRE(length > 0);

    return std::vector<std::uint8_t>(pixels, pixels + length);
}

/**
 * The session a link would be built from right now.
 *
 * Exactly the rule the share control follows: the scramble is everything below
 * the boundary, and the user's stretch stops at the cursor rather than at the
 * end of the record -- so a rewound tail is not carried.
 */
[[nodiscard]] Session current_session()
{
    const auto scramble_end = rubiks::app::timeline_scramble_end();
    const auto cursor = rubiks::app::timeline_cursor();
    REQUIRE(cursor >= scramble_end);

    Session session;
    for (std::uint32_t index = 0; index < scramble_end; ++index) {
        session.scramble.push_back(rubiks::app::timeline_move(index));
    }
    for (std::uint32_t index = scramble_end; index < cursor; ++index) {
        session.user.push_back(rubiks::app::timeline_move(index));
    }
    return session;
}

/** Writes a list of words into the engine's buffer and asks for them back. */
[[nodiscard]] bool restore(const std::vector<std::uint32_t>& words,
                           std::uint32_t scramble_count,
                           std::uint32_t user_count)
{
    const auto address =
        rubiks::app::restore_buffer(static_cast<std::uint32_t>(words.size()));
    if (address == 0) return false;

    auto* buffer = reinterpret_cast<std::uint32_t*>(address);
    for (std::size_t index = 0; index < words.size(); ++index) {
        buffer[index] = words[index];
    }

    return rubiks::app::restore_apply(scramble_count, user_count);
}

/** The same, for a session that is already split into its two stretches. */
[[nodiscard]] bool restore(const Session& session)
{
    std::vector<std::uint32_t> words = session.scramble;
    words.insert(words.end(), session.user.begin(), session.user.end());

    return restore(words, static_cast<std::uint32_t>(session.scramble.size()),
                   static_cast<std::uint32_t>(session.user.size()));
}

/** Scrambles and waits for the whole sequence to arrive on the cube. */
void arrive(std::uint32_t seed, std::uint32_t moves)
{
    REQUIRE(rubiks::app::scramble(seed, moves));
    settle();
    REQUIRE(rubiks::app::timeline_cursor() == moves);
}

/** Turns one face and waits for it to settle, the way a user's move does. */
void turn(rubiks::cube::Face face, int face_turns)
{
    REQUIRE(rubiks::app::turn_face(face, face_turns));
    settle();
}

/** R packed, which is a word every payload here can carry. */
constexpr std::uint32_t kPackedR = 0x44;

}  // namespace

TEST_CASE("a shared session comes back as the same cube")
{
    Session shared;
    std::vector<std::uint8_t> sent;

    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

        arrive(101U, 6U);
        turn(rubiks::cube::Face::Right, 1);
        turn(rubiks::cube::Face::Up, -1);
        turn(rubiks::cube::Face::Front, 2);

        shared = current_session();
        sent = drawn_frame();
    }

    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);
    REQUIRE(restore(shared));

    // The cube is there at once, without a frame being run for it.
    REQUIRE_FALSE(rubiks::app::is_busy());
    REQUIRE(drawn_frame() == sent);

    // And so are both stretches of the record, word for word. What is checked
    // is the state and the applied moves rather than the whole history: a
    // rewound tail is deliberately not carried, so the two records are not
    // required to be equal.
    REQUIRE(rubiks::app::timeline_scramble_end() == shared.scramble.size());
    REQUIRE(rubiks::app::timeline_length() ==
            shared.scramble.size() + shared.user.size());
    REQUIRE(rubiks::app::timeline_cursor() == rubiks::app::timeline_length());

    const Session round_trip = current_session();
    REQUIRE(round_trip.scramble == shared.scramble);
    REQUIRE(round_trip.user == shared.user);

    // Derived rather than set: nothing in the restore writes this number, and
    // it is the length of the payload's user stretch because the record is.
    REQUIRE(rubiks::app::committed_move_count() == shared.user.size());
}

TEST_CASE("a scramble cut short by a move is shared exactly as it stands")
{
    Session shared;
    std::vector<std::uint8_t> sent;
    std::uint32_t cut_at = 0;

    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

        arrive(103U, 8U);
        REQUIRE(rubiks::app::solve_rewind());

        // Broken off inside the scramble, which is where the boundary moves.
        for (int frame = 0; frame < 40; ++frame) {
            static_cast<void>(rubiks::app::advance(rubiks::test::kFrameMs));
        }
        rubiks::app::stop_playback();

        cut_at = rubiks::app::timeline_cursor();
        REQUIRE(cut_at > 0);
        REQUIRE(cut_at < 8);

        // A move here cuts the scramble down to the cube, which is a shape
        // ordinary play reaches and a share has to carry.
        turn(rubiks::cube::Face::Down, 1);
        REQUIRE(rubiks::app::timeline_scramble_end() == cut_at);

        shared = current_session();
        sent = drawn_frame();
    }

    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);
    REQUIRE(restore(shared));

    REQUIRE(rubiks::app::timeline_scramble_end() == cut_at);
    REQUIRE(rubiks::app::timeline_length() == cut_at + 1);
    REQUIRE(drawn_frame() == sent);
}

TEST_CASE("a session with no scramble at all is shared and restored")
{
    Session shared;
    std::vector<std::uint8_t> sent;

    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

        // Turned straight from a solved cube: nothing was handed out, so the
        // scramble stretch is empty and every move is the user's own.
        turn(rubiks::cube::Face::Right, 1);
        turn(rubiks::cube::Face::Up, 1);

        shared = current_session();
        REQUIRE(shared.scramble.empty());
        REQUIRE(shared.user.size() == 2);
        sent = drawn_frame();
    }

    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);
    REQUIRE(restore(shared));

    REQUIRE(rubiks::app::timeline_scramble_end() == 0);
    REQUIRE(rubiks::app::timeline_cursor() == 2);
    REQUIRE(rubiks::app::committed_move_count() == 2);
    REQUIRE(drawn_frame() == sent);

    // Every one of them is the user's own, so all of them can be taken back.
    REQUIRE(rubiks::app::undo());
    settle();
    REQUIRE(rubiks::app::undo());
    settle();
    REQUIRE(rubiks::app::is_solved());
}

TEST_CASE("what a rewound tail leaves out is what the sender took back")
{
    Session shared;

    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

        arrive(107U, 4U);
        turn(rubiks::cube::Face::Right, 1);
        turn(rubiks::cube::Face::Up, 1);
        turn(rubiks::cube::Face::Front, 1);

        // Two taken back, so the record holds seven moves and the cube six.
        REQUIRE(rubiks::app::undo());
        settle();
        REQUIRE(rubiks::app::undo());
        settle();
        REQUIRE(rubiks::app::timeline_length() == 7);
        REQUIRE(rubiks::app::timeline_cursor() == 5);

        shared = current_session();
        REQUIRE(shared.user.size() == 1);
    }

    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);
    REQUIRE(restore(shared));

    // The tail is not there to be put back: what was shared is a cube, and the
    // moves its sender had already withdrawn are not part of one.
    REQUIRE(rubiks::app::timeline_length() == 5);
    REQUIRE_FALSE(rubiks::app::redo());

    // Inside what was carried, both commands work as they did: the one move of
    // the user's own comes back off...
    REQUIRE(rubiks::app::undo());
    settle();
    REQUIRE(rubiks::app::timeline_cursor() == 4);
    REQUIRE_FALSE(rubiks::app::undo());

    // ...and a solve reaches through the scramble, which a redo then rebuilds.
    REQUIRE(rubiks::app::solve_rewind());
    settle();
    REQUIRE(rubiks::app::is_solved());
    REQUIRE(rubiks::app::timeline_cursor() == 0);
    REQUIRE(rubiks::app::redo());
    settle();
    REQUIRE(rubiks::app::timeline_cursor() == 1);
}

TEST_CASE("a restore is refused whole or not at all")
{
    SECTION("counts that do not add up to the buffer")
    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

        const std::vector<std::uint32_t> words{kPackedR, kPackedR, kPackedR};
        REQUIRE_FALSE(restore(words, 1, 1));
        REQUIRE(rubiks::app::timeline_length() == 0);

        // And a pair that wraps a thirty-two bit sum back onto the length,
        // which is the way a mismatch would otherwise get through on wasm32.
        REQUIRE_FALSE(restore(words, 0xffffffffU, 4));
        REQUIRE(rubiks::app::timeline_length() == 0);
    }

    SECTION("one word out of many that is not a move")
    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

        // The counts are right and every word but the last is a move, so a
        // check made as the words were read would already have applied two.
        const std::vector<std::uint32_t> words{kPackedR, kPackedR, 0};
        REQUIRE_FALSE(restore(words, 2, 1));

        REQUIRE(rubiks::app::timeline_length() == 0);
        REQUIRE(rubiks::app::timeline_cursor() == 0);
        REQUIRE(rubiks::app::is_solved());
    }

    SECTION("an axis code the cube has no axis for")
    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

        // Two bits for three axes leaves exactly one word a link can carry
        // that names nothing: the layer and the turns here are ordinary.
        const std::vector<std::uint32_t> words{kPackedR | 0x3};
        REQUIRE_FALSE(restore(words, 0, 1));
        REQUIRE(rubiks::app::timeline_length() == 0);
    }

    SECTION("a turns code that is no number of quarters")
    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

        const std::vector<std::uint32_t> words{0x4C};
        REQUIRE_FALSE(restore(words, 0, 1));
        REQUIRE(rubiks::app::timeline_length() == 0);
    }

    SECTION("a mask that turns more than one layer")
    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

        // A wide move: two layers at once, which the packing is happy to
        // carry and this cube has no way of making. Letting it in would put a
        // move on the record with no notation to write it in.
        const auto wide = rubiks::cube::pack(rubiks::cube::CubeMove{
            rubiks::cube::Axis::X, rubiks::cube::layers_through(1, 2), 1});
        REQUIRE(wide != 0);
        REQUIRE_FALSE(restore({wide}, 0, 1));
        REQUIRE(rubiks::app::timeline_length() == 0);

        // And a single layer past the edge of a cube this size.
        const auto beyond = rubiks::cube::pack(rubiks::cube::CubeMove{
            rubiks::cube::Axis::X, rubiks::cube::layer(3), 1});
        REQUIRE_FALSE(restore({beyond}, 0, 1));
        REQUIRE(rubiks::app::timeline_length() == 0);
    }
}

TEST_CASE("the restore buffer is asked for before it is read")
{
    SECTION("before there is an engine at all")
    {
        REQUIRE_FALSE(rubiks::app::is_initialized());
        REQUIRE(rubiks::app::restore_buffer(4) == 0);
        REQUIRE_FALSE(rubiks::app::restore_apply(2, 2));
    }

    SECTION("counts the buffer will not take")
    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

        REQUIRE(rubiks::app::restore_buffer(0) == 0);
        REQUIRE(rubiks::app::restore_buffer(rubiks::app::kMaxRestoreMoves + 1) ==
                0);
        REQUIRE(rubiks::app::restore_buffer(rubiks::app::kMaxRestoreMoves) != 0);
    }

    SECTION("applied without a buffer, and applied twice")
    {
        const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

        REQUIRE_FALSE(rubiks::app::restore_apply(0, 0));

        REQUIRE(restore({kPackedR}, 0, 1));
        REQUIRE(rubiks::app::timeline_length() == 1);

        // The buffer belongs to one restore, so the second call has nothing to
        // read and says so rather than applying the same record again.
        REQUIRE_FALSE(rubiks::app::restore_apply(0, 1));
        REQUIRE(rubiks::app::timeline_length() == 1);
    }
}

TEST_CASE("a restore replaces whatever the cube was doing")
{
    const rubiks::test::EngineLifecycle engine(kCanvas, kCanvas);

    arrive(109U, 5U);
    turn(rubiks::cube::Face::Right, 1);

    REQUIRE(restore({kPackedR, kPackedR}, 1, 1));

    // The old record is gone rather than added to, and nothing of it is left
    // in flight: a restore is a session arriving, not a move being made.
    REQUIRE(rubiks::app::timeline_length() == 2);
    REQUIRE(rubiks::app::timeline_scramble_end() == 1);
    REQUIRE(rubiks::app::timeline_cursor() == 2);
    REQUIRE_FALSE(rubiks::app::is_busy());

    settle();
    REQUIRE(rubiks::app::timeline_cursor() == 2);
}
