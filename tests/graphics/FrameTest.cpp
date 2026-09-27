#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"
#include "cube/Cubie.hpp"
#include "graphics/FrameInputs.hpp"
#include "graphics/Layout.hpp"
#include "graphics/Light.hpp"
#include "graphics/NetGeometry.hpp"
#include "graphics/OrbitCamera.hpp"

// What a frame is drawn from, compared. A canvas keeps its last frame for as
// long as this says nothing changed, so every input a drawing reads has to
// make it say otherwise.

namespace {

using namespace rubiks::graphics;
using rubiks::cube::Axis;
using rubiks::cube::CubeState;
using rubiks::cube::Face;
using rubiks::cube::FaceColor;

/** A canvas showing all four drawings, with something in every input. */
FrameInputs every_drawing(const CubeState& cube)
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
                                     net_cell(Face::Front, 0, 0, 3)}},
                           std::nullopt};
    frame.rings = RingsDrawing{revision, turn, Palette::Classic,
                               {RingsGuide{Axis::Z, 2}}};
    frame.axes = AxesDrawing{home_orbit()};
    return frame;
}

}  // namespace

TEST_CASE("a frame is the same as another only when all it is drawn from is")
{
    const CubeState cube(3);
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
    const CubeState cube(3);
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
    placed.net->painting->cursor = NetPosition{Face::Front, 1, 1};
    REQUIRE_FALSE(placed == before);
    FrameInputs stepped = placed;
    stepped.net->painting->cursor->col = 2;
    REQUIRE_FALSE(stepped == placed);
}

TEST_CASE("a cube is the cube it was for as long as its revision is")
{
    // The cube is the one input held by address, so two frames of a changed
    // cube are told apart by the number beside it, and two cubes by address.
    const CubeState one(3);
    const CubeState other(3);
    REQUIRE(CubeRevision{&one, 4} == CubeRevision{&one, 4});
    REQUIRE_FALSE(CubeRevision{&one, 4} == CubeRevision{&one, 5});
    REQUIRE_FALSE(CubeRevision{&one, 4} == CubeRevision{&other, 4});
}
