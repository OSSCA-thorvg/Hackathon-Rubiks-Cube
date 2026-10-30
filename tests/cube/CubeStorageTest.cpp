#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ReferenceCube.hpp"
#include "cube/Assembly.hpp"
#include "cube/Cube.hpp"
#include "cube/CubeMove.hpp"
#include "cube/Surface.hpp"

namespace {

using rubiks::cube::Axis;
using rubiks::cube::Cube;
using rubiks::cube::CubeMove;
using rubiks::cube::Face;
using rubiks::cube::FaceColor;
using rubiks::cube::LayerMask;
using rubiks::cube::SurfaceSticker;
using rubiks::test::ReferenceCube;

/** Small, even, odd, packed-maximum and mask-maximum domain sizes. */
constexpr std::array<int, 7> kSizes{2, 3, 4, 5, 7, 28, 31};

/** The face order as originally declared, independent of iterator helpers. */
constexpr std::array<Face, 6> kFaces{
    Face::Right, Face::Left, Face::Up, Face::Down, Face::Front, Face::Back};

/** The original painting coordinate order, independent of surface helpers. */
[[nodiscard]] SurfaceSticker legacy_sticker(Face face, int across, int down,
                                           int size) noexcept
{
    const int last = size - 1;
    switch (face) {
        case Face::Right:
            return SurfaceSticker{last, across, down, face};
        case Face::Left:
            return SurfaceSticker{0, across, down, face};
        case Face::Up:
            return SurfaceSticker{across, last, down, face};
        case Face::Down:
            return SurfaceSticker{across, 0, down, face};
        case Face::Front:
            return SurfaceSticker{across, down, last, face};
        case Face::Back:
            return SurfaceSticker{across, down, 0, face};
    }
    return SurfaceSticker{across, down, 0, face};
}

/** Checks every exposed colour against the frozen grid after a move. */
void require_same_surface(const Cube& cube, const ReferenceCube& reference)
{
    REQUIRE(cube.size() == reference.size());
    for (const Face face : kFaces) {
        for (int down = 0; down < cube.size(); ++down) {
            for (int across = 0; across < cube.size(); ++across) {
                const SurfaceSticker sticker =
                    legacy_sticker(face, across, down, cube.size());
                INFO("face " << static_cast<int>(face) << " across " << across
                             << " down " << down);
                REQUIRE(cube.at(sticker.x, sticker.y, sticker.z)
                            .sticker(sticker.face) == reference.sticker(sticker));
            }
        }
    }
}

/** Masks exercising empty, edge, inner, wide, whole and disconnected slices. */
[[nodiscard]] std::array<LayerMask, 7> masks_for(int size) noexcept
{
    LayerMask whole = 0;
    for (int index = 0; index < size; ++index) {
        whole |= LayerMask{1} << index;
    }
    const LayerMask low = LayerMask{1};
    const LayerMask high = LayerMask{1} << (size - 1);
    const LayerMask middle = LayerMask{1} << (size / 2);
    return {LayerMask{0}, low, high, middle, low | LayerMask{2}, whole,
            low | high};
}

/** Draws varied slice indices and widths, including arbitrary disconnected sets. */
[[nodiscard]] LayerMask random_mask(int size, std::mt19937& random) noexcept
{
    const std::array<LayerMask, 7> masks = masks_for(size);
    const int first = static_cast<int>(random() % static_cast<unsigned>(size));
    const int width = 1 + static_cast<int>(
        random() % static_cast<unsigned>(size - first));
    switch (random() % 7U) {
        case 0:
            return LayerMask{0};
        case 1:
            return LayerMask{1} << first;
        case 2:
            return random() % 2U == 0U ? masks[1] : masks[2];
        case 3:
            return LayerMask{1} << (size > 2
                ? 1 + static_cast<int>(random() %
                      static_cast<unsigned>(size - 2))
                : first);
        case 4: {
            LayerMask run = 0;
            for (int index = first; index < first + width; ++index) {
                run |= LayerMask{1} << index;
            }
            return run;
        }
        case 5:
            return masks[5];
        case 6:
            return static_cast<LayerMask>(random()) & masks[5];
    }
    return LayerMask{0};
}

}  // namespace

