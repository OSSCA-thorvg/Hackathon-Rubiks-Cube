#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <tuple>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"
#include "graphics/CubeGeometry.hpp"

namespace {

using Catch::Approx;
using namespace rubiks::graphics;
using rubiks::cube::Axis;
using rubiks::cube::CubeState;
using rubiks::math::Vec3;

constexpr int kSize = 3;

std::size_t count_body_faces(const WorldScene& scene)
{
    std::size_t total = 0;
    for (const auto& face : scene.faces) {
        if (face.color == kBodyColor) ++total;
    }
    return total;
}

/**
 * A quad reduced to something comparable: where its center sits and what
 * color it is, both rounded so a rotation by a right angle compares equal
 * despite the sine of ninety degrees not being exactly one.
 */
using Key = std::tuple<int, int, int, int>;

Key key_of(const WorldFace& face)
{
    Vec3 center{0.0f, 0.0f, 0.0f};
    for (const auto& point : face.points) {
        center.x += point.x * 0.25f;
        center.y += point.y * 0.25f;
        center.z += point.z * 0.25f;
    }

    const auto rounded = [](float value) {
        return static_cast<int>(std::lround(value * 1000.0f));
    };
    return Key{rounded(center.x), rounded(center.y), rounded(center.z),
               static_cast<int>(face.color.r) * 65536 +
                   static_cast<int>(face.color.g) * 256 +
                   static_cast<int>(face.color.b)};
}

/** The sticker quads of a scene, keyed by position and color. */
std::map<Key, int> sticker_keys(const WorldScene& scene)
{
    std::map<Key, int> keys;
    for (const auto& face : scene.faces) {
        if (face.color == kBodyColor) continue;
        ++keys[key_of(face)];
    }
    return keys;
}

ActiveRotation turning_right_layer(float degrees)
{
    return ActiveRotation{Axis::X, rubiks::cube::layer(kSize - 1), degrees};
}

}  // namespace

TEST_CASE("without a turn the scene is the one built for a cube at rest")
{
    const CubeState state(kSize);

    const WorldScene resting = build_cube_scene(state);
    const WorldScene same = build_cube_scene(state, std::nullopt);

    // Identical face for face and corner for corner, because the overload
    // hands the work to the resting builder rather than reproducing it. That
    // is what keeps the rendered scene contract valid unchanged.
    REQUIRE(same.faces.size() == resting.faces.size());
    for (std::size_t i = 0; i < resting.faces.size(); ++i) {
        REQUIRE(same.faces[i].color == resting.faces[i].color);
        for (std::size_t corner = 0; corner < 4; ++corner) {
            REQUIRE(same.faces[i].points[corner].x ==
                    Approx(resting.faces[i].points[corner].x));
            REQUIRE(same.faces[i].points[corner].y ==
                    Approx(resting.faces[i].points[corner].y));
            REQUIRE(same.faces[i].points[corner].z ==
                    Approx(resting.faces[i].points[corner].z));
        }
    }
}

TEST_CASE("a turning layer exposes two cut sheets")
{
    const CubeState state(kSize);
    const WorldScene turning =
        build_cube_scene(state, turning_right_layer(45.0f));

    // The 54 stickers, plus the two faces of the cut: one sheet on the layer
    // that moves and one on the layer it left behind.
    REQUIRE(turning.faces.size() == 54 + 2 * kSize * kSize);
    REQUIRE(count_body_faces(turning) == 2 * kSize * kSize);
    REQUIRE(build_cube_scene(state).faces.size() == 54);
    REQUIRE(count_body_faces(build_cube_scene(state)) == 0);
}

TEST_CASE("a turn of the whole cube cuts nothing")
{
    const CubeState state(kSize);
    const ActiveRotation everything{Axis::Y,
                                    rubiks::cube::layers_through(0, kSize - 1),
                                    30.0f};

    // Nothing is left behind, so no inside is revealed.
    REQUIRE(count_body_faces(build_cube_scene(state, everything)) == 0);
}

TEST_CASE("a wide turn cuts only where it meets a still layer")
{
    const CubeState state(kSize);
    const ActiveRotation wide{Axis::X, rubiks::cube::layers_through(1, 2),
                              30.0f};

    REQUIRE(count_body_faces(build_cube_scene(state, wide)) ==
            2 * kSize * kSize);
}

TEST_CASE("the cut is filled rather than left open")
{
    const CubeState state(kSize);
    const WorldScene turning =
        build_cube_scene(state, turning_right_layer(45.0f));

    // The body color has to be distinguishable from the background and from
    // every sticker, or a turn would look like a hole.
    for (const auto& face : turning.faces) {
        if (face.color != kBodyColor) continue;
        REQUIRE(face.color.a == 255);
    }
    REQUIRE(kBodyColor != Color{32, 32, 32, 255});
}

TEST_CASE("turning a layer a quarter of the way draws the turned cube")
{
    // The strongest statement about the sign convention that geometry can
    // make: a layer drawn at ninety degrees has to look exactly like the same
    // cube after the domain applies that quarter turn.
    CubeState state(kSize);

    const WorldScene mid_turn =
        build_cube_scene(state, turning_right_layer(90.0f));

    CubeState turned(kSize);
    turned.apply(rubiks::cube::moves::R(kSize));
    const WorldScene settled = build_cube_scene(turned);

    REQUIRE(sticker_keys(mid_turn) == sticker_keys(settled));
}

TEST_CASE("turning backwards draws the inverse move")
{
    CubeState state(kSize);

    const WorldScene mid_turn =
        build_cube_scene(state, turning_right_layer(-90.0f));

    CubeState turned(kSize);
    turned.apply(inverse(rubiks::cube::moves::R(kSize)));

    REQUIRE(sticker_keys(mid_turn) == sticker_keys(build_cube_scene(turned)));
}

TEST_CASE("only the selected layer moves")
{
    const CubeState state(kSize);

    const WorldScene resting = build_cube_scene(state);
    const WorldScene turning =
        build_cube_scene(state, turning_right_layer(30.0f));

    const auto still_at_rest = sticker_keys(resting);
    const auto during = sticker_keys(turning);

    std::size_t moved = 0;
    for (const auto& entry : still_at_rest) {
        if (during.count(entry.first) == 0) ++moved;
    }

    // The right layer carries twenty-one stickers: nine on its own face and
    // three along each of the four faces it borders. Twenty of them travel,
    // the exception being the center of the right face, which is centered on
    // the axis and so turns in place.
    REQUIRE(moved == 20);
}
