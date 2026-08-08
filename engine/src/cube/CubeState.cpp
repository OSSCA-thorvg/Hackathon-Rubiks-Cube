#include "cube/CubeState.hpp"

#include <array>
#include <utility>

namespace rubiks::cube {
namespace {

struct Position {
    int x;
    int y;
    int z;
};

/**
 * Where a cubie lands after one positive quarter turn about `axis`.
 *
 * These three mappings, together with the sticker cycles below, are the whole
 * move engine. They are verified against a physical cube by the known-answer
 * test: on a solved 3x3x3, R sends the UFR cubie to the UBR slot showing
 * green on top and white at the back.
 */
[[nodiscard]] Position rotated(Axis axis, const Position& p, int last) noexcept
{
    switch (axis) {
        case Axis::X:
            return Position{p.x, p.z, last - p.y};
        case Axis::Y:
            return Position{last - p.z, p.y, p.x};
        case Axis::Z:
            break;
    }
    return Position{p.y, last - p.x, p.z};
}

/**
 * The faces a positive quarter turn cycles, each entry moving to the next.
 *
 * The two faces perpendicular to the axis keep their color, which is why they
 * are absent here.
 */
[[nodiscard]] std::array<Face, 4> sticker_cycle(Axis axis) noexcept
{
    switch (axis) {
        case Axis::X:
            return {Face::Up, Face::Back, Face::Down, Face::Front};
        case Axis::Y:
            return {Face::Front, Face::Left, Face::Back, Face::Right};
        case Axis::Z:
            break;
    }
    return {Face::Up, Face::Right, Face::Down, Face::Left};
}

[[nodiscard]] Cubie rotated(Axis axis, const Cubie& cubie) noexcept
{
    Cubie turned = cubie;
    const auto cycle = sticker_cycle(axis);

    for (std::size_t i = 0; i < cycle.size(); ++i) {
        turned.stickers[face_index(cycle[(i + 1) % cycle.size()])] =
            cubie.sticker(cycle[i]);
    }
    return turned;
}

[[nodiscard]] int coordinate_on(Axis axis, const Position& p) noexcept
{
    switch (axis) {
        case Axis::X:
            return p.x;
        case Axis::Y:
            return p.y;
        case Axis::Z:
            break;
    }
    return p.z;
}

/** Turn count in 0 ... 3; a move of any integer size is well defined. */
[[nodiscard]] int normalized_turns(int quarter_turns) noexcept
{
    return ((quarter_turns % 4) + 4) % 4;
}

}  // namespace

Cubie solved_cubie() noexcept
{
    Cubie cubie;
    for (std::size_t i = 0; i < kFaceCount; ++i) {
        cubie.stickers[i] = solved_color(static_cast<Face>(i));
    }
    return cubie;
}

CubeState::CubeState(int size)
    : size_(size),
      cubies_(static_cast<std::size_t>(size) * static_cast<std::size_t>(size) *
                  static_cast<std::size_t>(size),
              solved_cubie())
{
}

std::size_t CubeState::index(int x, int y, int z) const noexcept
{
    return static_cast<std::size_t>((x * size_ + y) * size_ + z);
}

const Cubie& CubeState::at(int x, int y, int z) const noexcept
{
    return cubies_[index(x, y, z)];
}

bool CubeState::is_solved() const noexcept
{
    const int last = size_ - 1;

    for (int a = 0; a < size_; ++a) {
        for (int b = 0; b < size_; ++b) {
            if (at(last, a, b).sticker(Face::Right) !=
                    solved_color(Face::Right) ||
                at(0, a, b).sticker(Face::Left) !=
                    solved_color(Face::Left) ||
                at(a, last, b).sticker(Face::Up) !=
                    solved_color(Face::Up) ||
                at(a, 0, b).sticker(Face::Down) !=
                    solved_color(Face::Down) ||
                at(a, b, last).sticker(Face::Front) !=
                    solved_color(Face::Front) ||
                at(a, b, 0).sticker(Face::Back) !=
                    solved_color(Face::Back)) {
                return false;
            }
        }
    }

    return true;
}

void CubeState::rotate_quarter(Axis axis, LayerMask layers) noexcept
{
    // Cubies outside the turning layers keep their slot, so the copy starts
    // from the current state and only the moving ones are overwritten.
    std::vector<Cubie> next = cubies_;
    const int last = size_ - 1;

    for (int x = 0; x < size_; ++x) {
        for (int y = 0; y < size_; ++y) {
            for (int z = 0; z < size_; ++z) {
                const Position from{x, y, z};
                const auto bit = layer(coordinate_on(axis, from));
                if ((layers & bit) == 0) continue;

                const auto to = rotated(axis, from, last);
                next[index(to.x, to.y, to.z)] =
                    rotated(axis, cubies_[index(x, y, z)]);
            }
        }
    }
    cubies_ = std::move(next);
}

void CubeState::apply(const CubeMove& move) noexcept
{
    const int turns = normalized_turns(move.quarter_turns);
    for (int i = 0; i < turns; ++i) {
        rotate_quarter(move.axis, move.layers);
    }
}

void CubeState::apply(const std::vector<CubeMove>& sequence) noexcept
{
    for (const auto& move : sequence) {
        apply(move);
    }
}

bool operator==(const CubeState& a, const CubeState& b) noexcept
{
    return a.size_ == b.size_ && a.cubies_ == b.cubies_;
}

bool operator!=(const CubeState& a, const CubeState& b) noexcept
{
    return !(a == b);
}

}  // namespace rubiks::cube
