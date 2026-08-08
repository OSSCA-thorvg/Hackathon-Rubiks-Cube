#include <array>
#include <cstddef>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"
#include "graphics/Camera.hpp"
#include "graphics/CubeGeometry.hpp"
#include "graphics/Layout.hpp"
#include "graphics/Palette.hpp"
#include "graphics/Passes.hpp"
#include "graphics/Pipeline.hpp"
#include "math/Transform.hpp"

// Linked against the graphics library alone: if the pipeline ever picks up a
// ThorVG include, this target stops building.

namespace {

using Catch::Approx;
using namespace rubiks::graphics;
using rubiks::cube::CubeState;
using rubiks::cube::Face;
using rubiks::cube::FaceColor;
using rubiks::math::Vec2;
using rubiks::math::Vec3;

constexpr float kPi = 3.14159265358979323846f;
constexpr std::uint32_t kCanvas = 1024;

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

/** The topmost face covering `point`, or null when the point shows through. */
const RenderFace* face_at(const RenderScene& scene, const Vec2& point)
{
    const RenderFace* found = nullptr;
    for (const auto& face : scene.faces) {
        if (contains(face.points, point)) found = &face;
    }
    return found;
}

std::size_t count_color(const RenderScene& scene, const Color& color)
{
    std::size_t total = 0;
    for (const auto& face : scene.faces) {
        if (face.color == color) ++total;
    }
    return total;
}

/** Renders a cube state through the whole pipeline into the cube region. */
RenderScene render_cube(const CubeState& state,
                        const rubiks::math::Transform& model = {})
{
    const auto placement = layout(kCanvas, kCanvas);
    const Camera camera =
        default_camera(placement.cube.width / placement.cube.height);

    return build_cube_scene(state)   //
           | transform(model)        //
           | view(camera)            //
           | project(camera)         //
           | cull()                  //
           | depth_sort()            //
           | viewport(placement.cube);
}

/** A point given as a fraction of the square 3D region. */
Vec2 cube_sample(float fx, float fy)
{
    const auto region = layout(kCanvas, kCanvas).cube;
    return Vec2{region.x + fx * region.width, region.y + fy * region.height};
}

}  // namespace

TEST_CASE("build_cube_scene emits only surface stickers, wound outward")
{
    const WorldScene scene = build_cube_scene(CubeState(3));

    // Six faces of a 3x3 sheet. Interior and side faces are deliberately
    // absent, which is what makes every seam show the background.
    REQUIRE(scene.faces.size() == 54);

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
        // Every normal points away from the cube center; that is what
        // counter-clockwise-from-outside means.
        REQUIRE(rubiks::math::dot(normal, centroid) > 0.0f);
    }
}

TEST_CASE("cubie geometry tiles the cube without overlapping")
{
    constexpr int kSize = 3;

    // The outermost sticker plane sits just inside the nominal cube face,
    // inset by the seam.
    const float outer = cubie_center(kSize - 1, kSize) +
                        sticker_half_extent(kSize);
    REQUIRE(outer == Approx(kCubeHalfExtent * (kSize - 1 + kStickerScale) /
                            static_cast<float>(kSize)));
    REQUIRE(outer < kCubeHalfExtent);

    // Neighbouring cubie centers are one pitch apart and symmetric about 0.
    REQUIRE(cubie_center(0, kSize) == Approx(-cubie_center(kSize - 1, kSize)));
    REQUIRE(cubie_center(1, kSize) - cubie_center(0, kSize) ==
            Approx(2.0f * kCubeHalfExtent / static_cast<float>(kSize)));

    // Adjacent stickers do not touch, so a seam always has positive width.
    REQUIRE(2.0f * sticker_half_extent(kSize) <
            cubie_center(1, kSize) - cubie_center(0, kSize));
}

