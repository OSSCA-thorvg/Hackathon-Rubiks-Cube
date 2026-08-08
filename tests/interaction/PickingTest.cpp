#include "interaction/Picking.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "graphics/CubeGeometry.hpp"
#include "graphics/Layout.hpp"

namespace {

using namespace rubiks::interaction;
using rubiks::cube::Axis;
using rubiks::cube::Face;
using rubiks::graphics::Camera;
using rubiks::graphics::Rect;
using rubiks::math::Vec3;

constexpr int kSize = 3;
constexpr std::uint32_t kCanvas = 1024;

Rect cube_rect()
{
    return rubiks::graphics::layout(kCanvas, kCanvas).cube;
}

Camera cube_camera()
{
    const Rect rect = cube_rect();
    return rubiks::graphics::default_camera(rect.width / rect.height);
}

/** A point at a fraction of the 3D viewport, as the contract measures them. */
float sample_x(float fraction)
{
    const Rect rect = cube_rect();
    return rect.x + fraction * rect.width;
}

float sample_y(float fraction)
{
    const Rect rect = cube_rect();
    return rect.y + fraction * rect.height;
}

/** Picks at a fraction of the viewport, or nothing when the ray misses. */
std::optional<Pick> pick_at(float fx, float fy)
{
    const auto ray = pointer_ray(sample_x(fx), sample_y(fy), cube_camera(),
                                 cube_rect());
    if (!ray) return std::nullopt;
    return pick_cube(*ray, kSize);
}

/** A ray travelling in -X towards the right face, aimed at (y, z). */
Ray ray_at_right_face(float y, float z)
{
    return Ray{Vec3{5.0f, y, z}, Vec3{-1.0f, 0.0f, 0.0f}};
}

}  // namespace

TEST_CASE("the contract samples pick the center sticker of each visible face")
{
    // These are the three points the rendered scene contract reads as white,
    // green and red, so picking has to agree with what is drawn there.
    const auto up = pick_at(0.50f, 0.29f);
    REQUIRE(up);
    REQUIRE(up->face == Face::Up);
    REQUIRE(up->x == 1);
    REQUIRE(up->y == 2);
    REQUIRE(up->z == 1);

    const auto front = pick_at(0.31f, 0.61f);
    REQUIRE(front);
    REQUIRE(front->face == Face::Front);
    REQUIRE(front->x == 1);
    REQUIRE(front->y == 1);
    REQUIRE(front->z == 2);

    const auto right = pick_at(0.69f, 0.61f);
    REQUIRE(right);
    REQUIRE(right->face == Face::Right);
    REQUIRE(right->x == 2);
    REQUIRE(right->y == 1);
    REQUIRE(right->z == 1);
}

TEST_CASE("a seam is grabbed rather than missed")
{
    // Seam samples show the background, but nothing is cut out of the cube:
    // a press between two stickers still holds the cube.
    constexpr std::array<std::array<float, 2>, 3> kSeams{
        {{0.377f, 0.645f}, {0.313f, 0.534f}, {0.564f, 0.321f}}};

    for (const auto& seam : kSeams) {
        REQUIRE(pick_at(seam[0], seam[1]));
    }
}

TEST_CASE("pressing beside the cube misses")
{
    // The corners of the square viewport are outside the silhouette.
    REQUIRE_FALSE(pick_at(0.02f, 0.02f));
    REQUIRE_FALSE(pick_at(0.98f, 0.02f));
    REQUIRE_FALSE(pick_at(0.02f, 0.98f));
    REQUIRE_FALSE(pick_at(0.98f, 0.98f));
}

TEST_CASE("points outside the viewport produce no ray")
{
    const Rect rect = cube_rect();
    const Camera camera = cube_camera();

    REQUIRE_FALSE(pointer_ray(rect.x - 1.0f, rect.y + 1.0f, camera, rect));
    REQUIRE_FALSE(pointer_ray(rect.x + 1.0f, rect.y - 1.0f, camera, rect));
    REQUIRE_FALSE(
        pointer_ray(rect.x + rect.width + 1.0f, rect.y, camera, rect));
    REQUIRE_FALSE(
        pointer_ray(rect.x, rect.y + rect.height + 1.0f, camera, rect));

    // The net sits below the 3D region, so pressing it is a miss here.
    const Rect net = rubiks::graphics::layout(kCanvas, kCanvas).net;
    REQUIRE_FALSE(pointer_ray(net.x + net.width * 0.5f,
                              net.y + net.height * 0.5f, camera, rect));
}

TEST_CASE("non-finite coordinates produce no ray")
{
    const Rect rect = cube_rect();
    const Camera camera = cube_camera();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();

    REQUIRE_FALSE(pointer_ray(nan, rect.y, camera, rect));
    REQUIRE_FALSE(pointer_ray(rect.x, nan, camera, rect));
    REQUIRE_FALSE(pointer_ray(infinity, rect.y, camera, rect));
    REQUIRE_FALSE(pointer_ray(rect.x, -infinity, camera, rect));
}

TEST_CASE("the far edge of a face belongs to the last cell")
{
    // Without the clamp this corner would index one cell past the end.
    const auto corner = pick_cube(ray_at_right_face(1.0f, 1.0f), kSize);
    REQUIRE(corner);
    REQUIRE(corner->face == Face::Right);
    REQUIRE(corner->x == kSize - 1);
    REQUIRE(corner->y == kSize - 1);
    REQUIRE(corner->z == kSize - 1);

    const auto opposite = pick_cube(ray_at_right_face(-1.0f, -1.0f), kSize);
    REQUIRE(opposite);
    REQUIRE(opposite->y == 0);
    REQUIRE(opposite->z == 0);
}

TEST_CASE("cell boundaries are half-open towards the larger index")
{
    // Cells are 2/3 wide at N = 3, so the first boundary sits at y = -1/3.
    constexpr float kBoundary = -1.0f / 3.0f;
    constexpr float kNudge = 1e-3f;

    const auto below = pick_cube(ray_at_right_face(kBoundary - kNudge, 0.0f),
                                 kSize);
    REQUIRE(below);
    REQUIRE(below->y == 0);

    const auto above = pick_cube(ray_at_right_face(kBoundary + kNudge, 0.0f),
                                 kSize);
    REQUIRE(above);
    REQUIRE(above->y == 1);
}

TEST_CASE("a ray leaving the cube behind it does not hit")
{
    // Pointing away from the cube: the plane intersections are all behind the
    // origin, which the positive-distance requirement rejects.
    REQUIRE_FALSE(
        pick_cube(Ray{Vec3{5.0f, 0.0f, 0.0f}, Vec3{1.0f, 0.0f, 0.0f}}, kSize));
}

TEST_CASE("a face reports the axis it points along")
{
    REQUIRE(axis_of(Face::Right) == Axis::X);
    REQUIRE(axis_of(Face::Left) == Axis::X);
    REQUIRE(axis_of(Face::Up) == Axis::Y);
    REQUIRE(axis_of(Face::Down) == Axis::Y);
    REQUIRE(axis_of(Face::Front) == Axis::Z);
    REQUIRE(axis_of(Face::Back) == Axis::Z);

    const Pick pick{Face::Front, 2, 1, 0, Vec3{}};
    REQUIRE(layer_of(pick, Axis::X) == 2);
    REQUIRE(layer_of(pick, Axis::Y) == 1);
    REQUIRE(layer_of(pick, Axis::Z) == 0);
}
