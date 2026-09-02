#include "graphics/Light.hpp"

#include <algorithm>
#include <cmath>

namespace rubiks::graphics {

Lighting Lighting::standard()
{
    Lighting lighting;
    // The key: high, in front and a little to the left of the home eye, so
    // the top face is brightest and the front brighter than the right, and
    // the shadow it casts stays near the cube's footprint.
    // Its glint is kept tight so a lamp this far away does not glaze a whole
    // face: a distant lamp's half-vector barely turns across two units.
    lighting.lamps.push_back(Light{{2.6f, 7.0f, 4.0f}, 0.38f, 0.40f, 24.0f});
    // The kicker: close above the cube, so its reflection is a spot rather
    // than a glaze, landing just off the top face's corner nearest the home
    // eye. Specular only -- it is there to be seen in the surface, not to
    // light it, and so the render contract's bytes are the key's alone.
    lighting.lamps.push_back(Light{{0.0f, 2.4f, 0.0f}, 0.0f, 0.55f, 40.0f});
    return lighting;
}

Lighting Lighting::unlit(const Lighting& base)
{
    Lighting lighting = base;
    lighting.ambient = 1.0f;
    for (auto& lamp : lighting.lamps) {
        lamp.diffuse = 0.0f;
        lamp.specular = 0.0f;
    }
    return lighting;
}

bool Lighting::from_values(const float* values, std::size_t count)
{
    if (!values || count != kValueCount) return false;
    for (std::size_t i = 0; i < count; ++i) {
        if (!std::isfinite(values[i])) return false;
    }

    ambient = values[0];
    attenuation = values[1];
    lamps.clear();
    for (std::size_t i = 0; i < kLampCount; ++i) {
        const float* v = values + 2 + i * 6;
        lamps.push_back(Light{{v[0], v[1], v[2]}, v[3], v[4], v[5]});
    }
    return true;
}

void Lighting::to_values(float* values) const
{
    values[0] = ambient;
    values[1] = attenuation;
    for (std::size_t i = 0; i < kLampCount; ++i) {
        float* v = values + 2 + i * 6;
        const Light lamp = i < lamps.size() ? lamps[i] : Light{};
        v[0] = lamp.position.x;
        v[1] = lamp.position.y;
        v[2] = lamp.position.z;
        v[3] = lamp.diffuse;
        v[4] = lamp.specular;
        v[5] = lamp.shininess;
    }
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
