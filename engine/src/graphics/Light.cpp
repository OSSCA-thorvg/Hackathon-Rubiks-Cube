#include "graphics/Light.hpp"

#include <algorithm>
#include <cmath>

namespace rubiks::graphics {

Color shade(const Color& color, float intensity) noexcept
{
    const auto scale = static_cast<std::uint32_t>(
        std::lround(std::clamp(intensity, 0.0f, 1.0f) * 255.0f));

    const auto channel = [scale](std::uint8_t value) {
        // Rounded rather than truncated, and written so the contract can copy
        // it: (value * scale + 127) / 255, with scale 255 giving value back.
        return static_cast<std::uint8_t>((value * scale + 127U) / 255U);
    };

    return Color{channel(color.r), channel(color.g), channel(color.b), color.a};
}

}  // namespace rubiks::graphics