TEST_CASE("surface moves match the frozen grid for every move shape",
          "[cube-storage]")
{
    constexpr std::array<Axis, 3> axes{Axis::X, Axis::Y, Axis::Z};
    for (const int size : kSizes) {
        Cube cube(size);
        ReferenceCube reference(size);
        require_same_surface(cube, reference);
        for (const Axis axis : axes) {
            for (const LayerMask mask : masks_for(size)) {
                for (int turns = -5; turns <= 5; ++turns) {
                    const CubeMove move{axis, mask, turns};
                    INFO("size " << size << " axis " << static_cast<int>(axis)
                                 << " mask " << mask << " turns " << turns);
                    cube.apply(move);
                    reference.apply(move);
                    require_same_surface(cube, reference);
                }
            }
        }
    }
}

TEST_CASE("mixed random moves match the frozen grid after every commit",
          "[cube-storage]")
{
    std::mt19937 random(0x267de6cU);
    for (const int size : kSizes) {
        Cube cube(size);
        ReferenceCube reference(size);
        for (int step = 0; step < 120; ++step) {
            const Axis axis = static_cast<Axis>(random() % 3U);
            const LayerMask mask = random_mask(size, random);
            const int turns = static_cast<int>(random() % 11U) - 5;
            const CubeMove move{axis, mask, turns};
            INFO("size " << size << " step " << step << " axis "
                         << static_cast<int>(axis) << " mask " << mask
                         << " turns " << turns);
            cube.apply(move);
            reference.apply(move);
            require_same_surface(cube, reference);
        }
    }
}

TEST_CASE("every move shape has an inverse and quarter turns have order four",
          "[cube-storage]")
{
    constexpr std::array<Axis, 3> axes{Axis::X, Axis::Y, Axis::Z};
    for (const int size : kSizes) {
        const Cube solved(size);
        for (const Axis axis : axes) {
            for (const LayerMask mask : masks_for(size)) {
                INFO("size " << size << " axis " << static_cast<int>(axis)
                             << " mask " << mask);
                Cube repeated = solved;
                for (int count = 0; count < 4; ++count) {
                    repeated.apply(CubeMove{axis, mask, 1});
                }
                REQUIRE(repeated == solved);
                for (int turns = -5; turns <= 5; ++turns) {
                    Cube restored = solved;
                    restored.apply(CubeMove{axis, mask, turns});
                    restored.apply(CubeMove{axis, mask, -turns});
                    REQUIRE(restored == solved);
                }
            }
        }
    }
}

namespace {

using rubiks::cube::ColorView;
using rubiks::cube::CubeView;
using rubiks::cube::FaceCell;
using rubiks::cube::FaceView;

/** The outward net orientation written independently for all six faces. */
[[nodiscard]] SurfaceSticker outward_sticker(Face face, int row, int col,
                                             int size) noexcept
{
    const int last = size - 1;
    switch (face) {
        case Face::Right:
            return SurfaceSticker{last, last - row, last - col, face};
        case Face::Left:
            return SurfaceSticker{0, last - row, col, face};
        case Face::Up:
            return SurfaceSticker{col, last, row, face};
        case Face::Down:
            return SurfaceSticker{col, 0, last - row, face};
        case Face::Front:
            return SurfaceSticker{col, last - row, last, face};
        case Face::Back:
            return SurfaceSticker{last - col, last - row, 0, face};
    }
    return SurfaceSticker{last - col, last - row, 0, face};
}

/** Mixes visible colours before testing a view, so mirroring cannot pass. */
void mix(Cube& cube, ReferenceCube& reference)
{
    const int last = cube.size() - 1;
    const std::array<CubeMove, 5> moves{
        CubeMove{Axis::X, LayerMask{1} << last, 1},
        CubeMove{Axis::Y, LayerMask{1}, -1},
        CubeMove{Axis::Z, LayerMask{1} << last, 2},
        CubeMove{Axis::X, LayerMask{1}, -1},
        CubeMove{Axis::Y, LayerMask{1} << last, 1}};
    for (const CubeMove& move : moves) {
        cube.apply(move);
        reference.apply(move);
    }
}

/** Copies a borrowed colour range into an independently owned expectation. */
[[nodiscard]] std::vector<FaceColor> snapshot(ColorView colours)
{
    return std::vector<FaceColor>(colours.begin(), colours.end());
}

}  // namespace

