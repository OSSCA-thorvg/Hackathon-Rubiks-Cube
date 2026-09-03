#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"
#include "graphics/Camera.hpp"
#include "graphics/ConvexHull.hpp"
#include "graphics/CubeGeometry.hpp"
#include "graphics/Layout.hpp"
#include "graphics/Light.hpp"
#include "graphics/OrbitCamera.hpp"
#include "graphics/Palette.hpp"
#include "graphics/Passes.hpp"
#include "graphics/Pipeline.hpp"
#include "math/Transform.hpp"

// Phase 19: the light over the cube and the shadow under it, as passes. Linked
// against the graphics library alone, like the rest of this directory.

namespace {

using Catch::Approx;
using namespace rubiks::graphics;
using rubiks::cube::Axis;
using rubiks::cube::CubeState;
using rubiks::cube::FaceColor;
using rubiks::math::Vec2;
using rubiks::math::Vec3;

constexpr float kPi = 3.14159265358979323846f;
constexpr std::uint32_t kCanvas = 1024;

/** A camera at the origin looking down -Z: view space is world space. */
Camera identity_camera()
{
    return Camera{Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 0.0f, -1.0f},
                  Vec3{0.0f, 1.0f, 0.0f}, kPi / 2.0f, 1.0f, 1.0f, 10.0f};
}

Camera home_camera()
{
    return default_camera(1.0f);
}

/**
 * One lamp on its own, with the standard ambient and attenuation off, so a
 * test's arithmetic is Lambert's cosine and nothing else.
 */
Lighting one_lamp(const Vec3& position, float diffuse = 0.4f,
                  float specular = 0.6f, float shininess = 16.0f,
                  float ambient = 0.6f)
{
    Lighting lighting;
    lighting.ambient = ambient;
    lighting.attenuation = 0.0f;
    lighting.saturation = 1.0f;
    lighting.lamps.push_back(Light{position, diffuse, specular, shininess});
    return lighting;
}

/** A face at z = -depth facing the camera at the origin, or away from it. */
ViewFace facing_face(float depth, bool towards_eye)
{
    ViewFace face;
    face.color = Color{200, 100, 50, 255};
    const std::array<Vec3, 4> counter_clockwise{
        Vec3{-1.0f, -1.0f, -depth}, Vec3{1.0f, -1.0f, -depth},
        Vec3{1.0f, 1.0f, -depth}, Vec3{-1.0f, 1.0f, -depth}};
    face.points = counter_clockwise;
    if (!towards_eye) std::reverse(face.points.begin(), face.points.end());
    return face;
}

/** Interval a caster covers along one axis. */
std::pair<float, float> extent_along(const Caster& caster, Axis axis)
{
    float low = 1e9f;
    float high = -1e9f;
    for (const auto& corner : caster.corners) {
        const float value = axis == Axis::X ? corner.x
                            : axis == Axis::Y ? corner.y
                                              : corner.z;
        low = std::min(low, value);
        high = std::max(high, value);
    }
    return {low, high};
}

int l1_distance(const Color& a, const Color& b)
{
    return std::abs(int{a.r} - int{b.r}) + std::abs(int{a.g} - int{b.g}) +
           std::abs(int{a.b} - int{b.b});
}

/** Every distinct sticker colour after lighting, whatever the viewpoint. */
std::set<std::tuple<int, int, int>> lit_colors(const Camera& camera)
{
    const ViewScene scene = build_cube_scene(CubeState(3))  //
                            | view(camera)                  //
                            | light(default_lighting(), camera);
    std::set<std::tuple<int, int, int>> colors;
    for (const auto& face : scene.faces) {
        colors.insert({face.color.r, face.color.g, face.color.b});
    }
    return colors;
}

}  // namespace

TEST_CASE("shade scales a colour in integer arithmetic and leaves it at 1")
{
    const Color color{183, 18, 52, 255};
    REQUIRE(shade(color, 1.0f) == color);
    // Past 1 it brightens, each channel stopping at 255.
    REQUIRE(shade(color, 2.0f) == Color{255, 36, 104, 255});

    // (value * 153 + 127) / 255 at an intensity of 0.6.
    const Color dim = shade(color, 0.6f);
    REQUIRE(dim.r == (183 * 153 + 127) / 255);
    REQUIRE(dim.g == (18 * 153 + 127) / 255);
    REQUIRE(dim.b == (52 * 153 + 127) / 255);
    REQUIRE(dim.a == 255);

    REQUIRE(shade(color, 0.0f) == Color{0, 0, 0, 255});
}

TEST_CASE("the unlit lighting hands every colour back byte for byte")
{
    const Lighting none = Lighting::unlit(default_lighting());
    REQUIRE(none.ambient == 1.0f);
    REQUIRE(none.saturation == 1.0f);
    REQUIRE(none.lamps.size() == default_lighting().lamps.size());
    REQUIRE(none.lamps.size() == 1);

    // Through the pass as well: a high-contrast sticker comes back as it went.
    const Camera camera = identity_camera();
    ViewScene scene;
    scene.faces.push_back(facing_face(3.0f, true));
    scene.faces[0].color = to_color(FaceColor::Green, Palette::HighContrast);
    const ViewScene plain = light(none, camera)(scene);
    REQUIRE(plain.faces[0].color == to_color(FaceColor::Green, Palette::HighContrast));
    REQUIRE_FALSE(plain.faces[0].shading.has_value());
    for (const auto& lamp : none.lamps) {
        REQUIRE(lamp.diffuse == 0.0f);
        REQUIRE(lamp.specular == 0.0f);
    }
    // The key is still where it was, so the shadow is still cast.
    REQUIRE(none.lamps[0].position.y == default_lighting().lamps[0].position.y);

    for (const auto palette : {Palette::Classic, Palette::HighContrast}) {
        for (int i = 0; i < 6; ++i) {
            const Color color = to_color(static_cast<FaceColor>(i), palette);
            REQUIRE(shade(color, none.ambient) == color);
        }
    }
}

TEST_CASE("a brightness past one lifts a colour and stops at white")
{
    // The palette colour is a well-lit face, not the brightest: a face nearer
    // the lamp goes brighter than the palette, channel by channel to 255.
    const Color red{183, 18, 52, 255};
    const Color brighter = shade(red, 290.0f / 255.0f);
    REQUIRE(brighter.r == std::min(255U, (183U * 290U + 127U) / 255U));
    REQUIRE(brighter.g == (18U * 290U + 127U) / 255U);
    REQUIRE(shade(Color{255, 255, 255, 255}, 1.2f) == Color{255, 255, 255, 255});
}