TEST_CASE("append_sticker places one quad on the requested face")
{
    WorldScene scene;
    append_sticker(scene, Vec3{0.0f, 0.0f, 0.0f}, 0.5f, Face::Right,
                   Color{7, 0, 0, 255});

    REQUIRE(scene.faces.size() == 1);
    REQUIRE(scene.faces[0].color == Color{7, 0, 0, 255});
    for (const auto& point : scene.faces[0].points) {
        REQUIRE(point.x == Approx(0.5f));
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

TEST_CASE("viewport maps NDC into a sub-rectangle of the buffer")
{
    const std::array<Vec2, 4> quad{Vec2{-1.0f, -1.0f}, Vec2{1.0f, -1.0f},
                                   Vec2{1.0f, 1.0f}, Vec2{0.0f, 0.0f}};

    ClipScene scene;
    scene.faces.push_back(clip_face(quad, -1.0f, Color{9, 9, 9, 255}));

    const RenderScene mapped =
        viewport(Rect{40.0f, 10.0f, 200.0f, 100.0f})(scene);
    REQUIRE(mapped.faces.size() == 1);

    const auto& points = mapped.faces[0].points;
    // NDC -Y is the bottom of the region, which is the largest pixel row, and
    // the whole mapping is offset by the rectangle origin.
    REQUIRE(points[0].x == Approx(40.0f).margin(1e-4));
    REQUIRE(points[0].y == Approx(110.0f).margin(1e-4));
    REQUIRE(points[2].x == Approx(240.0f).margin(1e-4));
    REQUIRE(points[2].y == Approx(10.0f).margin(1e-4));
    REQUIRE(points[3].x == Approx(140.0f).margin(1e-4));
    REQUIRE(points[3].y == Approx(60.0f).margin(1e-4));

    REQUIRE(mapped.faces[0].color == Color{9, 9, 9, 255});
}

TEST_CASE("the layout splits the canvas into a square cube region and a net")
{
    SECTION("a square canvas matches the documented fractions")
    {
        const auto placement = layout(1000, 1000);

        REQUIRE(placement.cube.width == Approx(580.0f));
        REQUIRE(placement.cube.height == Approx(580.0f));
        REQUIRE(placement.cube.x == Approx(210.0f));
        REQUIRE(placement.cube.y == Approx(10.0f));

        REQUIRE(placement.net.width == Approx(480.0f));
        REQUIRE(placement.net.height == Approx(360.0f));
        REQUIRE(placement.net.x == Approx(260.0f));
        REQUIRE(placement.net.y == Approx(620.0f));

        // The two regions do not overlap, so appending one after the other
        // cannot hide anything.
        REQUIRE(placement.cube.y + placement.cube.height <= placement.net.y);
    }

    SECTION("the cube region stays square on any aspect ratio")
    {
        for (const auto& size : {std::pair<std::uint32_t, std::uint32_t>{320, 200},
                                 {200, 320},
                                 {1920, 1080}}) {
            const auto placement = layout(size.first, size.second);
            REQUIRE(placement.cube.width == Approx(placement.cube.height));

            // Both regions stay centered horizontally and inside the canvas.
            const auto width = static_cast<float>(size.first);
            REQUIRE(placement.cube.x ==
                    Approx(width - placement.cube.x - placement.cube.width));
            REQUIRE(placement.cube.x >= 0.0f);
            REQUIRE(placement.net.x >= 0.0f);
        }
    }
}

TEST_CASE("a solved cube renders 27 stickers, nine of each visible color")
{
    const RenderScene scene = render_cube(CubeState(3));

    // Three faces of a 3x3 sheet; the other 27 stickers face away.
    REQUIRE(scene.faces.size() == 27);
    REQUIRE(count_color(scene, to_color(FaceColor::White)) == 9);
    REQUIRE(count_color(scene, to_color(FaceColor::Green)) == 9);
    REQUIRE(count_color(scene, to_color(FaceColor::Red)) == 9);
    REQUIRE(count_color(scene, to_color(FaceColor::Yellow)) == 0);
    REQUIRE(count_color(scene, to_color(FaceColor::Orange)) == 0);
    REQUIRE(count_color(scene, to_color(FaceColor::Blue)) == 0);
}

TEST_CASE("the contract samples land on the center sticker of each face")
{
    const RenderScene scene = render_cube(CubeState(3));

    // Fractions of the square cube region, from the contract in
    // docs/tasks/04-rubiks-cube-domain.md.
    const auto* up = face_at(scene, cube_sample(0.50f, 0.29f));
    const auto* front = face_at(scene, cube_sample(0.31f, 0.61f));
    const auto* right = face_at(scene, cube_sample(0.69f, 0.61f));

    REQUIRE(up != nullptr);
    REQUIRE(front != nullptr);
    REQUIRE(right != nullptr);
    REQUIRE(up->color == to_color(FaceColor::White));
    REQUIRE(front->color == to_color(FaceColor::Green));
    REQUIRE(right->color == to_color(FaceColor::Red));
}

TEST_CASE("the seam samples fall between stickers")
{
    const RenderScene scene = render_cube(CubeState(3));

    // Nothing is drawn in a seam, so these points show the background. This
    // only holds because cubies have no body faces.
    REQUIRE(face_at(scene, cube_sample(0.377f, 0.645f)) == nullptr);
    REQUIRE(face_at(scene, cube_sample(0.313f, 0.534f)) == nullptr);
    REQUIRE(face_at(scene, cube_sample(0.564f, 0.321f)) == nullptr);
}

TEST_CASE("the cube silhouette stays inside its region")
{
    const RenderScene scene = render_cube(CubeState(3));
    const auto region = layout(kCanvas, kCanvas).cube;

    for (const auto& face : scene.faces) {
        for (const auto& point : face.points) {
            REQUIRE(point.x >= region.x);
            REQUIRE(point.x <= region.x + region.width);
            REQUIRE(point.y >= region.y);
            REQUIRE(point.y <= region.y + region.height);
        }
    }
}

TEST_CASE("a move changes the colors the pipeline puts on screen")
{
    CubeState state(3);
    state.apply(rubiks::cube::moves::U(3));

    const RenderScene scene = render_cube(state);

    // U cycles the top row Front -> Left -> Back -> Right -> Front. Of the
    // three visible faces the top face keeps its nine whites, the front row
    // takes the right face's red, and the right row takes blue from the back
    // face, which is itself culled along with the orange it received.
    REQUIRE(scene.faces.size() == 27);
    REQUIRE(count_color(scene, to_color(FaceColor::White)) == 9);
    REQUIRE(count_color(scene, to_color(FaceColor::Green)) == 6);
    REQUIRE(count_color(scene, to_color(FaceColor::Red)) == 6 + 3);
    REQUIRE(count_color(scene, to_color(FaceColor::Blue)) == 3);
    REQUIRE(count_color(scene, to_color(FaceColor::Orange)) == 0);

    // Centers sit outside the turning layer, so the contract samples are
    // unchanged; only the rows above them moved.
    REQUIRE(face_at(scene, cube_sample(0.50f, 0.29f))->color ==
            to_color(FaceColor::White));
    REQUIRE(face_at(scene, cube_sample(0.31f, 0.61f))->color ==
            to_color(FaceColor::Green));
    REQUIRE(face_at(scene, cube_sample(0.69f, 0.61f))->color ==
            to_color(FaceColor::Red));
}

TEST_CASE("the model transform reaches the pipeline output")
{
    rubiks::math::Transform shrunk;
    shrunk.scale = 0.5f;

    const RenderScene reference = render_cube(CubeState(3));
    const RenderScene scaled = render_cube(CubeState(3), shrunk);

    REQUIRE(reference.faces.size() == scaled.faces.size());

    // A half-size cube covers strictly less of the region, so the contract
    // sample of the full-size cube now shows background.
    REQUIRE(face_at(reference, cube_sample(0.50f, 0.29f)) != nullptr);
    REQUIRE(face_at(scaled, cube_sample(0.50f, 0.29f)) == nullptr);
}
