#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "cube/CubeMove.hpp"
#include "cube/Cube.hpp"
#include "cube/Face.hpp"
#include "graphics/frame/FrameInputs.hpp"
#include "graphics/Layout.hpp"
#include "graphics/3d/Light.hpp"
#include "graphics/2d/net/NetGeometry.hpp"
#include "graphics/3d/OrbitCamera.hpp"

// What a frame is drawn from, compared. A canvas keeps its last frame for as
// long as this says nothing changed, so every input a drawing reads has to
// make it say otherwise.

namespace {

using namespace rubiks::graphics;
using rubiks::cube::Axis;
using rubiks::cube::Cube;
using rubiks::cube::Face;
using rubiks::cube::FaceCell;
using rubiks::cube::FaceColor;
using rubiks::cube::face_sticker;

/** A canvas showing all four drawings, with something in every input. */
FrameInputs every_drawing(const Cube& cube)
{
    const CubeRevision revision{&cube, 7};
    const ActiveRotation turn{Axis::X, 1u, 30.0f, 1.0f};

    FrameInputs frame;
    frame.width = 640;
    frame.height = 480;
    frame.placement = layout(640, 480, ViewMode::Both, FlatStyle::Both);
    frame.cube = CubeDrawing{revision, turn, Palette::Classic,
                             Lighting::standard(), home_orbit()};
    frame.net = NetDrawing{revision, turn, Palette::Classic,
                           {NetGuide{Axis::Y, 0,
                                     face_sticker(Face::Front, 0, 0, 3)}},
                           std::nullopt};
    frame.rings = RingsDrawing{revision, turn, Palette::Classic,
                               {RingsGuide{Axis::Z, 2}}};
    frame.axes = AxesDrawing{home_orbit()};
    return frame;
}

}  // namespace

TEST_CASE("a frame is the same as another only when all it is drawn from is")
{
    const Cube cube(3);
    const FrameInputs drawn = every_drawing(cube);
    REQUIRE(every_drawing(cube) == drawn);

    const std::vector<std::function<void(FrameInputs&)>> changes{
        [](FrameInputs& f) { f.width += 1; },
        [](FrameInputs& f) { f.height += 1; },
        [](FrameInputs& f) { f.placement.rings.y += 1.0f; },

        [](FrameInputs& f) { f.cube.reset(); },
        [](FrameInputs& f) { f.cube->cube.revision += 1; },
        [](FrameInputs& f) { f.cube->rotation.reset(); },
        [](FrameInputs& f) { f.cube->rotation->layers = 2u; },
        [](FrameInputs& f) { f.cube->rotation->angle_degrees += 1.0f; },
        [](FrameInputs& f) { f.cube->rotation->opening = 0.5f; },
        [](FrameInputs& f) { f.cube->palette = Palette::HighContrast; },
        [](FrameInputs& f) { f.cube->lighting.ambient += 0.1f; },
        [](FrameInputs& f) { f.cube->lighting.lamps[0].position.x += 1.0f; },
        [](FrameInputs& f) { f.cube->lighting.lamps[0].shininess += 1.0f; },
        [](FrameInputs& f) { f.cube->orbit.yaw_degrees += 1.0f; },
        [](FrameInputs& f) { f.cube->orbit.pitch_degrees += 1.0f; },

        [](FrameInputs& f) { f.net->cube.revision += 1; },
        [](FrameInputs& f) { f.net->rotation->axis = Axis::Y; },
        [](FrameInputs& f) { f.net->palette = Palette::HighContrast; },
        [](FrameInputs& f) { f.net->guides.clear(); },
        [](FrameInputs& f) { f.net->guides[0].cell.x += 1; },
        [](FrameInputs& f) {
            f.net->painting = NetPainting{{FaceColor::Red}, {}, std::nullopt};
        },

        [](FrameInputs& f) { f.rings.reset(); },
        [](FrameInputs& f) { f.rings->cube.revision += 1; },
        [](FrameInputs& f) { f.rings->palette = Palette::HighContrast; },
        [](FrameInputs& f) { f.rings->guides[0].layer = 1; },

        [](FrameInputs& f) { f.axes.reset(); },
        [](FrameInputs& f) { f.axes->orbit.pitch_degrees -= 1.0f; },
    };

    for (std::size_t i = 0; i < changes.size(); ++i) {
        CAPTURE(i);
        FrameInputs changed = drawn;
        changes[i](changed);
        REQUIRE_FALSE(changed == drawn);
    }
}