TEST_CASE("a lighting setup round-trips through its flat list of values")
{
    const Lighting standard = default_lighting();
    REQUIRE(standard.lamps.size() == 1);
    REQUIRE(standard.value_count() == 9);
    float values[Lighting::kHeaderCount + Lighting::kMaxLamps * Lighting::kLampValueCount];
    standard.to_values(values);

    Lighting read;
    REQUIRE(read.from_values(values, standard.value_count()));
    REQUIRE(read.ambient == standard.ambient);
    REQUIRE(read.attenuation == standard.attenuation);
    REQUIRE(read.saturation == standard.saturation);
    REQUIRE(read.lamps.size() == 1);
    REQUIRE(read.lamps[0].position.z == standard.lamps[0].position.z);
    REQUIRE(read.lamps[0].shininess == standard.lamps[0].shininess);

    // One to four lamps, six values each after the three of the header.
    REQUIRE(Lighting::valid_count(9));
    REQUIRE(Lighting::valid_count(15));
    REQUIRE(Lighting::valid_count(27));
    REQUIRE_FALSE(Lighting::valid_count(3));
    REQUIRE_FALSE(Lighting::valid_count(8));
    REQUIRE_FALSE(Lighting::valid_count(10));
    REQUIRE_FALSE(Lighting::valid_count(33));

    // A wrong count or a value that is not a number is refused whole.
    REQUIRE_FALSE(read.from_values(values, 8));
    values[4] = std::numeric_limits<float>::quiet_NaN();
    REQUIRE_FALSE(read.from_values(values, 9));
    REQUIRE(read.lamps.size() == 1);
}

TEST_CASE("saturation scales a colour's chroma about its grey and leaves grey alone")
{
    REQUIRE(saturate(Color{183, 18, 52, 255}, 100) == Color{183, 18, 52, 255});
    REQUIRE(saturate(Color{200, 200, 200, 255}, 150) == Color{200, 200, 200, 255});
    REQUIRE(saturate(Color{255, 255, 255, 255}, 125) == Color{255, 255, 255, 255});

    // gray = (2126*0 + 7152*155 + 722*72 + 5000) / 10000 = 116;
    // r = 116 + ((0 - 116) * 125 + 50) / 100 -> clamped at 0, g -> 165, b -> 61.
    REQUIRE(saturate(Color{0, 155, 72, 255}, 125) == Color{0, 165, 61, 255});
    // gray = 56; r = 56 + 159 = 215, g = 56 - 48 = 8, b = 56 - 5 = 51.
    REQUIRE(saturate(Color{183, 18, 52, 255}, 125) == Color{215, 8, 51, 255});

    // The lighting applies it to stickers and not to the body.
    const Camera camera = identity_camera();
    Lighting lamp = one_lamp(Vec3{0.0f, 0.0f, 0.0f});
    lamp.saturation = 1.25f;
    ViewScene scene;
    scene.faces.push_back(facing_face(3.0f, true));
    scene.faces.push_back(facing_face(3.0f, true));
    scene.faces[1].color = kBodyColor;
    const ViewScene lit = light(lamp, camera)(scene);
    REQUIRE(lit.faces[0].color == saturate(Color{200, 100, 50, 255}, 125));
    REQUIRE(lit.faces[0].color != Color{200, 100, 50, 255});
    REQUIRE(lit.faces[1].color == kBodyColor);
}

TEST_CASE("convex hull of a box's corners drops the inside and orders the rim")
{
    const auto hull = convex_hull({Vec2{0.0f, 0.0f}, Vec2{2.0f, 0.0f},
                                   Vec2{2.0f, 2.0f}, Vec2{0.0f, 2.0f},
                                   Vec2{1.0f, 1.0f}, Vec2{1.0f, 0.0f},
                                   Vec2{2.0f, 2.0f}});
    REQUIRE(hull.size() == 4);

    // Counter-clockwise in a +Y-up frame: positive signed area.
    float area = 0.0f;
    for (std::size_t i = 0; i < hull.size(); ++i) {
        const auto& a = hull[i];
        const auto& b = hull[(i + 1) % hull.size()];
        area += a.x * b.y - b.x * a.y;
    }
    REQUIRE(area == Approx(8.0f));

    REQUIRE(convex_hull({Vec2{1.0f, 1.0f}, Vec2{1.0f, 1.0f}}).size() == 1);
}

TEST_CASE("a resting cube casts as one box, the cube itself")
{
    const WorldScene scene = build_cube_scene(CubeState(3));
    REQUIRE(scene.casters.size() == 1);

    for (const auto axis : {Axis::X, Axis::Y, Axis::Z}) {
        const auto [low, high] = extent_along(scene.casters[0], axis);
        REQUIRE(low == Approx(-kCubeHalfExtent));
        REQUIRE(high == Approx(kCubeHalfExtent));
    }
    REQUIRE_FALSE(scene.shadow.has_value());
}

