#include "graphics/Palette.hpp"

#include <array>
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstddef>
#include <limits>

using rubiks::cube::FaceColor;
using rubiks::graphics::Color;
using rubiks::graphics::Palette;
using rubiks::graphics::to_color;

namespace {

constexpr std::array<FaceColor, 6> kFaces{
    FaceColor::White, FaceColor::Yellow, FaceColor::Green,
    FaceColor::Blue,  FaceColor::Red,    FaceColor::Orange,
};

/**
 * How far apart the six have to stay under every deficiency.
 *
 * Well above the couple of units that count as a just-noticeable difference,
 * because these are not swatches side by side: a sticker is read across the
 * cube from another sticker, at a glance, while something else is being
 * counted. The standard cube sits at 12 and does not clear this, which is the
 * whole reason the alternative exists.
 */
constexpr double kMinimumDistance = 30.0;

struct Lms {
    double r;
    double g;
    double b;
};

/** A fixed projection onto what one kind of dichromatic vision receives. */
struct Deficiency {
    const char* name;
    std::array<double, 9> matrix;
};

/**
 * The three dichromacies, in linear light.
 *
 * All three, not the famous one. Protanopia and deuteranopia lose the
 * red-green axis and keep blue-yellow; tritanopia loses exactly the other one.
 * A palette scored against only the first pair drifts onto the blue-yellow
 * axis -- it is free distance there -- and arrives worse than the standard
 * cube for the third. Nothing but scoring all three at once catches that,
 * because each projection on its own is perfectly happy.
 *
 * Deterministic and parameterless on purpose: a simulation with knobs would
 * let a failing palette be argued into passing.
 */
constexpr std::array<Deficiency, 3> kDeficiencies{{
    {"protanopia",
     {0.567, 0.433, 0.000, 0.558, 0.442, 0.000, 0.000, 0.242, 0.758}},
    {"deuteranopia",
     {0.625, 0.375, 0.000, 0.700, 0.300, 0.000, 0.000, 0.300, 0.700}},
    {"tritanopia",
     {0.950, 0.050, 0.000, 0.000, 0.433, 0.567, 0.000, 0.475, 0.525}},
}};

double to_linear(std::uint8_t channel) noexcept
{
    const double c = channel / 255.0;
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

/** One colour as a dichromat of the given kind receives it. */
Lms project(Color color, const Deficiency& kind) noexcept
{
    const std::array<double, 3> in{to_linear(color.r), to_linear(color.g),
                                   to_linear(color.b)};
    std::array<double, 3> out{};
    for (std::size_t row = 0; row < out.size(); ++row) {
        for (std::size_t col = 0; col < in.size(); ++col) {
            out[row] += kind.matrix[row * 3 + col] * in[col];
        }
    }
    return Lms{std::clamp(out[0], 0.0, 1.0), std::clamp(out[1], 0.0, 1.0),
               std::clamp(out[2], 0.0, 1.0)};
}

double lab_f(double t) noexcept
{
    constexpr double kEpsilon = 216.0 / 24389.0;
    return t > kEpsilon ? std::cbrt(t) : (841.0 / 108.0) * t + 4.0 / 29.0;
}

/** CIELAB under D65, which is where a perceptual distance can be measured. */
std::array<double, 3> lab(Lms c) noexcept
{
    const double x = 0.4124 * c.r + 0.3576 * c.g + 0.1805 * c.b;
    const double y = 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
    const double z = 0.0193 * c.r + 0.1192 * c.g + 0.9505 * c.b;

    const double fx = lab_f(x / 0.95047);
    const double fy = lab_f(y);
    const double fz = lab_f(z / 1.08883);
    return {116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz)};
}

/**
 * The distance the whole check rests on: perceptual, not channel-wise.
 *
 * Measured after the projection and in Lab rather than as a lightness ratio,
 * because a lightness ratio is exactly the check that passes while two colors
 * are collapsing -- Red and Orange differ in lightness on the standard cube
 * and are still the pair everyone reaches for as the example.
 */
double distance(Color a, Color b, const Deficiency& kind) noexcept
{
    const auto la = lab(project(a, kind));
    const auto lb = lab(project(b, kind));
    double total = 0.0;
    for (std::size_t i = 0; i < la.size(); ++i) {
        total += (la[i] - lb[i]) * (la[i] - lb[i]);
    }
    return std::sqrt(total);
}

/** The smallest distance among the fifteen pairs, under one deficiency. */
double worst_pair(Palette palette, const Deficiency& kind) noexcept
{
    double worst = std::numeric_limits<double>::max();
    for (std::size_t i = 0; i < kFaces.size(); ++i) {
        for (std::size_t j = i + 1; j < kFaces.size(); ++j) {
            worst = std::min(worst, distance(to_color(kFaces[i], palette),
                                             to_color(kFaces[j], palette),
                                             kind));
        }
    }
    return worst;
}

/** The smallest distance among the fifteen pairs, under all three. */
double worst_pair(Palette palette) noexcept
{
    double worst = std::numeric_limits<double>::max();
    for (const auto& kind : kDeficiencies) {
        worst = std::min(worst, worst_pair(palette, kind));
    }
    return worst;
}

bool same(Color a, Color b) noexcept
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

}  // namespace

