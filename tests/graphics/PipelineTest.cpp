#include <array>
#include <cstddef>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "graphics/Camera.hpp"
#include "graphics/CubeGeometry.hpp"
#include "graphics/Passes.hpp"
#include "graphics/Pipeline.hpp"
#include "math/Transform.hpp"

// Linked against the graphics library alone: if the pipeline ever picks up a
// ThorVG include, this target stops building.

namespace {

using Catch::Approx;
using namespace rubiks::graphics;
using rubiks::math::Vec2;
using rubiks::math::Vec3;

constexpr float kPi = 3.14159265358979323846f;

/** Square camera with a 90 degree vertical field of view at the origin. */
Camera test_camera()
{
    return Camera{Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 0.0f, -1.0f},
                  Vec3{0.0f, 1.0f, 0.0f}, kPi / 2.0f, 1.0f, 1.0f, 10.0f};
}

ClipFace clip_face(const std::array<Vec2, 4>& ndc, float depth, Color color = {})
{
    ClipFace face;
    face.ndc = ndc;
    face.depth = depth;
    face.color = color;
    return face;
}

/** True when `point` is inside a convex quad of either winding. */
bool contains(const std::array<Vec2, 4>& quad, const Vec2& point)
{
    bool positive = false;
    bool negative = false;
    for (std::size_t i = 0; i < 4; ++i) {
        const Vec2 edge{quad[(i + 1) % 4].x - quad[i].x,
                        quad[(i + 1) % 4].y - quad[i].y};
        const Vec2 offset{point.x - quad[i].x, point.y - quad[i].y};
        const float cross = edge.x * offset.y - edge.y * offset.x;
        if (cross > 0.0f) positive = true;
        if (cross < 0.0f) negative = true;
    }
    return !(positive && negative);
}

const RenderFace* find_face(const RenderScene& scene, const Color& color)
{
    for (const auto& face : scene.faces) {
        if (face.color == color) return &face;
    }
    return nullptr;
}

}  // namespace

TEST_CASE("build_scene emits one cube whose faces wind outward")
{
    const WorldScene scene = build_scene();
    REQUIRE(scene.faces.size() == 6);

    // Every face normal, taken from the first two edges, must point away from
    // the cube center; that is what counter-clockwise-from-outside means.
    for (const auto& face : scene.faces) {
        const Vec3 first{face.points[1].x - face.points[0].x,
                         face.points[1].y - face.points[0].y,
                         face.points[1].z - face.points[0].z};
        const Vec3 second{face.points[2].x - face.points[1].x,
                          face.points[2].y - face.points[1].y,
                          face.points[2].z - face.points[1].z};
        const Vec3 normal = rubiks::math::cross(first, second);

        Vec3 centroid{0.0f, 0.0f, 0.0f};
        for (const auto& point : face.points) {
            centroid = Vec3{centroid.x + point.x * 0.25f,
                            centroid.y + point.y * 0.25f,
                            centroid.z + point.z * 0.25f};
        }
        REQUIRE(rubiks::math::dot(normal, centroid) > 0.0f);
    }
}

TEST_CASE("append_cube gives each face its own color")
{
    CubeFaceColors colors;
    colors.right = Color{1, 0, 0, 255};
    colors.left = Color{2, 0, 0, 255};
    colors.up = Color{3, 0, 0, 255};
    colors.down = Color{4, 0, 0, 255};
    colors.front = Color{5, 0, 0, 255};
    colors.back = Color{6, 0, 0, 255};

    WorldScene scene;
    append_cube(scene, Vec3{0.0f, 0.0f, 0.0f}, 0.5f, colors);

    REQUIRE(scene.faces.size() == 6);
    for (std::uint8_t expected = 1; expected <= 6; ++expected) {
        REQUIRE(scene.faces[expected - 1].color.r == expected);
    }
}

TEST_CASE("project maps view space to NDC and records the depth key")
{
    ViewScene scene;
    ViewFace face;
    // Square two units in front of the camera; at 90 degrees the half-extent
    // equals the depth, so these corners land on the NDC corners.
    face.points = {Vec3{-2.0f, -2.0f, -2.0f}, Vec3{2.0f, -2.0f, -2.0f},
                   Vec3{2.0f, 2.0f, -2.0f}, Vec3{-2.0f, 2.0f, -2.0f}};
    scene.faces.push_back(face);

    const ClipScene projected = project(test_camera())(scene);
    REQUIRE(projected.faces.size() == 1);

    const auto& ndc = projected.faces[0].ndc;
    REQUIRE(ndc[0].x == Approx(-1.0f).margin(1e-5));
    REQUIRE(ndc[0].y == Approx(-1.0f).margin(1e-5));
    REQUIRE(ndc[2].x == Approx(1.0f).margin(1e-5));
    REQUIRE(ndc[2].y == Approx(1.0f).margin(1e-5));

    // Depth stays in view space, so it is the mean z rather than an NDC value.
    REQUIRE(projected.faces[0].depth == Approx(-2.0f).margin(1e-5));

    SECTION("closer geometry projects larger")
    {
        ViewScene closer = scene;
        for (auto& point : closer.faces[0].points) point.z = -4.0f;

        const ClipScene result = project(test_camera())(closer);
        REQUIRE(result.faces.size() == 1);
        REQUIRE(result.faces[0].ndc[2].x == Approx(0.5f).margin(1e-5));
    }
}