TEST_CASE("a turn cuts the casters into runs along its axis alone")
{
    const CubeState state(3);

    SECTION("R: the turning face and the rest")
    {
        const ActiveRotation turn{Axis::X, rubiks::cube::layer(2), 45.0f};
        const WorldScene scene = build_cube_scene(state, turn);
        REQUIRE(scene.casters.size() == 2);

        // The still run spans the two left layers along X and the whole cube
        // across it; nothing was cut along Y or Z.
        const auto [low, high] = extent_along(scene.casters[0], Axis::X);
        REQUIRE(low == Approx(-1.0f));
        REQUIRE(high == Approx(1.0f / 3.0f));
        REQUIRE(extent_along(scene.casters[0], Axis::Y).first == Approx(-1.0f));
        REQUIRE(extent_along(scene.casters[0], Axis::Z).second == Approx(1.0f));

        // The turning run is one layer thick along X and has been rotated
        // about X, so its Y and Z extents have grown to the diagonal.
        const auto [turn_low, turn_high] = extent_along(scene.casters[1], Axis::X);
        REQUIRE(turn_low == Approx(1.0f / 3.0f));
        REQUIRE(turn_high == Approx(1.0f));
        REQUIRE(extent_along(scene.casters[1], Axis::Y).second ==
                Approx(std::sqrt(2.0f)).margin(1e-4));
    }

    SECTION("Rw: still, turning")
    {
        const ActiveRotation turn{Axis::X, rubiks::cube::layers_through(1, 2), 30.0f};
        REQUIRE(build_cube_scene(state, turn).casters.size() == 2);
    }

    SECTION("M: still, turning, still -- the most a run mask makes")
    {
        const ActiveRotation turn{Axis::Y, rubiks::cube::layer(1), 30.0f};
        const WorldScene scene = build_cube_scene(state, turn);
        REQUIRE(scene.casters.size() == 3);

        // The runs partition the axis without gaps or overlaps.
        std::vector<std::pair<float, float>> runs;
        for (const auto& caster : scene.casters) {
            runs.push_back(extent_along(caster, Axis::Y));
        }
        std::sort(runs.begin(), runs.end());
        REQUIRE(runs.front().first == Approx(-1.0f));
        REQUIRE(runs.back().second == Approx(1.0f));
        for (std::size_t i = 1; i < runs.size(); ++i) {
            REQUIRE(runs[i].first == Approx(runs[i - 1].second));
        }
    }

    SECTION("at zero degrees the pieces are the resting cube in more parts")
    {
        const ActiveRotation turn{Axis::Z, rubiks::cube::layer(0), 0.0f};
        const WorldScene scene = build_cube_scene(state, turn);
        REQUIRE(scene.casters.size() == 2);

        float low = 1e9f, high = -1e9f;
        for (const auto& caster : scene.casters) {
            const auto [run_low, run_high] = extent_along(caster, Axis::Z);
            low = std::min(low, run_low);
            high = std::max(high, run_high);
        }
        REQUIRE(low == Approx(-1.0f));
        REQUIRE(high == Approx(1.0f));
    }

    SECTION("a bigger cube makes no more casters")
    {
        const CubeState big(9);
        const ActiveRotation turn{Axis::X, rubiks::cube::layers_through(2, 4), 20.0f};
        REQUIRE(build_cube_scene(big, turn).casters.size() == 3);
    }
}

TEST_CASE("the model transform moves the casters with the stickers")
{
    rubiks::math::Transform moved;
    moved.translation = Vec3{0.5f, -0.25f, 2.0f};

    const WorldScene before = build_cube_scene(CubeState(3));
    const WorldScene after = before | transform(moved);

    REQUIRE(after.casters.size() == before.casters.size());
    for (std::size_t i = 0; i < 8; ++i) {
        const Vec3 delta =
            after.casters[0].corners[i] - before.casters[0].corners[i];
        REQUIRE(delta.x == Approx(0.5f));
        REQUIRE(delta.y == Approx(-0.25f));
        REQUIRE(delta.z == Approx(2.0f));
    }
    const Vec3 face_delta = after.faces[0].points[0] - before.faces[0].points[0];
    REQUIRE(face_delta.x == Approx(0.5f));
    REQUIRE(face_delta.z == Approx(2.0f));
}

TEST_CASE("the shadow's points travel through the passes like face corners")
{
    // One face, and a shadow polygon with the very same corners.
    WorldScene scene;
    WorldFace face;
    face.points = {Vec3{-0.5f, -0.5f, -3.0f}, Vec3{0.5f, -0.5f, -3.0f},
                   Vec3{0.5f, 0.5f, -3.0f}, Vec3{-0.5f, 0.5f, -3.0f}};
    scene.faces.push_back(face);

    ShadowGroup<Vec3> shadow;
    shadow.polygons.push_back(
        std::vector<Vec3>(face.points.begin(), face.points.end()));
    shadow.fade_start = face.points[0];
    shadow.fade_end = face.points[2];
    shadow.opacity = 77;
    scene.shadow = shadow;
    // And a caster, which becomes a body on screen.
    Caster box;
    for (std::size_t i = 0; i < 8; ++i) box.corners[i] = face.points[i % 4];
    scene.casters.push_back(box);

    const Rect region{40.0f, 10.0f, 200.0f, 200.0f};
    const Camera camera = identity_camera();
    const RenderScene result = scene | view(camera) | project(camera) | cull() |
                               depth_sort() | viewport(region);

    REQUIRE(result.faces.size() == 1);
    REQUIRE(result.shadow.has_value());
    REQUIRE(result.shadow->polygons.size() == 1);
    for (std::size_t i = 0; i < 4; ++i) {
        REQUIRE(result.shadow->polygons[0][i].x == Approx(result.faces[0].points[i].x));
        REQUIRE(result.shadow->polygons[0][i].y == Approx(result.faces[0].points[i].y));
    }
    REQUIRE(result.shadow->fade_start.x == Approx(result.faces[0].points[0].x));
    REQUIRE(result.shadow->fade_end.y == Approx(result.faces[0].points[2].y));
    REQUIRE(result.shadow->opacity == 77);

    // The viewport pass is the one that knows the rectangle.
    REQUIRE(result.shadow->clip.x == Approx(region.x));
    REQUIRE(result.shadow->clip.width == Approx(region.width));
    REQUIRE(result.shadow->blur_sigma == Approx(kShadowBlurShare * region.width));

    SECTION("given a stage, the shadow may use it while the blur follows the viewport")
    {
        const Rect stage{0.0f, 0.0f, 640.0f, 300.0f};
        const RenderScene staged = scene | view(camera) | project(camera) |
                                   viewport(region, stage);
        REQUIRE(staged.shadow->clip.width == Approx(stage.width));
        REQUIRE(staged.shadow->clip.height == Approx(stage.height));
        REQUIRE(staged.shadow->blur_sigma == Approx(kShadowBlurShare * region.width));
    }

    // The caster arrives as a body: a flat box's silhouette is its face.
    REQUIRE(result.bodies.size() == 1);
    REQUIRE(result.bodies[0].size() == 4);

    SECTION("the body is drawn whether or not there is a shadow")
    {
        WorldScene unshadowed = scene;
        unshadowed.shadow.reset();
        const RenderScene bare = unshadowed | view(camera) | project(camera) |
                                 cull() | depth_sort() | viewport(region);
        REQUIRE_FALSE(bare.shadow.has_value());
        REQUIRE(bare.bodies.size() == 1);
    }

    SECTION("culling a face does not cull the shadow")
    {
        WorldScene reversed = scene;
        std::reverse(reversed.faces[0].points.begin(),
                     reversed.faces[0].points.end());
        const RenderScene culled = reversed | view(camera) | project(camera) |
                                   cull() | depth_sort() | viewport(region);
        REQUIRE(culled.faces.empty());
        REQUIRE(culled.shadow.has_value());
    }

    SECTION("a shadow reaching the near plane goes whole")
    {
        WorldScene close = scene;
        close.shadow->polygons[0][0].z = -0.5f;
        const ClipScene projected = close | view(camera) | project(camera);
        REQUIRE(projected.faces.size() == 1);
        REQUIRE_FALSE(projected.shadow.has_value());
    }
}

