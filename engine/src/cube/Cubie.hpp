#pragma once

#include <array>
#include <cstddef>

/**
 * The Rubik's Cube domain: logical state only.
 *
 * Nothing here knows about graphics, math or ThorVG. Coordinates are integer
 * indices and rotations are color permutations, so the domain never needs a
 * float. The build enforces this — the cube target is declared with no
 * dependencies at all.
 */
namespace rubiks::cube {

/** Cube faces, in the axis order +X -X +Y -Y +Z -Z. */
enum class Face { Right, Left, Up, Down, Front, Back };

inline constexpr std::size_t kFaceCount = 6;

/** The six faces, in the order the enum declares them. */
[[nodiscard]] constexpr std::array<Face, kFaceCount> faces() noexcept
{
    return {Face::Right, Face::Left,  Face::Up,
            Face::Down,  Face::Front, Face::Back};
}

enum class FaceColor { Red, Orange, White, Yellow, Green, Blue };

[[nodiscard]] constexpr std::size_t face_index(Face face) noexcept
{
    return static_cast<std::size_t>(face);
}

/** The color a face carries while the cube is solved. */
[[nodiscard]] constexpr FaceColor solved_color(Face face) noexcept
{
    // Face and FaceColor are declared in the same order on purpose: Right is
    // Red, Left is Orange, and so on down the list.
    return static_cast<FaceColor>(static_cast<int>(face));
}

/**
 * One 1x1x1 cube with an independently colored sticker per face.
 *
 * A quarter turn permutes these six entries; there is no orientation
 * quaternion, because for 90 degree steps the color permutation is exactly
 * equivalent and it answers the question rendering actually asks: what color
 * does this position show on this face.
 */
struct Cubie {
    std::array<FaceColor, kFaceCount> stickers{};

    [[nodiscard]] FaceColor sticker(Face face) const noexcept
    {
        return stickers[face_index(face)];
    }
};

[[nodiscard]] inline bool operator==(const Cubie& a, const Cubie& b) noexcept
{
    return a.stickers == b.stickers;
}

[[nodiscard]] inline bool operator!=(const Cubie& a, const Cubie& b) noexcept
{
    return !(a == b);
}

/** A cubie whose every face shows its solved color. */
[[nodiscard]] Cubie solved_cubie() noexcept;

}  // namespace rubiks::cube
