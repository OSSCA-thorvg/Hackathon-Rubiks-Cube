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
    if (size < 2 || move_count == 0) return sequence;

    // Half the cube, rounded down: a wide move deeper than that is the same
    // layers turned from the other side, and on an odd cube the middle slice
    // is reached from either end. So this is every distinct move there is.
    const int deepest = size / 2;

    sequence.reserve(move_count);
    XorShift32 random(seed);
    std::optional<Axis> previous;

    for (std::size_t index = 0; index < move_count; ++index) {
        const Axis axis = next_axis(random, previous);
        const bool from_far_end = random.below(2) != 0;

        // Not drawn at all when there is nothing to choose between. A 3x3 has
        // one depth, so its sequences stay exactly the ones this generator has
        // always produced for a seed -- the draw that never had an answer is
        // the draw that must not consume one.
        const int depth = deepest == 1
                              ? 1
                              : 1 + static_cast<int>(random.below(
                                        static_cast<std::uint32_t>(deepest)));

        const int quarter_turns = turn_from(random.below(3));

        const LayerMask layers =
            from_far_end ? layers_through(size - depth, size - 1)
                         : layers_through(0, depth - 1);

        sequence.push_back(CubeMove{axis, layers, quarter_turns});
        previous = axis;
    }

    return sequence;
}

}  // namespace rubiks::cube