TEST_CASE("a face towards the light is lit and a face away from it is ambient")
{
    const Camera camera = identity_camera();
    const Lighting lamp = one_lamp(Vec3{0.0f, 0.0f, 0.0f});  // at the eye

    ViewScene scene;
    scene.faces.push_back(facing_face(3.0f, true));
    scene.faces.push_back(facing_face(3.0f, false));
    const ViewScene lit = light(lamp, camera)(scene);

    // Straight on: ambient + diffuse = 1, so the colour is untouched.
    REQUIRE(lit.faces[0].color == Color{200, 100, 50, 255});
    // Facing away: exactly the ambient floor.
    REQUIRE(lit.faces[1].color == shade(Color{200, 100, 50, 255}, lamp.ambient));

    SECTION("the unlit lighting changes nothing")
    {
        const ViewScene plain = light(Lighting::unlit(lamp), camera)(scene);
        REQUIRE(plain.faces[0].color == Color{200, 100, 50, 255});
        REQUIRE(plain.faces[1].color == Color{200, 100, 50, 255});
        REQUIRE(plain.faces[0].highlights.empty());
        REQUIRE_FALSE(plain.faces[0].shading.has_value());
    }
}

TEST_CASE("a lamp's diffuse falls off with distance from the cube's centre")
{
    // Two faces straight ahead at different depths, the lamp at the eye and
    // the cube's centre two units in: with attenuation on, the nearer face
    // is brighter than the palette and the farther one darker.
    Camera camera = Camera{Vec3{0.0f, 0.0f, 2.0f}, Vec3{0.0f, 0.0f, 0.0f},
                           Vec3{0.0f, 1.0f, 0.0f}, kPi / 2.0f, 1.0f, 1.0f, 10.0f};
    Lighting lamp = one_lamp(Vec3{0.0f, 0.0f, 2.0f});
    lamp.attenuation = 1.0f;

    ViewScene scene;
    scene.faces.push_back(facing_face(1.0f, true));   // one unit from the lamp
    scene.faces.push_back(facing_face(2.0f, true));   // at the centre's distance
    scene.faces.push_back(facing_face(4.0f, true));   // twice as far
    const ViewScene lit = light(lamp, camera)(scene);

    const auto luminance = [](const Color& c) { return int{c.r} + int{c.g} + int{c.b}; };
    REQUIRE(lit.faces[1].color == Color{200, 100, 50, 255});
    REQUIRE(luminance(lit.faces[0].color) > luminance(lit.faces[1].color));
    REQUIRE(luminance(lit.faces[2].color) < luminance(lit.faces[1].color));
    // At twice the distance the diffuse term is half: 0.6 + 0.4 * 0.5.
    REQUIRE(lit.faces[2].color == shade(Color{200, 100, 50, 255}, 0.8f));

    SECTION("with attenuation off, distance does not matter")
    {
        lamp.attenuation = 0.0f;
        const ViewScene flat = light(lamp, camera)(scene);
        REQUIRE(flat.faces[0].color == flat.faces[2].color);
    }
}

TEST_CASE("the light stays in the world when the camera orbits")
{
    // The same six shaded colours from the home viewpoint and from a quarter
    // of the way round it: a light riding with the eye would light other
    // faces instead.
    OrbitCamera turned = home_orbit();
    turned.turn(90.0f, 0.0f);

    const auto from_home = lit_colors(home_camera());
    const auto from_side = lit_colors(turned.to_camera(1.0f));
    REQUIRE(from_home == from_side);

    // Three of them are the visible faces at home, and they differ: that is
    // what lets the render contract tell a flipped normal from a right one.
    const ViewScene scene = build_cube_scene(CubeState(3))     //
                            | view(home_camera())              //
                            | light(default_lighting(), home_camera());
    std::set<std::tuple<int, int, int>> whites;
    for (const auto& face : scene.faces) {
        // Every white sticker is on the +Y face, so all nine shade alike --
        // one plane, one value, however far each sticker is from the light.
        if (face.color.r == face.color.g && face.color.g == face.color.b &&
            face.color.r > 128) {
            whites.insert({face.color.r, face.color.g, face.color.b});
        }
    }
    REQUIRE(whites.size() == 1);
    // Six planes, six shades: 54 stickers in no more than six colours each.
    REQUIRE(from_home.size() <= 6 * 6);
}

TEST_CASE("the home viewpoint's sample stickers carry no highlight")
{
    // From the home eye the mirror image of the light lands well outside the
    // three visible faces, so the contract's sample stickers -- the middle of
    // each -- are plain shaded colour with no glint to reproduce. A glint
    // reaching a corner sticker is allowed; the contract does not look there.
    const WorldScene world = build_cube_scene(CubeState(3));
    const ViewScene scene = world | view(home_camera()) | light(default_lighting(), home_camera());
    REQUIRE(scene.faces.size() == world.faces.size());

    int samples = 0;
    for (std::size_t i = 0; i < world.faces.size(); ++i) {
        Vec3 centre{0.0f, 0.0f, 0.0f};
        for (const auto& point : world.faces[i].points) centre += point;
        centre /= 4.0f;

        const bool up = std::abs(centre.x) < 1e-3f && std::abs(centre.z) < 1e-3f && centre.y > 0.0f;
        const bool front = std::abs(centre.x) < 1e-3f && std::abs(centre.y) < 1e-3f && centre.z > 0.0f;
        const bool right = std::abs(centre.y) < 1e-3f && std::abs(centre.z) < 1e-3f && centre.x > 0.0f;
        if (!(up || front || right)) continue;

        ++samples;
        INFO("sample sticker " << i);
        REQUIRE(scene.faces[i].highlights.empty());
    }
    REQUIRE(samples == 3);

}

