#include "graphics/Light.hpp"

#include <algorithm>
#include <cmath>

namespace rubiks::graphics {

Lighting Lighting::standard()
{
    Lighting lighting;
    // The key: high, in front and a little to the left of the home eye, so
    // the top face is brightest and the front brighter than the right, and
    // the shadow it casts stays near the cube's footprint. From the home eye
    // no resting face can mirror it -- a face that reflects a view from above
    // reflects it downwards -- so its glint lives on the tilted top of a
    // turning layer. The lobe is wide enough to sweep across that face as it
    // turns (peak alpha ~59 at R 45 degrees) and still under the highlight
    // threshold on every face at rest, which keeps the contract samples pure.
    lighting.lamps.push_back(Light{{2.6f, 7.0f, 4.0f}, 0.38f, 0.60f, 12.0f});
    return lighting;
}

Lighting Lighting::unlit(const Lighting& base)
{
    Lighting lighting = base;
    lighting.ambient = 1.0f;
    // Saturation off as well: the high-contrast palette's six shades were
    // measured for a purpose, and this must hand them back untouched.
    lighting.saturation = 1.0f;
    for (auto& lamp : lighting.lamps) {
        lamp.diffuse = 0.0f;
        lamp.specular = 0.0f;
    }
    return lighting;
}

bool Lighting::from_values(const float* values, std::size_t count)
{
    if (!values || !valid_count(count)) return false;
    for (std::size_t i = 0; i < count; ++i) {
        if (!std::isfinite(values[i])) return false;
    }

    ambient = values[0];
    attenuation = values[1];
    saturation = values[2];
    lamps.clear();
    const std::size_t lamp_count = (count - kHeaderCount) / kLampValueCount;
    for (std::size_t i = 0; i < lamp_count; ++i) {
        const float* v = values + kHeaderCount + i * kLampValueCount;
        lamps.push_back(Light{{v[0], v[1], v[2]}, v[3], v[4], v[5]});
    }
    return true;
}

void Lighting::to_values(float* values) const
{
    values[0] = ambient;
    values[1] = attenuation;
    values[2] = saturation;
    for (std::size_t i = 0; i < lamps.size(); ++i) {
        float* v = values + kHeaderCount + i * kLampValueCount;
        const Light& lamp = lamps[i];
        v[0] = lamp.position.x;
        v[1] = lamp.position.y;
        v[2] = lamp.position.z;
        v[3] = lamp.diffuse;
        v[4] = lamp.specular;
        v[5] = lamp.shininess;
    }
}

Color saturate(const Color& color, int percent) noexcept
{
    const int gray = (2126 * color.r + 7152 * color.g + 722 * color.b + 5000) / 10000;
    const auto channel = [gray, percent](std::uint8_t value) {
        // Rounded towards the nearest, in the direction of the difference,
        // so a channel above grey and one below move symmetrically.
        const int offset = (int{value} - gray) * percent;
        const int rounded = offset >= 0 ? (offset + 50) / 100 : -((-offset + 50) / 100);
        return static_cast<std::uint8_t>(std::clamp(gray + rounded, 0, 255));
    };
    return Color{channel(color.r), channel(color.g), channel(color.b), color.a};
}

const Lighting& default_lighting()
{
    static const Lighting lighting = Lighting::standard();
    return lighting;
}

Color shade(const Color& color, float intensity) noexcept
{
    const auto scale = static_cast<std::uint32_t>(
        std::lround(std::max(intensity, 0.0f) * 255.0f));

    const auto channel = [scale](std::uint8_t value) {
        // Rounded rather than truncated, and written so the contract can copy
        // it: min(255, (value * scale + 127) / 255), with scale 255 giving
        // value back.
        return static_cast<std::uint8_t>(
            std::min<std::uint32_t>(255U, (value * scale + 127U) / 255U));
    };

    return Color{channel(color.r), channel(color.g), channel(color.b), color.a};
}

}  // namespace rubiks::graphics