TEST_CASE("a painting is compared by what is painted on it")
{
    const Cube cube(3);
    FrameInputs before = every_drawing(cube);
    before.net->painting =
        NetPainting{{FaceColor::White, FaceColor::Red}, {}, std::nullopt};

    // The same squares, arrived at again, are the frame already drawn.
    FrameInputs again = before;
    again.net->painting =
        NetPainting{{FaceColor::White, FaceColor::Red}, {}, std::nullopt};
    REQUIRE(again == before);

    FrameInputs stroked = before;
    stroked.net->painting->stickers[1] = FaceColor::Blue;
    REQUIRE_FALSE(stroked == before);

    FrameInputs blamed = before;
    blamed.net->painting->blamed = {1};
    REQUIRE_FALSE(blamed == before);

    // The keyboard's place is drawn on it too, so a step is a new frame.
    FrameInputs placed = before;
    placed.net->painting->cursor = FaceCell{Face::Front, 1, 1};
    REQUIRE_FALSE(placed == before);
    FrameInputs stepped = placed;
    stepped.net->painting->cursor->col = 2;
    REQUIRE_FALSE(stepped == placed);
}

TEST_CASE("a cube is the cube it was for as long as its revision is")
{
    // The cube is the one input held by address, so two frames of a changed
    // cube are told apart by the number beside it, and two cubes by address.
    const Cube one(3);
    const Cube other(3);
    REQUIRE(CubeRevision{&one, 4} == CubeRevision{&one, 4});
    REQUIRE_FALSE(CubeRevision{&one, 4} == CubeRevision{&one, 5});
    REQUIRE_FALSE(CubeRevision{&one, 4} == CubeRevision{&other, 4});
}

/** Frame snapshots retain native face orientation and external blame identity. */
TEST_CASE("a painting frame draws its native snapshot directly")
{
    for (const int size : {2, 4, 7}) {
        const Cube cube(size);
        const std::size_t area = static_cast<std::size_t>(size * size);
        std::vector<FaceColor> snapshot(rubiks::cube::kFaceCount * area);
        for (const Face face : rubiks::cube::faces()) {
            for (int row = 0; row < size; ++row) {
                for (int col = 0; col < size; ++col) {
                    snapshot[rubiks::cube::face_index(face) * area +
                             static_cast<std::size_t>(row * size + col)] =
                        static_cast<FaceColor>(
                            (static_cast<int>(face) + 2 * row + 3 * col) % 6);
                }
            }
        }

        const FaceCell marked{Face::Right, 0, size - 1};
        const rubiks::cube::SurfaceSticker sticker =
            rubiks::cube::face_sticker(marked.face, marked.row, marked.col, size);
        const int blamed = static_cast<int>(
            rubiks::cube::painting_index(sticker, size));
        FrameInputs frame;
        frame.width = 600;
        frame.height = 600;
        frame.placement = layout(600, 600, ViewMode::Flat);
        frame.net = NetDrawing{
            CubeRevision{&cube, 0}, std::nullopt, Palette::Classic, {},
            NetPainting{snapshot, {blamed}, std::nullopt}};

        const RenderScene scene = compose(frame);
        REQUIRE(scene.faces.size() == snapshot.size());
        std::size_t emitted = 0;
        for (const Face face : net_faces()) {
            for (int row = 0; row < size; ++row) {
                for (int col = 0; col < size; ++col) {
                    const std::size_t index =
                        rubiks::cube::face_index(face) * area +
                        static_cast<std::size_t>(row * size + col);
                    REQUIRE(scene.faces[emitted++].color ==
                            to_color(snapshot[index], Palette::Classic));
                }
            }
        }

        const rubiks::math::Vec2 center =
            net_cell_center(marked, frame.placement.net, size);
        const float half =
            0.5f * kNetStickerScale * net_cell_side(frame.placement.net, size);
        REQUIRE(scene.strokes.size() == 1);
        REQUIRE(scene.strokes.front().start.x == Catch::Approx(center.x - half));
        REQUIRE(scene.strokes.front().start.y == Catch::Approx(center.y - half));
    }
}

TEST_CASE("a painting snapshot of another size draws no net")
{
    // Read as a cube of the drawing's size, a snapshot of any other size
    // would be read past its end; it is drawn as nothing instead.
    const Cube cube(4);
    FrameInputs frame;
    frame.width = 600;
    frame.height = 600;
    frame.placement = layout(600, 600, ViewMode::Flat);
    frame.net = NetDrawing{
        CubeRevision{&cube, 0}, std::nullopt, Palette::Classic, {},
        NetPainting{std::vector<FaceColor>(6 * 3 * 3, FaceColor::Red), {0},
                    FaceCell{Face::Front, 1, 1}}};

    const RenderScene scene = compose(frame);
    REQUIRE(scene.faces.empty());
    REQUIRE(scene.strokes.empty());
}
