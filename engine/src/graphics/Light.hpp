#pragma once

#include <cstdint>

#include "graphics/Color.hpp"
#include "math/Types.hpp"

/**
 * The one light over the cube, and the ground its shadow falls on.
 *
 * Blinn-Phong with its three terms and nothing more: ambient so that no face
 * ever goes black, diffuse for the face-by-face brightness, specular for the
 * glint a face throws when it sits between the light and the eye. There is no
 * reflection term because Blinn-Phong has none.
 *
 * The light is a point rather than a direction on purpose. A direction only
 * ever slides a shadow sideways; a point stretches and leans it as it moves,
 * and slides a highlight across a face, which is what makes a light visible
 * as a light rather than as a colour adjustment.
 */
namespace rubiks::graphics {

struct Light {
    /**
     * World space; each pass carries it into its own space itself.
     *
     * Above and in front of the home viewpoint, a little to its left, so the
     * top face is brightest and the front brighter than the right. The exact
     * numbers were chosen so that every plane's brightness lands well clear
     * of a rounding boundary (docs/tasks/04-rubiks-cube-domain.md, contract
     * v4): a byte that could tip either way with the compiler's arithmetic
     * is not a byte a contract can name.
     */
    math::Vec3 position{2.5f, 4.7f, 4.2f};

    /**
     * Floor of the brightness. Kept high because a sticker's colour is its
     * identity: a red that shades into an orange is not lighting but a
     * mistake, and ambient + diffuse is held at 1 so nothing ever clips.
     */
    float ambient = 0.60f;
    float diffuse = 0.40f;

    /** Peak strength of the glint and how tightly it is focused. */
    float specular = 0.60f;
    float shininess = 32.0f;

    /**
     * The light that changes nothing.
     *
     * Ambient 1 alone would leave the diffuse term on top of it and make
     * every lit face brighter than its sticker; all three have to be set for
     * the shading formula to hand a colour back untouched, byte for byte.
     */
    [[nodiscard]] static constexpr Light unlit() noexcept
    {
        Light light;
        light.ambient = 1.0f;
        light.diffuse = 0.0f;
        light.specular = 0.0f;
        return light;
    }
};

/** The light every frame is drawn under unless a palette asks for none. */
inline constexpr Light kDefaultLight{};

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
 * Radius of a specular highlight, in world units.
 *
 * A property of the plane the light reflects off, not of the sticker on it,
 * so it does not shrink as N grows; a small sticker simply shows less of the
 * same glint.
 */
inline constexpr float kHighlightRadius = 0.45f;

/** Below this peak alpha a face carries no highlight at all. */
inline constexpr std::uint8_t kHighlightMinAlpha = 8;

/**
 * The shadow's colour and strength. Darker and cooler than either ground
 * rather than grey, and multiplied into the ground's colour rather than
 * painted over it, so the same constants hold on the light theme and the
 * dark one. The renderer does the multiplication itself, once per frame.
 */
inline constexpr Color kShadowColor{40, 48, 64, 255};
inline constexpr std::uint8_t kShadowOpacity = 96;

/**
 * Softness of the shadow edge, as a share of the cube region's side, and the
 * blur's quality. Quality here is how many box passes approximate the
 * Gaussian; one pass reads as a soft edge on a shadow and costs half of two.
 */
inline constexpr float kShadowBlurShare = 0.012f;
inline constexpr int kShadowBlurQuality = 30;

/** How much of the shadow is left at the far end of its fade. */
inline constexpr float kShadowFadeFloor = 0.25f;

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
 * The soft patch directly under the cube, whatever the light is doing.
 *
 * Read as the ambient shadow of something hovering close to the ground, which
 * is what the cube is, rather than as contact -- so a turning layer does not
 * contradict it.
 */
inline constexpr float kContactRadiusShare = 0.55f;
inline constexpr std::uint8_t kContactAlpha = 64;

/**
 * A colour at a brightness, in the integer arithmetic the render contract is
 * written in.
 *
 * `intensity` is clamped to [0, 1] and scaled to a byte first, so the whole
 * computation after that is integer and the contract can state exact bytes.
 * At an intensity of 1 every channel comes back unchanged.
 */
[[nodiscard]] Color shade(const Color& color, float intensity) noexcept;

}  // namespace rubiks::graphics
