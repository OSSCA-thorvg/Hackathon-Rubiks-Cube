#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
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
                            | light(kDefaultLight, camera);
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
    REQUIRE(shade(color, 2.0f) == color);  // clamped, not brightened

    // (value * 153 + 127) / 255 at an intensity of 0.6.
    const Color dim = shade(color, 0.6f);
    REQUIRE(dim.r == (183 * 153 + 127) / 255);
    REQUIRE(dim.g == (18 * 153 + 127) / 255);
    REQUIRE(dim.b == (52 * 153 + 127) / 255);
    REQUIRE(dim.a == 255);

    REQUIRE(shade(color, 0.0f) == Color{0, 0, 0, 255});
}

TEST_CASE("the unlit light hands every colour back byte for byte")
{
    const Light none = Light::unlit();
    for (const auto palette : {Palette::Classic, Palette::HighContrast}) {
        for (int i = 0; i < 6; ++i) {
            const Color color = to_color(static_cast<FaceColor>(i), palette);
            REQUIRE(shade(color, none.ambient + none.diffuse) == color);
        }
    }
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
    shadow.contact = {face.points[1], face.points[3]};
    Caster box;
    box.corners.fill(face.points[0]);
    shadow.occluders.push_back(box.corners);
    shadow.opacity = 77;
    scene.shadow = shadow;

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
    REQUIRE(result.shadow->contact.size() == 2);
    REQUIRE(result.shadow->opacity == 77);

    // The viewport pass is the one that knows the rectangle.
    REQUIRE(result.shadow->clip.x == Approx(region.x));
    REQUIRE(result.shadow->clip.width == Approx(region.width));
    REQUIRE(result.shadow->blur_sigma == Approx(kShadowBlurShare * region.width));

    // A box collapsed to a point has no silhouette worth the name.
    REQUIRE(result.shadow->occluders.size() == 1);
    REQUIRE(result.shadow->occluders[0].size() == 1);

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
    Light lamp = kDefaultLight;
    lamp.position = Vec3{0.0f, 0.0f, 0.0f};  // at the eye

    ViewScene scene;
    scene.faces.push_back(facing_face(3.0f, true));
    scene.faces.push_back(facing_face(3.0f, false));
    const ViewScene lit = light(lamp, camera)(scene);

    // Straight on: ambient + diffuse = 1, so the colour is untouched.
    REQUIRE(lit.faces[0].color == Color{200, 100, 50, 255});
    // Facing away: exactly the ambient floor.
    REQUIRE(lit.faces[1].color == shade(Color{200, 100, 50, 255}, lamp.ambient));

    SECTION("the unlit light changes nothing")
    {
        const ViewScene plain = light(Light::unlit(), camera)(scene);
        REQUIRE(plain.faces[0].color == Color{200, 100, 50, 255});
        REQUIRE(plain.faces[1].color == Color{200, 100, 50, 255});
        REQUIRE_FALSE(plain.faces[0].highlight.has_value());
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
                            | light(kDefaultLight, home_camera());
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

TEST_CASE("the home viewpoint's faces carry no highlight")
{
    // From the home eye the mirror image of the light lands outside every
    // face that can be seen, so the contract's sample faces are plain
    // shaded colour with no glint to reproduce.
    const ViewScene scene = build_cube_scene(CubeState(3))     //
                            | view(home_camera())              //
                            | light(kDefaultLight, home_camera());
    for (const auto& face : scene.faces) {
        REQUIRE_FALSE(face.highlight.has_value());
    }
}

TEST_CASE("a highlight sits where the light reflects into the eye")
{
    const Camera camera = identity_camera();
    ViewScene scene;
    scene.faces.push_back(facing_face(2.0f, true));

    SECTION("inside the face, at the mirror point")
    {
        Light lamp = kDefaultLight;
        lamp.position = Vec3{0.5f, 0.5f, 0.0f};
        const ViewScene lit = light(lamp, camera)(scene);

        REQUIRE(lit.faces[0].highlight.has_value());
        const auto& glint = *lit.faces[0].highlight;
        // Mirrored to (0.5, 0.5, -4); half way back to the eye is z = -2.
        REQUIRE(glint.centre.x == Approx(0.25f).margin(1e-4));
        REQUIRE(glint.centre.y == Approx(0.25f).margin(1e-4));
        REQUIRE(glint.centre.z == Approx(-2.0f).margin(1e-4));
        // A perfect reflection: the whole specular strength.
        REQUIRE(glint.alpha == std::lround(lamp.specular * 255.0f));
        // The rim is one radius away along the face.
        const Vec3 span = glint.rim - glint.centre;
        REQUIRE(rubiks::math::length(span) == Approx(kHighlightRadius));
        REQUIRE(span.z == Approx(0.0f));
    }

    SECTION("pulled onto the edge when the mirror point falls outside")
    {
        Light lamp = kDefaultLight;
        lamp.position = Vec3{5.0f, 0.0f, 0.0f};
        const ViewScene lit = light(lamp, camera)(scene);

        REQUIRE(lit.faces[0].highlight.has_value());
        REQUIRE(lit.faces[0].highlight->centre.x == Approx(1.0f));
        REQUIRE(lit.faces[0].highlight->alpha > 0);
        REQUIRE(lit.faces[0].highlight->alpha <
                std::lround(lamp.specular * 255.0f));
    }

    SECTION("and gone when the mirror point is far away")
    {
        Light lamp = kDefaultLight;
        lamp.position = Vec3{40.0f, 0.0f, 0.0f};
        REQUIRE_FALSE(light(lamp, camera)(scene).faces[0].highlight.has_value());
    }

    SECTION("a face lit from behind carries none")
    {
        Light lamp = kDefaultLight;
        lamp.position = Vec3{0.0f, 0.0f, -5.0f};
        REQUIRE_FALSE(light(lamp, camera)(scene).faces[0].highlight.has_value());
    }
}

TEST_CASE("the six sticker colours stay apart at both ends of the shading")
{
    for (const auto palette : {Palette::Classic, Palette::HighContrast}) {
        for (const float intensity : {kDefaultLight.ambient, 1.0f}) {
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
        build_cube_scene(CubeState(3)) | shadow(kDefaultLight, home_camera());

    REQUIRE(scene.shadow.has_value());
    const auto& cast = *scene.shadow;
    REQUIRE(cast.polygons.size() == 1);
    REQUIRE(cast.occluders.size() == 1);
    REQUIRE(cast.contact.size() == 16);
    REQUIRE(cast.opacity == kShadowOpacity);

    // Everything lies on the ground plane, and the polygon is its own hull.
    std::vector<Vec2> footprint;
    for (const auto& point : cast.polygons[0]) {
        REQUIRE(point.y == Approx(kGroundY));
        footprint.push_back(Vec2{point.x, point.z});
    }
    REQUIRE(footprint.size() >= 4);
    REQUIRE(convex_hull(footprint).size() == footprint.size());
    for (const auto& point : cast.contact) REQUIRE(point.y == Approx(kGroundY));

    // The anchor is the cube's centre dropped straight down; the far anchor
    // is away from the light, which sits at +X +Z of the cube.
    REQUIRE(cast.fade_start.x == Approx(0.0f).margin(1e-5));
    REQUIRE(cast.fade_start.z == Approx(0.0f).margin(1e-5));
    REQUIRE(cast.fade_end.x < cast.fade_start.x);
    REQUIRE(cast.fade_end.z < cast.fade_start.z);
}

TEST_CASE("a light straight overhead widens the shadow by its height")
{
    Light lamp = kDefaultLight;
    lamp.position = Vec3{0.0f, 4.5f, 0.0f};
    const WorldScene scene =
        build_cube_scene(CubeState(3)) | shadow(lamp, home_camera());
    REQUIRE(scene.shadow.has_value());

    // A point light at a finite height does not trace the footprint: the top
    // corners at y = 1 are pushed out by (4.5 + 1.5) / (4.5 - 1) and are the
    // whole outline. The centre and axes are the cube's own.
    const float scale = (lamp.position.y - kGroundY) / (lamp.position.y - 1.0f);
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
    Light lamp = kDefaultLight;
    lamp.position = Vec3{2.0f, 0.5f, 2.0f};
    const WorldScene scene =
        build_cube_scene(CubeState(3)) | shadow(lamp, home_camera());
    REQUIRE_FALSE(scene.shadow.has_value());
}

TEST_CASE("the shadow thins as the eye nears the ground and is gone below it")
{
    const auto camera_at = [](float eye_y) {
        return Camera{Vec3{4.0f, eye_y, 4.0f}, kCubeCenter, kWorldUp,
                      kVerticalFov, 1.0f, kNearPlane, kFarPlane};
    };
    const WorldScene world = build_cube_scene(CubeState(3));

    REQUIRE((world | shadow(kDefaultLight, camera_at(3.0f))).shadow->opacity ==
            kShadowOpacity);

    const auto half = world | shadow(kDefaultLight,
                                     camera_at(kGroundY + 0.5f * kShadowFadeBand));
    REQUIRE(half.shadow.has_value());
    REQUIRE(half.shadow->opacity == std::lround(kShadowOpacity * 0.5f));

    REQUIRE_FALSE((world | shadow(kDefaultLight, camera_at(kGroundY))).shadow.has_value());
    REQUIRE_FALSE((world | shadow(kDefaultLight, camera_at(-3.0f))).shadow.has_value());
}

TEST_CASE("a turning layer's shadow turns with it")
{
    const ActiveRotation turn{Axis::X, rubiks::cube::layer(2), 45.0f};
    const WorldScene scene = build_cube_scene(CubeState(3), turn) |
                             shadow(kDefaultLight, home_camera());

    REQUIRE(scene.shadow.has_value());
    REQUIRE(scene.shadow->polygons.size() == 2);
    REQUIRE(scene.shadow->occluders.size() == 2);

    // The same turn at zero degrees: the still slab's shadow is the same, and
    // the turning slab's is not.
    const ActiveRotation settled{Axis::X, rubiks::cube::layer(2), 0.0f};
    const WorldScene rest = build_cube_scene(CubeState(3), settled) |
                            shadow(kDefaultLight, home_camera());
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
                              | shadow(kDefaultLight, camera) //
                              | view(camera)                  //
                              | light(kDefaultLight, camera)  //
                              | project(camera)               //
                              | cull()                        //
                              | depth_sort()                  //
                              | viewport(placement.cube);

    REQUIRE(scene.faces.size() == 27);
    REQUIRE(scene.shadow.has_value());
    REQUIRE(scene.shadow->occluders.size() == 1);
    // A box seen corner-on is a hexagon.
    REQUIRE(scene.shadow->occluders[0].size() == 6);

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
                            | light(kDefaultLight, home_camera());

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

    REQUIRE(int{white->r} == 215);
    REQUIRE(int{green->g} == (155 * 206 + 127) / 255);
    REQUIRE(int{green->b} == (72 * 206 + 127) / 255);
    REQUIRE(int{red->r} == (183 * 177 + 127) / 255);
    REQUIRE(int{red->b} == (52 * 177 + 127) / 255);

    SECTION("and the same bytes for every size a person can pick")
    {
        // The sticker plane sits a little further out on a bigger cube, and
        // the light was placed so that never tips a byte between 2 and 9.
        for (const int size : {2, 4, 5, 7, 9}) {
            const ViewScene bigger = build_cube_scene(CubeState(size))  //
                                     | view(home_camera())               //
                                     | light(kDefaultLight, home_camera());
            for (const auto& face : bigger.faces) {
                if (face.color.r == face.color.g && face.color.g == face.color.b &&
                    face.color.r > 128) {
                    REQUIRE(int{face.color.r} == 215);
                }
            }
        }
    }
}