TEST_CASE("a lit face shades from its near end to its far end")
{
    const Camera camera = identity_camera();
    // Level with the eye, off to the right; no distance falloff, so the ends
    // are Lambert's cosine alone.
    const Lighting lamp = one_lamp(Vec3{2.0f, 0.0f, 0.0f});

    ViewScene scene;
    scene.faces.push_back(facing_face(3.0f, true));
    const ViewScene lit = light(lamp, camera)(scene);
    const auto& face = lit.faces[0];

    // The light's foot on the plane z = -3 is (2, 0, -3); the shading runs
    // away from it, from the face's right edge to its left.
    REQUIRE(face.shading.has_value());
    REQUIRE(face.shading->from.x == Approx(1.0f).margin(1e-4));
    REQUIRE(face.shading->to.x == Approx(-1.0f).margin(1e-4));
    REQUIRE(face.shading->from.z == Approx(-3.0f).margin(1e-4));
    REQUIRE(face.shading->colors.size() == 5);
    const auto luminance = [](const Color& c) { return int{c.r} + int{c.g} + int{c.b}; };
    REQUIRE(luminance(face.shading->colors.front()) > luminance(face.shading->colors.back()));

    // The stops are the function's true values: cos = h / sqrt(h^2 + rho^2)
    // with h = 3 and rho = 1 at the near end, 2 in the middle, 3 at the far.
    const auto expected = [&](float rho) {
        return shade(Color{200, 100, 50, 255},
                     lamp.ambient + lamp.lamps[0].diffuse * 3.0f / std::sqrt(9.0f + rho * rho));
    };
    REQUIRE(face.shading->colors[0] == expected(1.0f));
    REQUIRE(face.shading->colors[2] == expected(2.0f));
    REQUIRE(face.shading->colors[4] == expected(3.0f));
    // And the reference colour is read where the plane comes nearest the
    // cube's centre -- the origin here -- which is rho = 2 from the foot.
    REQUIRE(face.color == expected(2.0f));

    SECTION("a face the light does not reach is flat")
    {
        ViewScene away;
        away.faces.push_back(facing_face(3.0f, false));
        const ViewScene dim = light(lamp, camera)(away);
        REQUIRE_FALSE(dim.faces[0].shading.has_value());
        REQUIRE(dim.faces[0].color == shade(Color{200, 100, 50, 255}, lamp.ambient));
    }

    SECTION("the unlit lighting draws no shading either")
    {
        REQUIRE_FALSE(light(Lighting::unlit(lamp), camera)(scene).faces[0].shading.has_value());
    }
}

TEST_CASE("the shading of one plane runs on across its stickers")
{
    const WorldScene world = build_cube_scene(CubeState(3));
    const ViewScene scene = world | view(home_camera()) | light(default_lighting(), home_camera());

    // The nine +Z stickers (green: the white top clamps and shows nothing):
    // every one shaded, all with the plane's one axis, every end on the plane.
    int counted = 0;
    std::optional<Vec3> axis_from, axis_to;
    for (std::size_t i = 0; i < world.faces.size(); ++i) {
        Vec3 centre{0.0f, 0.0f, 0.0f};
        for (const auto& point : world.faces[i].points) centre += point;
        centre /= 4.0f;
        if (centre.z < 0.9f) continue;

        ++counted;
        const auto& face = scene.faces[i];
        REQUIRE(face.shading.has_value());

        // One function for the plane: the same axis under every sticker.
        if (!axis_from) {
            axis_from = face.shading->from;
            axis_to = face.shading->to;
        }
        REQUIRE(rubiks::math::length(face.shading->from - *axis_from) < 1e-4f);
        REQUIRE(rubiks::math::length(face.shading->to - *axis_to) < 1e-4f);
        REQUIRE(face.shading->colors.size() == 5);

        // Both ends lie on the face's plane: the same view-space height above
        // it as the corners, which is zero.
        const Vec3 normal = rubiks::math::normalize(rubiks::math::cross(
            face.points[1] - face.points[0], face.points[2] - face.points[0]));
        REQUIRE(rubiks::math::dot(face.shading->from - face.points[0], normal) == Approx(0.0f).margin(1e-3));
        REQUIRE(rubiks::math::dot(face.shading->to - face.points[0], normal) == Approx(0.0f).margin(1e-3));

        if (std::abs(centre.x) < 1e-3f && std::abs(centre.y) < 1e-3f) {
            // The middle sticker: the axis runs through the middle of the
            // face, so the middle stop is the plane's reference byte, which
            // is what the contract reads.
            REQUIRE(face.shading->colors.front() != face.shading->colors.back());
            REQUIRE(face.shading->colors[2] == face.color);
        }
    }
    REQUIRE(counted == 9);

    SECTION("a mixed cube shares the axis too, each sticker in its own colour")
    {
        CubeState mixed(3);
        mixed.apply(rubiks::cube::moves::U(3));
        mixed.apply(rubiks::cube::moves::R(3));
        const WorldScene mixed_world = build_cube_scene(mixed);
        const ViewScene lit = mixed_world | view(home_camera()) | light(default_lighting(), home_camera());

        std::optional<Vec3> from;
        std::set<std::tuple<int, int, int>> colors;
        for (std::size_t i = 0; i < mixed_world.faces.size(); ++i) {
            Vec3 centre{0.0f, 0.0f, 0.0f};
            for (const auto& point : mixed_world.faces[i].points) centre += point;
            centre /= 4.0f;
            if (centre.z < 0.9f || !lit.faces[i].shading) continue;
            if (!from) from = lit.faces[i].shading->from;
            REQUIRE(rubiks::math::length(lit.faces[i].shading->from - *from) < 1e-4f);
            const auto& c = lit.faces[i].shading->colors[2];
            colors.insert({c.r, c.g, c.b});
        }
        // More than one colour on the front face now, all on one axis.
        REQUIRE(colors.size() > 1);
    }

    SECTION("a sticker whose ends shade alike is drawn flat")
    {
        // On the white top face the clamp makes both ends 255 wherever the
        // brightness passes 1, and those stickers carry no gradient.
        const ViewScene fine = build_cube_scene(CubeState(28))  //
                               | view(home_camera())            //
                               | light(default_lighting(), home_camera());
        for (const auto& face : fine.faces) {
            if (face.shading) {
                REQUIRE(face.shading->colors.front() != face.shading->colors.back());
            }
        }
    }
}

