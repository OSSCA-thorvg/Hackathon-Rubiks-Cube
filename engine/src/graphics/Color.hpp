#pragma once

#include <cstdint>

namespace rubiks::graphics {

/** Opaque-capable RGBA color with 8-bit unpremultiplied channels. */
struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

[[nodiscard]] constexpr bool operator==(const Color& lhs,
                                        const Color& rhs) noexcept
{
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

[[nodiscard]] constexpr bool operator!=(const Color& lhs,
                                        const Color& rhs) noexcept
{
    return !(lhs == rhs);
}

}  // namespace rubiks::graphics