TEST_CASE("face rows and flat views read the outward surface without copies",
          "[cube-storage][cube-view]")
{
    static_assert(std::is_same_v<decltype(std::declval<ColorView>().data()),
                                 const FaceColor*>);
    static_assert(std::is_same_v<decltype(std::declval<ColorView>().begin()),
                                 const FaceColor*>);
    static_assert(std::is_same_v<decltype(std::declval<ColorView>().end()),
                                 const FaceColor*>);
    static_assert(std::is_same_v<decltype(std::declval<FaceView>().row(0)),
                                 ColorView>);
    static_assert(std::is_same_v<decltype(std::declval<FaceView>().begin()),
                                 const FaceColor*>);
    static_assert(std::is_same_v<decltype(std::declval<FaceView>().end()),
                                 const FaceColor*>);

    for (const int size : kSizes) {
        Cube cube(size);
        ReferenceCube reference(size);
        mix(cube, reference);
        const CubeView view = cube.view();
        const ColorView flat = view.flat();
        REQUIRE(view.size() == size);
        REQUIRE(flat.size() == static_cast<std::size_t>(6 * size * size));
        REQUIRE_FALSE(flat.empty());
        for (std::size_t face_index = 0; face_index < kFaces.size(); ++face_index) {
            const Face face = kFaces[face_index];
            const FaceView face_view = cube.face(face);
            REQUIRE(face_view.size() == size);
            REQUIRE(face_view.flat().size() ==
                    static_cast<std::size_t>(size * size));
            REQUIRE(face_view.flat().data() ==
                    flat.data() + face_index * static_cast<std::size_t>(size * size));
            REQUIRE(view.face(face).flat().data() == face_view.flat().data());
            REQUIRE(face_view.begin() == face_view.flat().begin());
            REQUIRE(face_view.end() == face_view.flat().end());
            for (int row = 0; row < size; ++row) {
                const ColorView row_view = face_view.row(row);
                REQUIRE(row_view.size() == static_cast<std::size_t>(size));
                REQUIRE(row_view.data() == face_view.flat().data() + row * size);
                for (int col = 0; col < size; ++col) {
                    const SurfaceSticker sticker =
                        outward_sticker(face, row, col, size);
                    const std::size_t index = face_index *
                        static_cast<std::size_t>(size * size) +
                        static_cast<std::size_t>(row * size + col);
                    const FaceColor expected = reference.sticker(sticker);
                    INFO("size " << size << " face " << static_cast<int>(face)
                                 << " row " << row << " col " << col);
                    REQUIRE(face_view.at(row, col) == expected);
                    REQUIRE(row_view[static_cast<std::size_t>(col)] == expected);
                    REQUIRE(face_view.flat()[static_cast<std::size_t>(row * size + col)]
                            == expected);
                    REQUIRE(flat[index] == expected);
                    REQUIRE(view.sticker(sticker) == expected);
                    REQUIRE(cube.sticker(sticker) == expected);
                    REQUIRE(cube.at(sticker.x, sticker.y, sticker.z)
                                .sticker(face) == expected);
                }
            }
        }
    }
}

TEST_CASE("surface indices preserve outward layout and the legacy painting order",
          "[cube-storage][cube-view]")
{
    for (const int size : kSizes) {
        std::size_t native_index = 0;
        for (const SurfaceSticker sticker : rubiks::cube::surface_cells(size)) {
            const std::size_t face_area = static_cast<std::size_t>(size * size);
            const Face face = kFaces[native_index / face_area];
            const int row = static_cast<int>((native_index % face_area) /
                                             static_cast<std::size_t>(size));
            const int col = static_cast<int>(native_index %
                                             static_cast<std::size_t>(size));
            const SurfaceSticker expected = outward_sticker(face, row, col, size);
            REQUIRE(sticker == expected);
            REQUIRE(rubiks::cube::face_sticker(face, row, col, size) == expected);
            const FaceCell cell = rubiks::cube::face_cell(sticker, size);
            REQUIRE(cell.face == face);
            REQUIRE(cell.row == row);
            REQUIRE(cell.col == col);
            REQUIRE(rubiks::cube::surface_index(sticker, size) == native_index);
            ++native_index;
        }
        REQUIRE(native_index == static_cast<std::size_t>(6 * size * size));

        const std::vector<SurfaceSticker> legacy =
            rubiks::cube::surface_stickers(size);
        std::size_t legacy_index = 0;
        for (const Face face : kFaces) {
            for (int down = 0; down < size; ++down) {
                for (int across = 0; across < size; ++across) {
                    const SurfaceSticker expected =
                        legacy_sticker(face, across, down, size);
                    REQUIRE(legacy[legacy_index] == expected);
                    REQUIRE(rubiks::cube::painting_index(expected, size) ==
                            legacy_index);
                    ++legacy_index;
                }
            }
        }
        REQUIRE(legacy_index == legacy.size());
    }
}