TEST_CASE("project drops faces that reach the near plane")
{
    const Camera camera = test_camera();  // near plane at 1.0

    ViewScene scene;
    ViewFace face;
    face.points = {Vec3{-1.0f, -1.0f, -2.0f}, Vec3{1.0f, -1.0f, -2.0f},
                   Vec3{1.0f, 1.0f, -2.0f}, Vec3{-1.0f, 1.0f, -2.0f}};
    scene.faces.push_back(face);
    REQUIRE(project(camera)(scene).faces.size() == 1);

    SECTION("a single vertex in front of the near plane removes the face")
    {
        scene.faces[0].points[0].z = -0.5f;
        REQUIRE(project(camera)(scene).faces.empty());
    }

    SECTION("a vertex exactly on the near plane removes the face")
    {
        scene.faces[0].points[0].z = -1.0f;
        REQUIRE(project(camera)(scene).faces.empty());
    }

    SECTION("geometry behind the camera removes the face")
    {
        scene.faces[0].points[0].z = 3.0f;
        REQUIRE(project(camera)(scene).faces.empty());
    }
}

TEST_CASE("cull keeps counter-clockwise faces only")
{
    const std::array<Vec2, 4> counter_clockwise{
        Vec2{-1.0f, -1.0f}, Vec2{1.0f, -1.0f}, Vec2{1.0f, 1.0f},
        Vec2{-1.0f, 1.0f}};
    const std::array<Vec2, 4> clockwise{Vec2{-1.0f, 1.0f}, Vec2{1.0f, 1.0f},
                                        Vec2{1.0f, -1.0f}, Vec2{-1.0f, -1.0f}};
    const std::array<Vec2, 4> edge_on{Vec2{-1.0f, 0.0f}, Vec2{1.0f, 0.0f},
                                      Vec2{1.0f, 0.0f}, Vec2{-1.0f, 0.0f}};

    ClipScene scene;
    scene.faces.push_back(clip_face(counter_clockwise, -5.0f, Color{7, 0, 0, 255}));
    scene.faces.push_back(clip_face(clockwise, -5.0f));
    scene.faces.push_back(clip_face(edge_on, -5.0f));

    const ClipScene culled = cull()(scene);
    REQUIRE(culled.faces.size() == 1);
    REQUIRE(culled.faces[0].color.r == 7);
}

TEST_CASE("depth_sort orders faces back to front and is stable")
{
    const std::array<Vec2, 4> quad{Vec2{0.0f, 0.0f}, Vec2{1.0f, 0.0f},
                                   Vec2{1.0f, 1.0f}, Vec2{0.0f, 1.0f}};

    ClipScene scene;
    scene.faces.push_back(clip_face(quad, -2.0f, Color{1, 0, 0, 255}));
    scene.faces.push_back(clip_face(quad, -8.0f, Color{2, 0, 0, 255}));
    scene.faces.push_back(clip_face(quad, -5.0f, Color{3, 0, 0, 255}));

    const ClipScene sorted = depth_sort()(scene);
    REQUIRE(sorted.faces.size() == 3);

    // More negative view-space z is farther, so it must be drawn first.
    REQUIRE(sorted.faces[0].color.r == 2);
    REQUIRE(sorted.faces[1].color.r == 3);
    REQUIRE(sorted.faces[2].color.r == 1);

    SECTION("equal depths keep their input order")
    {
        ClipScene tied;
        tied.faces.push_back(clip_face(quad, -4.0f, Color{10, 0, 0, 255}));
        tied.faces.push_back(clip_face(quad, -4.0f, Color{20, 0, 0, 255}));
        tied.faces.push_back(clip_face(quad, -4.0f, Color{30, 0, 0, 255}));

        const ClipScene result = depth_sort()(tied);
        REQUIRE(result.faces[0].color.r == 10);
        REQUIRE(result.faces[1].color.r == 20);
        REQUIRE(result.faces[2].color.r == 30);
    }
}

