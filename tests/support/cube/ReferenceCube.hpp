#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "cube/CubeMove.hpp"
#include "cube/Surface.hpp"

namespace rubiks::test {

/**
 * The former N cubed storage, frozen at 267de6c as an independent move oracle.
 *
 * Only the public move, face and colour vocabulary is shared with production.
 * Position formulas, face permutations and internal six-colour pieces are
 * literal copies of the former rules: a mistake in the new surface mapping
 * must not silently change the expected answer along with the implementation.
 */
class ReferenceCube {
public:
    /** Builds the former solved grid, including its unexposed colours. */
    explicit ReferenceCube(int size)
        : size_(size),
          pieces_(static_cast<std::size_t>(size) *
                      static_cast<std::size_t>(size) *
                      static_cast<std::size_t>(size),
                  Colours{cube::FaceColor::Red, cube::FaceColor::Orange,
                          cube::FaceColor::White, cube::FaceColor::Yellow,
                          cube::FaceColor::Green, cube::FaceColor::Blue})
    {
    }

    /** The number of slots along each of the former grid's axes. */
    [[nodiscard]] int size() const noexcept { return size_; }

    /** Reads a named surface colour without using production index formulas. */
    [[nodiscard]] cube::FaceColor sticker(
        const cube::SurfaceSticker& sticker) const noexcept
    {
        return pieces_[index(sticker.x, sticker.y, sticker.z)]
                      [static_cast<std::size_t>(sticker.face)];
    }

    /** Turns the selected grid slices, with the original modulo-four rule. */
    void apply(const cube::CubeMove& move)
    {
        const int turns = ((move.quarter_turns % 4) + 4) % 4;
        for (int turn = 0; turn < turns; ++turn) {
            rotate_quarter(move.axis, move.layers);
        }
    }

private:
    /** The former piece's six directional colours, including hidden ones. */
    using Colours = std::array<cube::FaceColor, 6>;

    /** A grid coordinate, independent of production surface identities. */
    struct Position {
        int x;
        int y;
        int z;
    };

    /** Flattens the former x-major N cubed grid. */
    [[nodiscard]] std::size_t index(int x, int y, int z) const noexcept
    {
        return static_cast<std::size_t>((x * size_ + y) * size_ + z);
    }

    /** Reads the layer coordinate using the original three literal choices. */
    [[nodiscard]] static int coordinate(cube::Axis axis,
                                        const Position& position) noexcept
    {
        switch (axis) {
            case cube::Axis::X:
                return position.x;
            case cube::Axis::Y:
                return position.y;
            case cube::Axis::Z:
                return position.z;
        }
        return position.z;
    }

    /** Maps a slot using the former clockwise quarter-turn formulas. */
    [[nodiscard]] Position turned(cube::Axis axis,
                                   const Position& position) const noexcept
    {
        const int last = size_ - 1;
        switch (axis) {
            case cube::Axis::X:
                return Position{position.x, position.z, last - position.y};
            case cube::Axis::Y:
                return Position{last - position.z, position.y, position.x};
            case cube::Axis::Z:
                return Position{position.y, last - position.x, position.z};
        }
        return position;
    }

    /** Permutes six colours by the old directional cycles, written literally. */
    [[nodiscard]] static Colours rotated(cube::Axis axis,
                                         const Colours& colours) noexcept
    {
        switch (axis) {
            case cube::Axis::X:
                return Colours{colours[0], colours[1], colours[4], colours[5],
                               colours[3], colours[2]};
            case cube::Axis::Y:
                return Colours{colours[5], colours[4], colours[2], colours[3],
                               colours[0], colours[1]};
            case cube::Axis::Z:
                return Colours{colours[2], colours[3], colours[1], colours[0],
                               colours[4], colours[5]};
        }
        return colours;
    }

    /** Carries every selected grid slot and its six colours to its new slot. */
    void rotate_quarter(cube::Axis axis, cube::LayerMask layers)
    {
        std::vector<Colours> next = pieces_;
        for (int x = 0; x < size_; ++x) {
            for (int y = 0; y < size_; ++y) {
                for (int z = 0; z < size_; ++z) {
                    const Position from{x, y, z};
                    const cube::LayerMask bit = cube::LayerMask{1}
                        << coordinate(axis, from);
                    if ((layers & bit) == 0) continue;
                    const Position to = turned(axis, from);
                    next[index(to.x, to.y, to.z)] =
                        rotated(axis, pieces_[index(x, y, z)]);
                }
            }
        }
        pieces_.swap(next);
    }

    int size_;
    std::vector<Colours> pieces_;
};

}  // namespace rubiks::test
