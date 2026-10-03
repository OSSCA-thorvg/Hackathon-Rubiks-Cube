#pragma once

#include <array>
#include <cstddef>
#include <optional>

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

/**
 * The face an integer names, or nothing when it names none.
 *
 * For a value that arrives as a primitive, across the C ABI or out of a
 * buffer, and for a caller that was handed an enum it cannot vouch for. This
 * is the one place the question is answered for this enum.
 *
 * The switch has no default on purpose: a face added to the enum and not to
 * the switch is a warning from the compiler rather than a value refused in
 * silence. The comparison before it refuses an integer the cast could not
 * hold.
 */
[[nodiscard]] constexpr std::optional<Face> face_from(int value) noexcept
{
    const auto named = static_cast<Face>(value);
    if (static_cast<int>(named) != value) return std::nullopt;

    switch (named) {
        case Face::Right:
        case Face::Left:
        case Face::Up:
        case Face::Down:
        case Face::Front:
        case Face::Back:
            return named;
    }
    return std::nullopt;
}

/** The six faces, in the order the enum declares them. */
[[nodiscard]] constexpr std::array<Face, kFaceCount> faces() noexcept
{
    return {Face::Right, Face::Left,  Face::Up,
            Face::Down,  Face::Front, Face::Back};
}

enum class FaceColor { Red, Orange, White, Yellow, Green, Blue };

/** The colour an integer names, or nothing; see face_from(). */
[[nodiscard]] constexpr std::optional<FaceColor> face_color_from(
    int value) noexcept
{
    const auto named = static_cast<FaceColor>(value);
    if (static_cast<int>(named) != value) return std::nullopt;

    switch (named) {
        case FaceColor::Red:
        case FaceColor::Orange:
        case FaceColor::White:
        case FaceColor::Yellow:
        case FaceColor::Green:
        case FaceColor::Blue:
            return named;
    }
    return std::nullopt;
}

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

}  // namespace rubiks::cube