TEST_CASE("a layer range walks exactly the selected surface stickers once",
          "[cube-storage][cube-view]")
{
    using StickerKey = std::tuple<int, int, int, int>;
    for (const int size : kSizes) {
        for (const Axis axis : {Axis::X, Axis::Y, Axis::Z}) {
            for (const LayerMask mask : masks_for(size)) {
                std::set<StickerKey> expected;
                for (const Face face : kFaces) {
                    for (int down = 0; down < size; ++down) {
                        for (int across = 0; across < size; ++across) {
                            const SurfaceSticker sticker =
                                legacy_sticker(face, across, down, size);
                            const int coordinate = axis == Axis::X ? sticker.x
                                : axis == Axis::Y ? sticker.y : sticker.z;
                            if ((mask & (LayerMask{1} << coordinate)) == 0) continue;
                            expected.emplace(static_cast<int>(face), sticker.x,
                                             sticker.y, sticker.z);
                        }
                    }
                }
                std::set<StickerKey> visited;
                std::size_t count = 0;
                const rubiks::cube::LayerCells cells =
                    rubiks::cube::layer_cells(axis, mask, size);
                for (const SurfaceSticker sticker :
                     cells) {
                    const StickerKey key{static_cast<int>(sticker.face),
                                         sticker.x, sticker.y, sticker.z};
                    REQUIRE(expected.count(key) == 1);
                    REQUIRE(visited.insert(key).second);
                    ++count;
                }
                REQUIRE(visited == expected);
                REQUIRE(count == expected.size());
                std::size_t selected = 0;
                for (int index = 0; index < size; ++index) {
                    if ((mask & (LayerMask{1} << index)) != 0) ++selected;
                }
                const std::size_t caps =
                    ((mask & LayerMask{1}) != 0 ? std::size_t{1} : std::size_t{0}) +
                    ((mask & (LayerMask{1} << (size - 1))) != 0
                         ? std::size_t{1} : std::size_t{0});
                const std::size_t expected_count =
                    4 * static_cast<std::size_t>(size) * selected +
                    static_cast<std::size_t>(size * size) * caps;
                REQUIRE(count == expected_count);
                const std::ptrdiff_t distance =
                    std::distance(cells.begin(), cells.end());
                REQUIRE(distance == static_cast<std::ptrdiff_t>(expected_count));
            }
        }
        const rubiks::cube::LayerCells outside =
            rubiks::cube::layer_cells(Axis::X, LayerMask{1} << size, size);
        REQUIRE(outside.begin() == outside.end());
    }
}

TEST_CASE("copying a cube owns its colours and borrowed views follow their owner",
          "[cube-storage][cube-view]")
{
    Cube original(7);
    ReferenceCube reference(7);
    mix(original, reference);
    Cube copied = original;
    const CubeView original_view = original.view();
    const CubeView copied_view = copied.view();
    const std::vector<FaceColor> before = snapshot(original_view.flat());
    REQUIRE(copied == original);
    REQUIRE(copied_view.flat().data() != original_view.flat().data());

    original.apply(CubeMove{Axis::X, LayerMask{1} << 6, 1});
    REQUIRE(snapshot(copied_view.flat()) == before);
    REQUIRE(snapshot(original_view.flat()) == snapshot(original.view().flat()));
    REQUIRE_FALSE(snapshot(original_view.flat()) == before);
    REQUIRE_FALSE(copied == original);

    Cube assigned(4);
    assigned = original;
    REQUIRE(assigned.size() == 7);
    REQUIRE(assigned == original);
    REQUIRE(assigned.view().flat().data() != original.view().flat().data());
    assigned.apply(CubeMove{Axis::Y, LayerMask{1}, -1});
    REQUIRE_FALSE(assigned == original);
    REQUIRE(snapshot(copied_view.flat()) == before);
}

