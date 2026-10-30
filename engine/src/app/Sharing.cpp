#include "app/Application.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "app/ApplicationState.hpp"
#include "cube/Cube.hpp"
#include "cube/CubeMove.hpp"
#include "cube/Face.hpp"
#include "cube/MoveTimeline.hpp"
#include "cube/PackedMove.hpp"

namespace rubiks::app {

using namespace detail;

namespace {

/**
 * The moves the words of a shared record stand for, on a cube of `size`.
 *
 * Nothing when any one of them is not a move: a word that unpacks to none, or
 * one whose layers this application could not have turned. A record is taken
 * whole or not at all, so one such word refuses all of it.
 */
[[nodiscard]] std::optional<std::vector<cube::CubeMove>> unpacked_moves(
    const std::vector<std::uint32_t>& words, int size)
{
    std::vector<cube::CubeMove> moves;
    moves.reserve(words.size());
    for (const std::uint32_t word : words) {
        const auto move = cube::unpack(word);
        if (!move || !turnable_layers(move->layers, size)) return std::nullopt;
        moves.push_back(*move);
    }
    return moves;
}

}  // namespace

std::uintptr_t restore_buffer(std::uint32_t total_count) noexcept
{
    if (!state) return 0;

    // Nothing to restore is not a shorter restore: the caller has decoded a
    // record and is here because it holds something, so an empty one is a
    // fault rather than a case -- and refusing it is what keeps zero meaning
    // "refused" for the address as well.
    if (total_count == 0 || total_count > kMaxRestoreMoves) return 0;

    state->restore.assign(total_count, 0);
    return reinterpret_cast<std::uintptr_t>(state->restore.data());
}

bool restore_apply(int size, std::uint32_t scramble_count,
                   std::uint32_t user_count) noexcept
{
    if (!state) return false;
    if (!valid_cube_size(size)) return false;

    // Taken out before it is read, whichever way this goes: the buffer belongs
    // to one restore, so a second call with nothing written into it is refused
    // by the same emptiness that refuses a call without a buffer at all.
    std::vector<std::uint32_t> words;
    words.swap(state->restore);
    if (words.empty()) return false;

    // Widened before it is added. On wasm32 a size_t is thirty-two bits, so
    // two counts that wrap would agree with the buffer's length while one of
    // the two loops below walked off the end of it.
    const std::uint64_t declared =
        static_cast<std::uint64_t>(scramble_count) + user_count;
    if (declared != words.size()) return false;

    const auto moves = unpacked_moves(words, size);
    if (!moves) return false;

    // Past every refusal, so what follows cannot leave the cube half restored
    // -- the size included, which is why it arrives with the record rather
    // than through a call of its own before it.
    install_cube(cube::Cube(size));

    // Built through the ordinary operations rather than through a way in of
    // its own: the scramble is begun and then walked up, and the user's moves
    // are recorded on top of it, which is exactly what happened to the cube
    // this record came from.
    const auto scramble_end = static_cast<std::size_t>(scramble_count);
    state->timeline.begin_scramble(std::vector<cube::CubeMove>(
        moves->begin(),
        moves->begin() + static_cast<std::ptrdiff_t>(scramble_end)));

    for (std::size_t index = 0; index < moves->size(); ++index) {
        const cube::CubeMove& move = (*moves)[index];
        state->cube_state.apply(move);
        if (index < scramble_end) {
            state->timeline.step(cube::TimelineEffect::Advance);
        } else {
            state->timeline.record(move);
        }
    }

    return true;
}

std::uint32_t origin_painting_count() noexcept
{
    if (!state) return 0;
    return static_cast<std::uint32_t>(state->origin.size());
}

int origin_painting_at(std::uint32_t index) noexcept
{
    if (!state || index >= state->origin.size()) return -1;
    return static_cast<int>(state->origin[index]);
}

std::uintptr_t painting_buffer(std::uint32_t count) noexcept
{
    if (!state) return 0;

    // The one count a cube of some size this application builds could have.
    // Refusing anything else is what keeps zero meaning "refused" for the
    // address as well.
    bool fits = false;
    for (int size = kMinCubeSize; size <= kMaxCubeSize; ++size) {
        if (count == static_cast<std::uint32_t>(6 * size * size)) fits = true;
    }
    if (!fits) return 0;

    state->painting_bytes.assign(count, 0);
    return reinterpret_cast<std::uintptr_t>(state->painting_bytes.data());
}

bool restore_painting(int size, std::uint32_t user_count) noexcept
{
    if (!state) return false;
    if (!valid_cube_size(size)) return false;

    // Both buffers are taken out before either is read, whichever way this
    // goes: they belong to one restore, so a second call with nothing written
    // into them is refused by the same emptiness that refuses a call without
    // them at all.
    std::vector<std::uint8_t> colours;
    colours.swap(state->painting_bytes);
    std::vector<std::uint32_t> words;
    words.swap(state->restore);

    if (colours.size() != static_cast<std::size_t>(6 * size * size)) {
        return false;
    }
    if (words.size() != user_count) return false;

    std::vector<cube::FaceColor> painting;
    painting.reserve(colours.size());
    for (const std::uint8_t colour : colours) {
        const auto named = cube::face_color_from(colour);
        if (!named) return false;
        painting.push_back(*named);
    }

    const auto moves = unpacked_moves(words, size);
    if (!moves) return false;

    // The last refusal, and the one this whole feature exists for: a colouring
    // that no turning reaches is not a cube, and a solver handed one would
    // look for a position that does not exist.
    auto built = cube::assembled(size, painting);
    if (!built) return false;

    // Past every refusal, so what follows cannot leave the cube half restored.
    install_cube(std::move(*built), std::move(painting));

    // The colouring is the starting position, so what follows it is the user's
    // own -- recorded the ordinary way, which is what happened to the cube
    // this link came from.
    for (const auto& move : *moves) {
        state->cube_state.apply(move);
        state->timeline.record(move);
    }
    return true;
}

}  // namespace rubiks::app