TEST_CASE("a highlight sits where the light reflects into the eye")
{
    const Camera camera = identity_camera();
    ViewScene scene;
    scene.faces.push_back(facing_face(2.0f, true));

    SECTION("inside the face, at the mirror point")
    {
        const Lighting lamp = one_lamp(Vec3{0.5f, 0.5f, 0.0f});
        const ViewScene lit = light(lamp, camera)(scene);

        REQUIRE(lit.faces[0].highlights.size() == 1);
        const auto& glint = lit.faces[0].highlights[0];
        // Mirrored to (0.5, 0.5, -4); half way back to the eye is z = -2,
        // where the peak is. The footprint is fitted round the peak, so its
        // centre lies on the plane within the footprint of the peak.
        REQUIRE(glint.centre.z == Approx(-2.0f).margin(1e-4));
        const Vec3 semi_u = glint.rims[0] - glint.centre;
        const Vec3 semi_v = glint.rims[2] - glint.centre;
        REQUIRE(rubiks::math::length(semi_u) > 0.0f);
        REQUIRE(rubiks::math::length(semi_v) > 0.0f);
        REQUIRE(semi_u.z == Approx(0.0f).margin(1e-4));
        REQUIRE(semi_v.z == Approx(0.0f).margin(1e-4));
        // Opposite rims mirror each other through the centre.
        REQUIRE((glint.rims[0] + glint.rims[1]).x == Approx(2.0f * glint.centre.x).margin(1e-4));
        REQUIRE((glint.rims[2] + glint.rims[3]).y == Approx(2.0f * glint.centre.y).margin(1e-4));
        REQUIRE(std::abs(glint.centre.x - 0.25f) < rubiks::math::length(semi_u));
        REQUIRE(std::abs(glint.centre.y - 0.25f) < rubiks::math::length(semi_v));
        // Strong at the centre, never rising outwards, gone at the rim.
        REQUIRE(glint.stops.front() >= std::lround(lamp.lamps[0].specular * 255.0f) / 2);
        for (std::size_t s = 1; s < glint.stops.size(); ++s) {
            REQUIRE(glint.stops[s] <= glint.stops[s - 1]);
        }
        REQUIRE(glint.stops[1] >= kHighlightMinAlpha);
        REQUIRE(glint.stops.back() == 0);
    }

    SECTION("left where it falls when the mirror point is off the face")
    {
        // The peak is the plane's, not the sticker's: it stays at (2.5, 0, -2)
        // beyond the face's edge, and the face carries it because the lobe
        // reaches in. Every sticker on this plane would see the same peak.
        const Lighting lamp = one_lamp(Vec3{5.0f, 0.0f, 0.0f});
        const ViewScene lit = light(lamp, camera)(scene);

        REQUIRE(lit.faces[0].highlights.size() == 1);
        const auto& glint = lit.faces[0].highlights[0];
        // The footprint's centre is off the face, and its near rim reaches
        // across the edge into it.
        REQUIRE(glint.centre.x > 1.0f);
        REQUIRE(std::min(glint.rims[0].x, glint.rims[1].x) < 1.0f);
        REQUIRE(glint.stops.front() >= kHighlightMinAlpha);
    }

    SECTION("two lamps, two glints, each where its own mirror point is")
    {
        Lighting pair = one_lamp(Vec3{0.5f, 0.5f, 0.0f});
        pair.lamps.push_back(Light{Vec3{-0.5f, -0.5f, 0.0f}, 0.0f, 0.6f, 16.0f});
        const ViewScene lit = light(pair, camera)(scene);
        REQUIRE(lit.faces[0].highlights.size() == 2);
        REQUIRE(lit.faces[0].highlights[0].centre.x > 0.0f);
        REQUIRE(lit.faces[0].highlights[1].centre.x < 0.0f);
    }

    SECTION("and gone when a tight lobe's mirror point is far away")
    {
        // A soft lobe reaches a long way -- a grazing light puts a faint sheen
        // on most of a plane, which is what a satin plastic does. Tighten it
        // and the glint stays where the mirror point is.
        const Lighting lamp = one_lamp(Vec3{40.0f, 0.0f, 0.0f}, 0.4f, 0.6f, 128.0f);
        REQUIRE(light(lamp, camera)(scene).faces[0].highlights.empty());
    }

    SECTION("a face lit from behind carries none")
    {
        const Lighting lamp = one_lamp(Vec3{0.0f, 0.0f, -5.0f});
        REQUIRE(light(lamp, camera)(scene).faces[0].highlights.empty());
    }
}

TEST_CASE("the six sticker colours stay apart at both ends of the shading")
{
    for (const auto palette : {Palette::Classic, Palette::HighContrast}) {
        for (const float intensity : {default_lighting().ambient, 1.0f}) {
            std::vector<Color> shaded;
            for (int i = 0; i < 6; ++i) {
                shaded.push_back(shade(
                    to_color(static_cast<FaceColor>(i), palette), intensity));
            }
            for (std::size_t a = 0; a < shaded.size(); ++a) {
                for (std::size_t b = a + 1; b < shaded.size(); ++b) {
                    INFO("palette " << static_cast<int>(palette) << " at "
                                    << intensity << ": colours " << a << " and "
                                    << b);
                    REQUIRE(l1_distance(shaded[a], shaded[b]) >= 60);
                }
            }
        }
    }
}

TEST_CASE("a resting cube casts one convex shadow onto the ground")
{
    const WorldScene scene =
        build_cube_scene(CubeState(3)) | shadow(default_lighting(), home_camera());

    REQUIRE(scene.shadow.has_value());
    const auto& cast = *scene.shadow;
    REQUIRE(cast.polygons.size() == 1);
    REQUIRE(cast.opacity == kShadowOpacity);

    // Everything lies on the ground plane, and the polygon is its own hull.
    std::vector<Vec2> footprint;
    for (const auto& point : cast.polygons[0]) {
        REQUIRE(point.y == Approx(kGroundY));
        footprint.push_back(Vec2{point.x, point.z});
    }
    REQUIRE(footprint.size() >= 4);
    REQUIRE(convex_hull(footprint).size() == footprint.size());

    // The anchor is the cube's centre dropped straight down; the far anchor
    // is away from the key light, which sits at +X +Z of the cube.
    REQUIRE(cast.fade_start.x == Approx(0.0f).margin(1e-5));
    REQUIRE(cast.fade_start.z == Approx(0.0f).margin(1e-5));
    REQUIRE(cast.fade_end.x < cast.fade_start.x);
    REQUIRE(cast.fade_end.z < cast.fade_start.z);

    // And it is the shadow's tip: no point of the outline lies further along
    // that direction, and one lies exactly there.
    const Vec3 thrown = cast.fade_end - cast.fade_start;
    const float reach = rubiks::math::length(thrown);
    const Vec3 direction = thrown / reach;
    float furthest = 0.0f;
    for (const auto& point : cast.polygons[0]) {
        furthest = std::max(furthest, rubiks::math::dot(point - cast.fade_start, direction));
    }
    REQUIRE(furthest == Approx(reach).margin(1e-3));
}