TEST_CASE("viewport maps NDC to pixels with the origin at the top left")
{
    const std::array<Vec2, 4> quad{Vec2{-1.0f, -1.0f}, Vec2{1.0f, -1.0f},
                                   Vec2{1.0f, 1.0f}, Vec2{0.0f, 0.0f}};

    ClipScene scene;
    scene.faces.push_back(clip_face(quad, -1.0f, Color{9, 9, 9, 255}));

    const RenderScene mapped = viewport(200, 100)(scene);
    REQUIRE(mapped.faces.size() == 1);

    const auto& points = mapped.faces[0].points;
    // NDC -Y is the bottom of the image, which is the largest pixel row.
    REQUIRE(points[0].x == Approx(0.0f).margin(1e-4));
    REQUIRE(points[0].y == Approx(100.0f).margin(1e-4));
    REQUIRE(points[2].x == Approx(200.0f).margin(1e-4));
    REQUIRE(points[2].y == Approx(0.0f).margin(1e-4));
    REQUIRE(points[3].x == Approx(100.0f).margin(1e-4));
    REQUIRE(points[3].y == Approx(50.0f).margin(1e-4));

    REQUIRE(mapped.faces[0].color == Color{9, 9, 9, 255});
}

TEST_CASE("the cube pipeline yields the three faces of the scene contract")
{
    constexpr std::uint32_t kSize = 512;
    const Camera camera = default_camera(1.0f);
    const rubiks::math::Transform model;

    const RenderScene scene = build_scene()            //
                              | transform(model)       //
                              | view(camera)           //
                              | project(camera)        //
                              | cull()                 //
                              | depth_sort()           //
                              | viewport(kSize, kSize);

    const CubeFaceColors colors = standard_cube_colors();

    SECTION("only the three faces turned toward the camera survive")
    {
        REQUIRE(scene.faces.size() == 3);
        REQUIRE(find_face(scene, colors.right) != nullptr);
        REQUIRE(find_face(scene, colors.up) != nullptr);
        REQUIRE(find_face(scene, colors.front) != nullptr);
        REQUIRE(find_face(scene, colors.left) == nullptr);
        REQUIRE(find_face(scene, colors.down) == nullptr);
        REQUIRE(find_face(scene, colors.back) == nullptr);
    }

    SECTION("the rendered scene contract samples fall inside their faces")
    {
        // Fractions of the drawing buffer, matching the contract in
        // docs/tasks/03-math-and-graphics-foundation.md.
        const auto sample = [](float fx, float fy) {
            return Vec2{fx * static_cast<float>(kSize),
                        fy * static_cast<float>(kSize)};
        };

        const auto* up_face = find_face(scene, colors.up);
        const auto* front_face = find_face(scene, colors.front);
        const auto* right_face = find_face(scene, colors.right);
        REQUIRE(up_face != nullptr);
        REQUIRE(front_face != nullptr);
        REQUIRE(right_face != nullptr);

        REQUIRE(contains(up_face->points, sample(0.50f, 0.29f)));
        REQUIRE(contains(front_face->points, sample(0.31f, 0.61f)));
        REQUIRE(contains(right_face->points, sample(0.69f, 0.61f)));

        // Each sample identifies exactly one face, so a swapped color or
        // winding cannot pass unnoticed.
        REQUIRE(!contains(up_face->points, sample(0.31f, 0.61f)));
        REQUIRE(!contains(front_face->points, sample(0.69f, 0.61f)));
        REQUIRE(!contains(right_face->points, sample(0.50f, 0.29f)));
    }

    SECTION("the cube silhouette stays clear of the image corners")
    {
        for (const auto& face : scene.faces) {
            REQUIRE(!contains(face.points, Vec2{0.0f, 0.0f}));
            REQUIRE(!contains(face.points, Vec2{kSize - 1.0f, 0.0f}));
            REQUIRE(!contains(face.points, Vec2{0.0f, kSize - 1.0f}));
            REQUIRE(!contains(face.points, Vec2{kSize - 1.0f, kSize - 1.0f}));
        }
    }
}

TEST_CASE("the model transform reaches the pipeline output")
{
    const Camera camera = default_camera(1.0f);

    rubiks::math::Transform shrunk;
    shrunk.scale = 0.5f;

    const RenderScene reference = build_scene()
                                  | transform(rubiks::math::Transform{})
                                  | view(camera) | project(camera) | cull()
                                  | depth_sort() | viewport(512, 512);
    const RenderScene scaled = build_scene() | transform(shrunk) | view(camera)
                               | project(camera) | cull() | depth_sort()
                               | viewport(512, 512);

    REQUIRE(reference.faces.size() == scaled.faces.size());

    // A half-size cube covers strictly less of the image, so its top face
    // cannot still contain the contract sample of the full-size cube.
    const CubeFaceColors colors = standard_cube_colors();
    const auto* scaled_up = find_face(scaled, colors.up);
    REQUIRE(scaled_up != nullptr);
    REQUIRE(!contains(scaled_up->points, Vec2{0.50f * 512.0f, 0.29f * 512.0f}));
}
