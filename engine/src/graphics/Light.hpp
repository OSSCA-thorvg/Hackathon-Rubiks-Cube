#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "graphics/Color.hpp"
#include "math/Types.hpp"

/**
 * The lights over the cube, and the ground its shadow falls on.
 *
 * Blinn-Phong with its three terms and nothing more: ambient so that no face
 * ever goes black, diffuse for the face-by-face brightness, specular for the
 * glint a face throws when it sits between a light and the eye. There is no
 * reflection term because Blinn-Phong has none.
 *
 * One lamp by default, the key: high in front for the diffuse shading and a
 * tight glint. A second lamp, a kicker for the glint alone, was tried and
 * taken out -- a distant one glazes a whole face, a near one puts a spot on
 * one face and nothing on the others, and neither read as a cube.
 *
 * The lamps are points rather than directions on purpose. A direction only
 * ever slides a shadow sideways and lights a flat face evenly; a point
 * stretches and leans the shadow, and lights the near end of a face more than
 * the far end, which is what makes a light visible as a light.
 */
namespace rubiks::graphics {

/** One lamp. World space; each pass carries it into its own space itself. */
struct Light {
    math::Vec3 position{0.0f, 0.0f, 0.0f};
    /** Strength of its diffuse term at the cube's centre. */
    float diffuse = 0.0f;
    /** Peak strength of its glint and how tightly the glint is focused. */
    float specular = 0.0f;
    float shininess = 16.0f;
};

struct Lighting {
    /**
     * Floor of the brightness. Kept high because a sticker's colour is its
     * identity: a red that shades into an orange is not lighting but a
     * mistake. The palette colour is the colour of a well-lit face, so the
     * sum of ambient and a lamp's diffuse may pass 1; shade() clamps.
     */
    float ambient = 0.75f;

    /**
     * Exponent on the distance falloff of every lamp's diffuse term:
     * (distance to the cube's centre / distance to the point) to this power,
     * so a point at the centre's distance is lit at full strength and the
     * near end of a face is lit more than the far end. Zero switches it off.
     */
    float attenuation = 1.0f;

    /**
     * Saturation of the stickers before they are lit, as a factor; 1 leaves
     * them alone. Multiplying a colour down for shade lowers its lightness,
     * which the eye reads as washing out, and a little more chroma up front
     * gives that back. Not applied to the cubie body, which is plastic.
     */
    float saturation = 1.25f;

    /** The key light first: it alone casts the shadow. */
    std::vector<Light> lamps;

    /** The arrangement the app is drawn under. */
    [[nodiscard]] static Lighting standard();

    /**
     * The same lamps with every term switched off but the ambient at 1, and
     * the saturation at 1, so every colour comes back as it is, byte for byte,
     * while the key still casts its shadow. What the high-contrast palette is
     * drawn under.
     */
    [[nodiscard]] static Lighting unlit(const Lighting& base);

    /**
     * Flat list form for tuning by eye: ambient, attenuation, saturation,
     * then for each lamp x, y, z, diffuse, specular, shininess. One to four
     * lamps.
     */
    static constexpr std::size_t kHeaderCount = 3;
    static constexpr std::size_t kLampValueCount = 6;
    static constexpr std::size_t kMaxLamps = 4;

    /** Whether `count` values describe a lighting with one to four lamps. */
    [[nodiscard]] static constexpr bool valid_count(std::size_t count) noexcept
    {
        if (count < kHeaderCount + kLampValueCount) return false;
        if ((count - kHeaderCount) % kLampValueCount != 0) return false;
        return (count - kHeaderCount) / kLampValueCount <= kMaxLamps;
    }

    /** How many values to_values() writes for this lighting. */
    [[nodiscard]] std::size_t value_count() const noexcept
    {
        return kHeaderCount + lamps.size() * kLampValueCount;
    }

    /** Refuses a wrong count or a value that is not a number. */
    [[nodiscard]] bool from_values(const float* values, std::size_t count);
    void to_values(float* values) const;
};

/**
 * A colour with its chroma scaled about its luminance, in integer arithmetic:
 * gray = (2126 r + 7152 g + 722 b + 5000) / 10000, then each channel
 * clamp(gray + ((c - gray) * percent + 50) / 100). At 100 percent every
 * channel comes back unchanged, and a grey stays a grey at any percent.
 */
[[nodiscard]] Color saturate(const Color& color, int percent) noexcept;

/** The lighting every frame is drawn under unless a palette asks for none. */
[[nodiscard]] const Lighting& default_lighting();

/**
 * Height of the ground plane the shadow lies on.
 *
 * Not the bottom of the cube. A side layer half way round swings its bottom
 * corners down to sqrt(2) times the half extent, so a ground flush with the
 * resting cube would be cut through on every R or F. The cube floats a little
 * above the ground instead, whatever size it is and whichever layer turns.
 */
inline constexpr float kGroundY = -1.5f;

/**
 * Below this alpha a highlight is not worth drawing.
 *
 * Also what fixes a highlight's radius: the distance along the plane from
 * the reflection point at which the lobe has fallen to this. The radius is a
 * property of the plane and the lamp, not of the sticker, so one glint spans
 * as many stickers as it reaches and reads as one.
 */
inline constexpr std::uint8_t kHighlightMinAlpha = 8;

/**
 * The shadow's colour and strength. Darker and cooler than either ground
 * rather than grey, and multiplied into the ground's colour rather than
 * painted over it, so the same constants hold on the light theme and the
 * dark one. The renderer does the multiplication itself, once per frame.
 */
inline constexpr Color kShadowColor{40, 48, 64, 255};
inline constexpr std::uint8_t kShadowOpacity = 80;

/**
 * Softness of the shadow edge, as a share of the cube region's side, and the
 * blur's quality. Quality here is how many box passes approximate the
 * Gaussian; one pass reads as a soft edge on a shadow and costs half of two.
 */
inline constexpr float kShadowBlurShare = 0.018f;
inline constexpr int kShadowBlurQuality = 30;

/**
 * How much of the shadow is left at its far tip. Nothing: a shadow that still
 * has a quarter of its strength where the clip cuts it shows the cut.
 */
inline constexpr float kShadowFadeFloor = 0.0f;

/** How much is left half way to the tip: the fade bends rather than runs straight. */
inline constexpr float kShadowFadeMidway = 0.35f;

/** A fade shorter than this in pixels has no direction and falls off radially. */
inline constexpr float kShadowFadeMinLength = 4.0f;

/**
 * The eye height above the ground over which the shadow thins to nothing.
 *
 * The orbit lets the eye drop below the ground, where a plane's shadow would
 * flip over and cross the near plane. Seen from underneath, a floating
 * object's ground shadow is simply not there.
 */
inline constexpr float kShadowFadeBand = 1.0f;

/**
 * A colour at a brightness, in the integer arithmetic the render contract is
 * written in.
 *
 * `intensity` is scaled to a byte first -- it may pass 255, since the
 * palette colour is the colour of a well-lit face rather than the brightest
 * one -- and each channel is min(255, (value * scale + 127) / 255). At an
 * intensity of 1 every channel comes back unchanged.
 */
[[nodiscard]] Color shade(const Color& color, float intensity) noexcept;

}  // namespace rubiks::graphics