TEST_CASE("a light straight overhead widens the shadow by its height")
{
    const Lighting lamp = one_lamp(Vec3{0.0f, 4.5f, 0.0f});
    const WorldScene scene =
        build_cube_scene(CubeState(3)) | shadow(lamp, home_camera());
    REQUIRE(scene.shadow.has_value());

    // A point light at a finite height does not trace the footprint: the top
    // corners at y = 1 are pushed out by (4.5 + 1.5) / (4.5 - 1) and are the
    // whole outline. The centre and axes are the cube's own.
    const float scale = (lamp.lamps[0].position.y - kGroundY) / (lamp.lamps[0].position.y - 1.0f);
    float reach = 0.0f;
    for (const auto& point : scene.shadow->polygons[0]) {
        reach = std::max({reach, std::abs(point.x), std::abs(point.z)});
        REQUIRE(std::abs(point.x) == Approx(scale).margin(1e-4));
        REQUIRE(std::abs(point.z) == Approx(scale).margin(1e-4));
    }
    REQUIRE(reach == Approx(scale).margin(1e-4));
    REQUIRE(scene.shadow->polygons[0].size() == 4);

    // No direction to fade along.
    REQUIRE(scene.shadow->fade_end.x == Approx(scene.shadow->fade_start.x).margin(1e-5));
    REQUIRE(scene.shadow->fade_end.z == Approx(scene.shadow->fade_start.z).margin(1e-5));
}

TEST_CASE("a light no higher than the cube casts nothing onto the ground")
{
    const WorldScene scene = build_cube_scene(CubeState(3)) |
                             shadow(one_lamp(Vec3{2.0f, 0.5f, 2.0f}), home_camera());
    REQUIRE_FALSE(scene.shadow.has_value());

    SECTION("nor does a lighting with no lamps at all")
    {
        Lighting dark;
        REQUIRE_FALSE((build_cube_scene(CubeState(3)) | shadow(dark, home_camera())).shadow.has_value());
    }

    SECTION("only the key casts: the kicker behind the cube adds no second shadow")
    {
        const WorldScene lit = build_cube_scene(CubeState(3)) |
                               shadow(default_lighting(), home_camera());
        REQUIRE(lit.shadow.has_value());
        REQUIRE(lit.shadow->polygons.size() == 1);
        // Thrown away from the key at +X +Z, not towards it from the kicker.
        REQUIRE(lit.shadow->fade_end.x < lit.shadow->fade_start.x);
    }
}

TEST_CASE("the shadow thins as the eye nears the ground and is gone below it")
{
    const auto camera_at = [](float eye_y) {
        return Camera{Vec3{4.0f, eye_y, 4.0f}, kCubeCenter, kWorldUp,
                      kVerticalFov, 1.0f, kNearPlane, kFarPlane};
    };
    const WorldScene world = build_cube_scene(CubeState(3));

    REQUIRE((world | shadow(default_lighting(), camera_at(3.0f))).shadow->opacity ==
            kShadowOpacity);

    const auto half = world | shadow(default_lighting(),
                                     camera_at(kGroundY + 0.5f * kShadowFadeBand));
    REQUIRE(half.shadow.has_value());
    REQUIRE(half.shadow->opacity == std::lround(kShadowOpacity * 0.5f));

    REQUIRE_FALSE((world | shadow(default_lighting(), camera_at(kGroundY))).shadow.has_value());
    REQUIRE_FALSE((world | shadow(default_lighting(), camera_at(-3.0f))).shadow.has_value());
}

TEST_CASE("a turning layer's shadow turns with it")
{
    const ActiveRotation turn{Axis::X, rubiks::cube::layer(2), 45.0f};
    const WorldScene scene = build_cube_scene(CubeState(3), turn) |
                             shadow(default_lighting(), home_camera());

    REQUIRE(scene.shadow.has_value());
    REQUIRE(scene.shadow->polygons.size() == 2);

    // The same turn at zero degrees: the still slab's shadow is the same, and
    // the turning slab's is not.
    const ActiveRotation settled{Axis::X, rubiks::cube::layer(2), 0.0f};
    const WorldScene rest = build_cube_scene(CubeState(3), settled) |
                            shadow(default_lighting(), home_camera());
    REQUIRE(rest.shadow->polygons.size() == 2);

    const auto same_outline = [](const std::vector<Vec3>& a,
                                 const std::vector<Vec3>& b) {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (std::abs(a[i].x - b[i].x) > 1e-3f ||
                std::abs(a[i].z - b[i].z) > 1e-3f) {
                return false;
            }
        }
        return true;
    };
    REQUIRE(same_outline(scene.shadow->polygons[0], rest.shadow->polygons[0]));
    REQUIRE_FALSE(same_outline(scene.shadow->polygons[1], rest.shadow->polygons[1]));

    // A box turned off its axes shows more corners to the ground than four.
    REQUIRE(scene.shadow->polygons[1].size() > 4);
}

TEST_CASE("the whole chain puts the shadow below the cube inside its region")
{
    const auto placement = layout(kCanvas, kCanvas);
    const Camera camera = default_camera(placement.cube.width / placement.cube.height);
    const RenderScene scene = build_cube_scene(CubeState(3))  //
                              | transform({})                 //
                              | shadow(default_lighting(), camera) //
                              | view(camera)                  //
                              | light(default_lighting(), camera)  //
                              | project(camera)               //
                              | cull()                        //
                              | depth_sort()                  //
                              | viewport(placement.cube);

    REQUIRE(scene.faces.size() == 27);
    REQUIRE(scene.shadow.has_value());
    REQUIRE(scene.bodies.size() == 1);
    // A box seen corner-on is a hexagon.
    REQUIRE(scene.bodies[0].size() == 6);

    // The anchor under the cube is lower on screen than the cube's middle,
    // and the far anchor is higher up: the shadow leans away behind it.
    float middle_y = 0.0f;
    for (const auto& face : scene.faces) {
        for (const auto& point : face.points) middle_y += point.y;
    }
    middle_y /= static_cast<float>(scene.faces.size() * 4);
    REQUIRE(scene.shadow->fade_start.y > middle_y);
    REQUIRE(scene.shadow->fade_end.y < scene.shadow->fade_start.y);
    REQUIRE(scene.shadow->clip.width == Approx(placement.cube.width));
}

