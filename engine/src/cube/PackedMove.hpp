#pragma once

#include <cstdint>

#include "cube/CubeMove.hpp"

namespace rubiks::cube {

/**
 * How one move crosses a boundary that carries nothing but numbers.
 *
 * ```text
 * bits 0-1   axis        0 = X, 1 = Y, 2 = Z
 * bits 2-3   turns code  0 = -1, 1 = +1, 2 = +2
 * bits 4-31  layer mask  bit 4 is layer 0, up to 28 layers
 * ```
 *
 * A move is packed rather than named because notation is a way of writing a
 * move down and not a property of it: the reader assembles `R'` or `M2` from
 * these numbers, and a different way of writing the same move never reaches
 * this side. So no string crosses the boundary, and the C ABI keeps to
 * primitives.
 *
 * A layer set is never empty, so a packed move can never be zero -- which is
 * what makes 0 the one word for "there is no move here" and saves the reader a
 * second query to ask whether an index exists.
 */
inline constexpr std::uint32_t kPackedAxisShift = 0;
inline constexpr std::uint32_t kPackedAxisMask = 0x3;
inline constexpr std::uint32_t kPackedTurnsShift = 2;
inline constexpr std::uint32_t kPackedTurnsMask = 0x3;
inline constexpr std::uint32_t kPackedLayerShift = 4;

/** How many layers the mask field holds; a wider cube cannot be packed. */
inline constexpr int kPackedMaxLayers = 28;

/**
 * The three turns a quarter-turn count is written as.
 *
 * Two codes rather than four because a turn and the same turn plus a full
 * revolution are one move to a reader, and because the two ways round to a
 * half turn are the same move as well.
 */
inline constexpr std::uint32_t kPackedTurnsCounterClockwise = 0;
inline constexpr std::uint32_t kPackedTurnsClockwise = 1;
inline constexpr std::uint32_t kPackedTurnsHalf = 2;

/**
 * Writes a move as one unsigned word, normalizing its turns on the way.
 *
 * Normalizing here and nowhere else is the whole of the arrangement: a record
 * of what happened keeps the turns it was played with, because a replay of
 * `-2` has to turn back the way it went, while a reader of the record cares
 * only which cube comes out and would have to undo the distinction anyway.
 * This function is on the reader's path alone, so the two never meet.
 *
 * @return zero for a move nothing can be written for: a turn of no quarters,
 *         an empty layer set, or layers past what the field holds.
 */
[[nodiscard]] constexpr std::uint32_t pack(const CubeMove& move) noexcept
{
    // Modulo with the sign folded away, so every integer a move may hold ends
    // up as one of the three codes.
    const int turns = ((move.quarter_turns % 4) + 4) % 4;
    if (turns == 0 || move.layers == 0) return 0;
    if ((move.layers >> kPackedMaxLayers) != 0) return 0;

    const std::uint32_t code = turns == 1   ? kPackedTurnsClockwise
                               : turns == 2 ? kPackedTurnsHalf
                                            : kPackedTurnsCounterClockwise;

    return ((static_cast<std::uint32_t>(move.axis) & kPackedAxisMask)
            << kPackedAxisShift) |
           ((code & kPackedTurnsMask) << kPackedTurnsShift) |
           (move.layers << kPackedLayerShift);
}

}  // namespace rubiks::cube
