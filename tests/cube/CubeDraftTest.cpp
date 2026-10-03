#include <array>
#include <cstddef>
#include <optional>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "cube/Assembly.hpp"
#include "cube/Cube.hpp"
#include "cube/CubeDraft.hpp"
#include "cube/CubeMove.hpp"
#include "cube/Surface.hpp"

namespace {

using rubiks::cube::Axis;
using rubiks::cube::Cube;
using rubiks::cube::CubeDraft;
using rubiks::cube::CubeMove;
using rubiks::cube::Face;
using rubiks::cube::FaceCell;
using rubiks::cube::FaceColor;
using rubiks::cube::PaintFault;
using rubiks::cube::PaintReading;
using rubiks::cube::SurfaceSticker;
using rubiks::cube::face_cell;
using rubiks::cube::layer;
using rubiks::cube::layers_through;
using rubiks::cube::painting_of;

static_assert(!std::is_convertible<const CubeDraft&, const Cube&>::value,
              "an unchecked draft must not cross the solver's Cube boundary");
static_assert(std::is_final<Cube>::value,
              "editable derived types must not weaken Cube's invariant");

}  // namespace

TEST_CASE("a draft owns an independent copy of the cube's outward faces")
{
    Cube cube(4);
    cube.apply(CubeMove{Axis::X, layer(3), 1});
    CubeDraft draft(cube);
    REQUIRE(draft.size() == 4);
    CHECK(painting_of(draft.view()) == painting_of(cube));

    const FaceColor before = cube.face(Face::Front).at(1, 1);
    const FaceColor instead = before == FaceColor::Red ? FaceColor::Blue
                                                      : FaceColor::Red;
    REQUIRE(draft.set_color(Face::Front, 1, 1, instead));
    CHECK(draft.face(Face::Front).at(1, 1) == instead);
    CHECK(cube.face(Face::Front).at(1, 1) == before);

    CubeDraft copied = draft;
    REQUIRE(copied.set_color(Face::Front, 1, 1, before));
    CHECK(draft.face(Face::Front).at(1, 1) == instead);
    CHECK(copied.face(Face::Front).at(1, 1) == before);
}

TEST_CASE("draft colour totals follow changed cells and whole-face fills")
{
    const Cube cube(3);
    CubeDraft draft(cube);
    REQUIRE(draft.set_color(Face::Front, 0, 0, FaceColor::Red));
    CHECK(draft.count(FaceColor::Red) == 10);
    CHECK(draft.count(FaceColor::Green) == 8);
    CHECK_FALSE(draft.set_color(Face::Front, 0, 0, FaceColor::Red));

    const std::array<std::size_t, rubiks::cube::kFaceCount> before = draft.counts();
    CHECK_FALSE(draft.set_color(Face::Front, -1, 0, FaceColor::Red));
    CHECK_FALSE(draft.set_color(Face::Front, 0, 3, FaceColor::Red));
    CHECK_FALSE(draft.set_color(static_cast<Face>(6), 0, 0, FaceColor::Red));
    CHECK_FALSE(draft.set_color(Face::Front, 0, 0, static_cast<FaceColor>(6)));
    CHECK_FALSE(draft.fill_face(static_cast<Face>(6), FaceColor::Red));
    CHECK_FALSE(draft.fill_face(Face::Front, static_cast<FaceColor>(6)));
    CHECK(draft.counts() == before);
    CHECK(draft.count(static_cast<FaceColor>(6)) == 0);

    REQUIRE(draft.fill_face(Face::Front, FaceColor::Green));
    CHECK(draft.count(FaceColor::Red) == 9);
    CHECK(draft.count(FaceColor::Green) == 9);
    REQUIRE(draft.fill_face(Face::Front, FaceColor::Red));
    CHECK(draft.count(FaceColor::Red) == 18);
    CHECK(draft.count(FaceColor::Green) == 0);
    CHECK_FALSE(draft.fill_face(Face::Front, FaceColor::Red));
}

TEST_CASE("draft edits retain diagnostics until a fresh validation is requested")
{
    const Cube cube(3);
    CubeDraft draft(cube);
    REQUIRE(draft.set_color(Face::Front, 0, 0, FaceColor::Red));
    CHECK(draft.reading().fault == PaintFault::None);

    const PaintReading refused = draft.validate();
    REQUIRE(refused.fault == PaintFault::ColourCount);
    REQUIRE_FALSE(refused.blamed.empty());
    CHECK_FALSE(draft.assemble());

    REQUIRE(draft.set_color(Face::Front, 0, 0, FaceColor::Green));
    CHECK(draft.reading().fault == refused.fault);
    CHECK(draft.reading().blamed == refused.blamed);

    const std::optional<Cube> built = draft.assemble();
    REQUIRE(built.has_value());
    CHECK(*built == cube);
    CHECK(draft.reading().fault == PaintFault::None);
    CHECK(draft.reading().blamed.empty());
}

TEST_CASE("balanced draft totals do not admit a mirrored corner")
{
    const Cube cube(3);
    CubeDraft draft(cube);
    const FaceCell right = face_cell(SurfaceSticker{2, 2, 2, Face::Right}, 3);
    const FaceCell up = face_cell(SurfaceSticker{2, 2, 2, Face::Up}, 3);
    const FaceColor right_color = draft.face(right.face).at(right.row, right.col);
    const FaceColor up_color = draft.face(up.face).at(up.row, up.col);
    REQUIRE(draft.set_color(right.face, right.row, right.col, up_color));
    REQUIRE(draft.set_color(up.face, up.row, up.col, right_color));
    for (const Face face : rubiks::cube::faces()) {
        CHECK(draft.count(rubiks::cube::solved_color(face)) == 9);
    }
    CHECK_FALSE(draft.assemble());
    CHECK(draft.reading().fault != PaintFault::None);
    CHECK(cube.is_solved());
}

TEST_CASE("draft assembly normalizes the orientation of an odd cube")
{
    Cube cube(3);
    cube.apply(CubeMove{Axis::X, layer(2), 1});
    cube.apply(CubeMove{Axis::Y, layer(2), -1});
    Cube held = cube;
    held.apply(CubeMove{Axis::Z, layers_through(0, 2), 1});
    CubeDraft draft(held);

    REQUIRE(draft.validate().fault == PaintFault::None);
    const std::optional<Cube> built = draft.assemble();
    REQUIRE(built.has_value());
    CHECK(*built == cube);
    CHECK(painting_of(draft.view()) == painting_of(held));
}