TEST_CASE("classic palette is the standard cube, channel for channel")
{
    // Known answers rather than a property: the reason Classic exists is that
    // it is the cube people know, so the thing to fix is the exact shades.
    REQUIRE(same(to_color(FaceColor::White), Color{216, 216, 216, 255}));
    REQUIRE(same(to_color(FaceColor::Yellow), Color{255, 213, 0, 255}));
    REQUIRE(same(to_color(FaceColor::Green), Color{0, 155, 72, 255}));
    REQUIRE(same(to_color(FaceColor::Blue), Color{0, 70, 173, 255}));
    REQUIRE(same(to_color(FaceColor::Red), Color{183, 18, 52, 255}));
    REQUIRE(same(to_color(FaceColor::Orange), Color{255, 88, 0, 255}));
}

TEST_CASE("the palette defaults to classic")
{
    for (const FaceColor face : kFaces) {
        REQUIRE(same(to_color(face), to_color(face, Palette::Classic)));
    }
}

TEST_CASE("every high contrast pair survives every dichromacy")
{
    // Named one at a time as well as together, so a regression says which kind
    // of vision it broke rather than only that something got closer.
    for (const auto& kind : kDeficiencies) {
        INFO(kind.name);
        REQUIRE(worst_pair(Palette::HighContrast, kind) >= kMinimumDistance);
    }

    REQUIRE(worst_pair(Palette::HighContrast) >= kMinimumDistance);
}

TEST_CASE("high contrast beats the standard cube for every dichromacy")
{
    // The property the alternative exists for, and the one the first draft of
    // it broke: tuned for red-green deficiencies alone, it drifted onto the
    // blue-yellow axis and came out worse than the standard cube for a
    // tritanope. Better than Classic everywhere is what stops that returning.
    for (const auto& kind : kDeficiencies) {
        INFO(kind.name);
        REQUIRE(worst_pair(Palette::HighContrast, kind) >
                worst_pair(Palette::Classic, kind));
    }
}

TEST_CASE("the standard cube does not clear that bar")
{
    // Not a demand on Classic -- it is allowed to be the standard cube. It is
    // here so the bar above cannot be quietly lowered until anything passes:
    // if a change to the projection or the distance lets Classic through, the
    // check has stopped measuring what it was written to measure.
    REQUIRE(worst_pair(Palette::Classic) < kMinimumDistance);
}

TEST_CASE("high contrast keeps the warm three apart")
{
    // The three that collapse into one yellow-brown on the standard cube for a
    // red-green deficiency, named here so a regression says which pair.
    const Color red = to_color(FaceColor::Red, Palette::HighContrast);
    const Color orange = to_color(FaceColor::Orange, Palette::HighContrast);
    const Color yellow = to_color(FaceColor::Yellow, Palette::HighContrast);

    for (const auto& kind : kDeficiencies) {
        INFO(kind.name);
        REQUIRE(distance(red, orange, kind) >= kMinimumDistance);
        REQUIRE(distance(red, yellow, kind) >= kMinimumDistance);
        REQUIRE(distance(orange, yellow, kind) >= kMinimumDistance);
    }
}

TEST_CASE("both palettes leave every sticker opaque")
{
    for (const FaceColor face : kFaces) {
        REQUIRE(to_color(face, Palette::Classic).a == 255);
        REQUIRE(to_color(face, Palette::HighContrast).a == 255);
    }
}