TEST_CASE("the home viewpoint's planes shade to the bytes the contract names")
{
    // docs/tasks/04-rubiks-cube-domain.md, contract v4: the brightness of the
    // +Y, +Z and +X planes under the resting light, as a byte out of 255. The
    // numbers are worked out there by hand; this is the engine agreeing.
    const ViewScene scene = build_cube_scene(CubeState(3))     //
                            | view(home_camera())              //
                            | light(default_lighting(), home_camera());

    std::optional<Color> white, green, red;
    for (const auto& face : scene.faces) {
        const Color plain_white = to_color(FaceColor::White);
        const Color plain_green = to_color(FaceColor::Green);
        const Color plain_red = to_color(FaceColor::Red);
        // A shaded colour keeps its zero channels, which is enough to tell
        // the six apart here.
        if (face.color.r == face.color.g && face.color.g == face.color.b &&
            face.color.r > 128) {
            white = face.color;
        } else if (face.color.r == 0 && face.color.g > 100 && face.color.b > 40 &&
                   face.color.b < face.color.g) {
            if (plain_green.r == 0) green = face.color;
        } else if (face.color.r > 100 && face.color.g < 20) {
            if (plain_red.g == 18) red = face.color;
        }
        static_cast<void>(plain_white);
    }
    REQUIRE(white.has_value());
    REQUIRE(green.has_value());
    REQUIRE(red.has_value());

    // +Y at 275 out of 255: white clamps; +Z at 230 and +X at 211, on the
    // stickers' colours with their saturation raised first.
    REQUIRE(int{white->r} == 255);
    REQUIRE(int{green->g} == (165 * 230 + 127) / 255);
    REQUIRE(int{green->b} == (61 * 230 + 127) / 255);
    REQUIRE(int{red->r} == (215 * 211 + 127) / 255);
    REQUIRE(int{red->b} == (51 * 211 + 127) / 255);

    SECTION("and the same bytes for every size a person can pick")
    {
        // The top face is past full brightness at every size, so white
        // clamps to white whatever the sticker plane's exact height.
        for (const int size : {2, 4, 5, 7, 9}) {
            const ViewScene bigger = build_cube_scene(CubeState(size))  //
                                     | view(home_camera())               //
                                     | light(default_lighting(), home_camera());
            for (const auto& face : bigger.faces) {
                if (face.color.r == face.color.g && face.color.g == face.color.b &&
                    face.color.r > 128) {
                    REQUIRE(int{face.color.r} == 255);
                }
            }
        }
    }
}

TEST_CASE("a highlight whose rim crosses the near plane keeps its radius")
{
    // A face in front of the eye carrying a glint whose centre is in front
    // too, but whose rim -- a wide lobe measured along a plane tilted towards
    // the eye -- has crossed the near plane. The face is drawn, so the glint
    // on it must be as well.
    ViewFace face;
    face.points = {Vec3{-1.0f, -1.0f, -4.0f}, Vec3{1.0f, -1.0f, -4.0f},
                   Vec3{1.0f, 1.0f, -4.0f}, Vec3{-1.0f, 1.0f, -4.0f}};
    face.color = Color{255, 255, 255, 255};
    HighlightOf<Vec3> wide;
    wide.centre = Vec3{0.0f, 0.0f, -2.5f};
    wide.rims = {Vec3{0.0f, 1.0f, -0.5f}, Vec3{0.0f, -1.0f, -4.5f},
                 Vec3{1.0f, 0.0f, -2.5f}, Vec3{-1.0f, 0.0f, -2.5f}};
    wide.stops = {120, 90, 60, 30, 0};
    face.highlights.push_back(wide);
    ViewScene scene;
    scene.faces.push_back(face);

    const Camera camera = identity_camera();
    const ClipScene projected = scene | project(camera);
    REQUIRE(projected.faces.size() == 1);
    REQUIRE(projected.faces[0].highlights.size() == 1);
    const auto& glint = projected.faces[0].highlights[0];
    REQUIRE(glint.stops[0] == 120);
    REQUIRE(glint.stops[3] == 30);

    // The rim that had crossed came through as its partner's mirror image
    // through the projected centre, so the pair still spans an axis; the
    // rims that were in front are where they project to.
    REQUIRE(glint.rims[0].x == Approx(2.0f * glint.centre.x - glint.rims[1].x));
    REQUIRE(glint.rims[0].y == Approx(2.0f * glint.centre.y - glint.rims[1].y));
    REQUIRE(glint.rims[0].y != Approx(glint.rims[1].y));
    const ViewScene untouched = [&] {
        ViewScene copy = scene;
        copy.faces[0].highlights[0].rims[0] = Vec3{0.0f, -1.0f, -4.5f};
        return copy;
    }();
    const ClipScene untouched_projected = untouched | project(camera);
    const auto& reference = untouched_projected.faces[0].highlights[0];
    REQUIRE(glint.rims[1].y == Approx(reference.rims[1].y));
    REQUIRE(glint.rims[2].x == Approx(reference.rims[2].x));
    REQUIRE(glint.rims[3].x == Approx(reference.rims[3].x));

    // A pair with both ends behind the near plane has no axis to draw.
    ViewScene axis_gone = scene;
    axis_gone.faces[0].highlights[0].rims[1] = Vec3{0.0f, -1.0f, -0.5f};
    REQUIRE((axis_gone | project(camera)).faces[0].highlights.empty());

    // A glint whose centre itself is behind the near plane still cannot be
    // drawn, and is dropped as before.
    ViewScene behind = scene;
    behind.faces[0].highlights[0].centre = Vec3{0.0f, 0.0f, -0.5f};
    REQUIRE((behind | project(camera)).faces[0].highlights.empty());
}
