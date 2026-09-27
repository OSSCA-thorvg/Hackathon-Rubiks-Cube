#include "graphics/OrbitCamera.hpp"

#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "graphics/CubeGeometry.hpp"

namespace {

using Catch::Approx;
using namespace rubiks::graphics;
using rubiks::math::Vec3;

float radius_of(const OrbitCamera& view)
{
    return rubiks::math::length(view.to_camera(1.0f).eye());
}

/** The furthest the cube reaches from the middle of the viewport, in NDC. */
float silhouette_extent(const OrbitCamera& view)
{
    const Camera camera = view.to_camera(1.0f);
    const auto clip_from_world =
        rubiks::math::multiply(camera.projection(), camera.view());

    // The eight corners bound every sticker, which sits inside the cube.
    float furthest = 0.0f;
    for (int corner = 0; corner < 8; ++corner) {
        const Vec3 point{(corner & 1) ? kCubeHalfExtent : -kCubeHalfExtent,
                         (corner & 2) ? kCubeHalfExtent : -kCubeHalfExtent,
                         (corner & 4) ? kCubeHalfExtent : -kCubeHalfExtent};

        const auto clip = rubiks::math::apply_point(clip_from_world, point);
        REQUIRE(clip.w > 0.0f);

        furthest = std::max(furthest, std::abs(clip.x / clip.w));
        furthest = std::max(furthest, std::abs(clip.y / clip.w));
    }
    return furthest;
}

}  // namespace

TEST_CASE("the home viewpoint is the camera the contract was derived from")
{
    // default_camera delegates here, so this pins the delegation rather than
    // a coincidence between two ways of building the same camera.
    const Camera from_orbit = home_orbit().to_camera(1.5f);
    const Camera pinned = default_camera(1.5f);

    REQUIRE(from_orbit.eye().x == pinned.eye().x);
    REQUIRE(from_orbit.eye().y == pinned.eye().y);
    REQUIRE(from_orbit.eye().z == pinned.eye().z);
    REQUIRE(from_orbit.near_plane() == pinned.near_plane());

    const auto view = from_orbit.view();
    const auto expected_view = pinned.view();
    REQUIRE(view.x.x == expected_view.x.x);
    REQUIRE(view.w.z == expected_view.w.z);
}

TEST_CASE("the home eye lands on the diagonal it is defined by")
{
    const Vec3 eye = home_orbit().to_camera(1.0f).eye();

    // Reconstructing the eye through angles costs about one unit in the last
    // place, which is far below anything the rendered scene can show.
    REQUIRE(eye.x == Approx(kHomeEye.x).margin(1e-5f));
    REQUIRE(eye.y == Approx(kHomeEye.y).margin(1e-5f));
    REQUIRE(eye.z == Approx(kHomeEye.z).margin(1e-5f));

    REQUIRE(home_orbit().yaw_degrees == Approx(45.0f));
    REQUIRE(home_orbit().pitch_degrees == Approx(35.264389f).margin(1e-4f));
}

TEST_CASE("yaw walks the eye around the cube")
{
    const float radius = radius_of(home_orbit());

    // Zero looks straight at the +Z face, and a quarter turn brings +X round.
    const Vec3 front = OrbitCamera{0.0f, 0.0f}.to_camera(1.0f).eye();
    REQUIRE(front.x == Approx(0.0f).margin(1e-4f));
    REQUIRE(front.z == Approx(radius).margin(1e-4f));

    const Vec3 right = OrbitCamera{90.0f, 0.0f}.to_camera(1.0f).eye();
    REQUIRE(right.x == Approx(radius).margin(1e-4f));
    REQUIRE(right.z == Approx(0.0f).margin(1e-4f));

    const Vec3 back = OrbitCamera{180.0f, 0.0f}.to_camera(1.0f).eye();
    REQUIRE(back.z == Approx(-radius).margin(1e-4f));
}

TEST_CASE("every viewpoint keeps the same distance from the cube")
{
    const float expected = radius_of(home_orbit());

    for (float yaw = 0.0f; yaw < 360.0f; yaw += 37.0f) {
        for (float pitch = -kPitchLimitDegrees; pitch <= kPitchLimitDegrees;
             pitch += 20.0f) {
            REQUIRE(radius_of(OrbitCamera{yaw, pitch}) ==
                    Approx(expected).margin(1e-4f));
        }
    }
}

TEST_CASE("pitch stops at the limit and leaves again immediately")
{
    OrbitCamera view = home_orbit();

    view.turn(0.0f, 1000.0f);
    REQUIRE(view.pitch_degrees == Approx(kPitchLimitDegrees));

    // Clamping the stored angle rather than the drag is what makes this work:
    // had the total displacement been kept, the viewpoint would sit still
    // until the drag retraced everything it spent beyond the limit.
    view.turn(0.0f, -5.0f);
    REQUIRE(view.pitch_degrees == Approx(kPitchLimitDegrees - 5.0f));

    view.turn(0.0f, -1000.0f);
    REQUIRE(view.pitch_degrees == Approx(-kPitchLimitDegrees));

    view.turn(0.0f, 5.0f);
    REQUIRE(view.pitch_degrees == Approx(-kPitchLimitDegrees + 5.0f));
}

TEST_CASE("yaw wraps instead of growing without bound")
{
    OrbitCamera view{0.0f, 0.0f};

    view.turn(370.0f, 0.0f);
    REQUIRE(view.yaw_degrees == Approx(10.0f));

    view.turn(-30.0f, 0.0f);
    REQUIRE(view.yaw_degrees == Approx(340.0f));

    view.turn(-720.0f, 0.0f);
    REQUIRE(view.yaw_degrees == Approx(340.0f));
    REQUIRE(view.yaw_degrees >= 0.0f);
    REQUIRE(view.yaw_degrees < 360.0f);

    // A hair below zero, which a float would round up to 360 on the way back.
    OrbitCamera edge{0.0f, 0.0f};
    edge.turn(-1e-6f, 0.0f);
    REQUIRE(edge.yaw_degrees >= 0.0f);
    REQUIRE(edge.yaw_degrees < 360.0f);
}

TEST_CASE("the cube stays inside the viewport from every viewpoint")
{
    // The silhouette is widest edge on rather than corner on, so orbiting
    // reaches views a little larger than the home one. This sweeps the whole
    // reachable range instead of re-deriving the worst case by hand.
    float worst = 0.0f;
    for (int yaw = 0; yaw < 360; yaw += 3) {
        for (int pitch = -static_cast<int>(kPitchLimitDegrees);
             pitch <= static_cast<int>(kPitchLimitDegrees); pitch += 2) {
            worst = std::max(worst,
                             silhouette_extent(OrbitCamera{
                                 static_cast<float>(yaw),
                                 static_cast<float>(pitch)}));
        }
    }

    // Comfortably inside the viewport, so the camera distance needs no
    // adjusting for orbiting.
    REQUIRE(worst < 0.95f);
}
