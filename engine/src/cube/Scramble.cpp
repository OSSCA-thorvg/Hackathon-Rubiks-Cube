#include "cube/Scramble.hpp"

#include <array>
#include <optional>

namespace rubiks::cube {
namespace {

/** A small, fully specified 32-bit generator with native/WASM parity. */
class XorShift32 {
public:
    explicit XorShift32(std::uint32_t seed) noexcept
        : state_(seed == 0 ? kZeroSeedReplacement : seed)
    {
    }

    /** Returns the next value in the xorshift32 sequence. */
    [[nodiscard]] std::uint32_t next() noexcept
    {
        std::uint32_t value = state_;
        value ^= value << 13U;
        value ^= value >> 17U;
        value ^= value << 5U;
        state_ = value;
        return value;
    }

    /** Returns a value in [0, bound); every call advances exactly once. */
    [[nodiscard]] std::uint32_t below(std::uint32_t bound) noexcept
    {
        return next() % bound;
    }

private:
    static constexpr std::uint32_t kZeroSeedReplacement = 0x6d2b79f5U;
    std::uint32_t state_;
};

/** Chooses one of the two axes different from `previous`. */
[[nodiscard]] Axis next_axis(XorShift32& random,
                             std::optional<Axis> previous) noexcept
{
    constexpr std::array<Axis, 3> kAxes{Axis::X, Axis::Y, Axis::Z};
    if (!previous) return kAxes[random.below(3)];

    std::array<Axis, 2> choices{};
    std::size_t count = 0;
    for (const Axis axis : kAxes) {
        if (axis != *previous) choices[count++] = axis;
    }
    return choices[random.below(2)];
}

/** Maps a stable random index to one quarter-turn amount. */
[[nodiscard]] int turn_from(std::uint32_t index) noexcept
{
    constexpr std::array<int, 3> kTurns{-1, 1, 2};
    return kTurns[index];
}

}  // namespace

std::vector<CubeMove> make_scramble(int size, std::uint32_t seed,
                                    std::size_t move_count)
{
    std::vector<CubeMove> sequence;
    if (size <= 0 || move_count == 0) return sequence;

    sequence.reserve(move_count);
    XorShift32 random(seed);
    std::optional<Axis> previous;

    for (std::size_t index = 0; index < move_count; ++index) {
        const Axis axis = next_axis(random, previous);
        const int layer_index = random.below(2) == 0 ? 0 : size - 1;
        const int quarter_turns = turn_from(random.below(3));

        sequence.push_back(
            CubeMove{axis, layer(layer_index), quarter_turns});
        previous = axis;
    }

    return sequence;
}

}  // namespace rubiks::cube