TEST_CASE("a cube moved from is still the cube it was", "[cube-storage]")
{
    // A move is a copy, so nothing can hold a Cube with a size and no
    // colours. The source is read on purpose after it has been moved from.
    Cube original(5);
    ReferenceCube reference(5);
    mix(original, reference);
    const Cube kept = original;

    const Cube constructed = std::move(original);
    REQUIRE(constructed == kept);
    REQUIRE(original == kept);
    REQUIRE(original.view().flat().size() == 6U * 5U * 5U);

    Cube assigned(3);
    assigned = std::move(original);
    REQUIRE(assigned == kept);
    REQUIRE(original == kept);
    REQUIRE_FALSE(original.is_solved());
}

TEST_CASE("equal visible paintings remain equal through the same later moves",
          "[cube-storage]")
{
    for (const int size : {2, 3, 4, 5, 7}) {
        Cube original(size);
        ReferenceCube reference(size);
        mix(original, reference);
        const std::vector<FaceColor> painting = rubiks::cube::painting_of(original);
        const std::optional<Cube> rebuilt = rubiks::cube::assembled(size, painting);
        REQUIRE(rebuilt.has_value());
        Cube restored = *rebuilt;
        REQUIRE(restored == original);
        require_same_surface(restored, reference);
        for (const LayerMask mask : masks_for(size)) {
            for (const Axis axis : {Axis::X, Axis::Y, Axis::Z}) {
                const CubeMove move{axis, mask, -1};
                original.apply(move);
                restored.apply(move);
                reference.apply(move);
                REQUIRE(restored == original);
                require_same_surface(restored, reference);
            }
        }
    }
}

TEST_CASE("uniform faces stay solved when the whole cube is held differently",
          "[cube-storage]")
{
    for (const int size : kSizes) {
        const LayerMask whole = masks_for(size)[5];
        for (const Axis axis : {Axis::X, Axis::Y, Axis::Z}) {
            for (int turns = 1; turns < 4; ++turns) {
                Cube held(size);
                held.apply(CubeMove{axis, whole, turns});
                REQUIRE(held.is_solved());
                REQUIRE_FALSE(held == Cube(size));
                held.apply(CubeMove{axis, LayerMask{1}, 1});
                REQUIRE_FALSE(held.is_solved());
            }
        }
    }
}

TEST_CASE("storage bounds are enforced before allocating and empty views stay empty",
          "[cube-storage][cube-view]")
{
    for (const int size : {-1, 0, 1, 32, 1000000}) {
        REQUIRE_THROWS_AS(Cube(size), std::invalid_argument);
    }
    const ColorView empty(nullptr, 0);
    REQUIRE(empty.empty());
    REQUIRE(empty.size() == 0);
    REQUIRE(empty.begin() == empty.end());
    for (const int size : {-1, 0}) {
        const rubiks::cube::SurfaceCells surface = rubiks::cube::surface_cells(size);
        REQUIRE(surface.size() == 0);
        REQUIRE(surface.begin() == surface.end());
        const rubiks::cube::LayerCells layers =
            rubiks::cube::layer_cells(Axis::X, LayerMask{1}, size);
        REQUIRE(layers.begin() == layers.end());
    }
    REQUIRE(rubiks::cube::is_surface_sticker(SurfaceSticker{2, 1, 1, Face::Right}, 3));
    for (const SurfaceSticker invalid : {
             SurfaceSticker{1, 1, 1, Face::Right},
             SurfaceSticker{-1, 1, 1, Face::Left},
             SurfaceSticker{3, 1, 1, Face::Right},
             SurfaceSticker{2, -1, 1, Face::Right},
             SurfaceSticker{2, 1, 3, Face::Right},
             SurfaceSticker{2, 1, 1, static_cast<Face>(6)}}) {
        REQUIRE_FALSE(rubiks::cube::is_surface_sticker(invalid, 3));
    }
}
