#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <vector>

#include "cube/CubeMove.hpp"
#include "cube/Face.hpp"

namespace rubiks::cube {

/**
 * The cube's surface as named stickers, and what a quarter turn does to them.
 *
 * `Cube` answers "what color is at this position", which is the question
 * rendering a still frame asks. Drawing a turn in progress asks a different
 * one: where does *this* sticker go. Color cannot answer it — six colors over
 * fifty-four stickers, so an error that permutes stickers within one face
 * reads as correct. So a sticker gets an identity here, and the permutation
 * that carries it is the same one `Cube` already turns cubies with.
 *
 * Screen-free on purpose: this is a property of the cube, not of any view of
 * it, and both the unfolded net and the ring diagram lay coordinates over the
 * same cycles.
 */

/** A cubie slot as index coordinates; each of x, y, z is 0 ... N-1. */
struct CubiePosition {
    int x;
    int y;
    int z;
};

/** One sticker of the surface: which cubie, and which of its faces. */
struct SurfaceSticker {
    int x;
    int y;
    int z;
    Face face;

    /** Returns the piece coordinate shared by this sticker's other exposed faces. */
    [[nodiscard]] constexpr CubiePosition position() const noexcept { return CubiePosition{x, y, z}; }
};

/** One native face coordinate, counted down rows and right across columns. */
struct FaceCell {
    Face face;
    int row;
    int col;
};

/**
 * Where a native cell sits in storage: six row-major faces in `Face` order.
 *
 * The one statement of the layout. Storage, its views and the sticker lookup
 * below all index through it, and the legacy painting order reuses it with
 * its own reading of row and column.
 */
[[nodiscard]] constexpr std::size_t native_index(Face face, int row, int col,
                                                 int size) noexcept
{
    return face_index(face) * static_cast<std::size_t>(size * size) +
           static_cast<std::size_t>(row * size + col);
}

/** Maps a native face cell to its three-dimensional sticker identity. */
[[nodiscard]] SurfaceSticker face_sticker(Face face, int row, int col, int size) noexcept;

/** Maps an exposed sticker to a native face cell; requires a valid surface sticker. */
[[nodiscard]] FaceCell face_cell(const SurfaceSticker& sticker, int size) noexcept;

/** Whether a named sticker is exposed and inside the given cube. */
[[nodiscard]] bool is_surface_sticker(const SurfaceSticker& sticker, int size) noexcept;

/** The index of a sticker in six outward-facing row-major native faces. */
[[nodiscard]] std::size_t surface_index(const SurfaceSticker& sticker, int size) noexcept;

/**
 * The index in the legacy painting and share order.
 *
 * Face by face like native storage, but each face is read across the first of
 * the other two axes and down the second, in the order `other_axes()` gives
 * them, rather than in the net's rows and columns. `surface_stickers()` lists
 * the stickers in this order.
 */
[[nodiscard]] std::size_t painting_index(const SurfaceSticker& sticker, int size) noexcept;

/** An allocation-free traversal of the native surface, face by face. */
class SurfaceCells {
public:
    /** A forward iterator over sticker identities rather than stored colours. */
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = SurfaceSticker;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = SurfaceSticker;
        /** Constructs an empty default iterator. */
        Iterator() noexcept : Iterator(0, 0) {}
        /** Starts at a native index for this size. */
        Iterator(int size, std::size_t index) noexcept : size_(size), index_(index) {}
        /** Returns the current exposed sticker. */
        [[nodiscard]] SurfaceSticker operator*() const noexcept;
        /** Advances to the next native cell. */
        Iterator& operator++() noexcept { ++index_; return *this; }
        /** Advances while returning the preceding iterator. */
        Iterator operator++(int) noexcept { const Iterator previous = *this; ++*this; return previous; }
        /** Whether two iterators denote the same native cell. */
        [[nodiscard]] bool operator==(const Iterator& other) const noexcept
        { return size_ == other.size_ && index_ == other.index_; }
        /** Whether two iterators denote different native cells. */
        [[nodiscard]] bool operator!=(const Iterator& other) const noexcept { return !(*this == other); }
    private:
        int size_;
        std::size_t index_;
    };
    /** Traverses a size's surface, or an empty surface for nonpositive sizes. */
    explicit SurfaceCells(int size) noexcept : size_(size) {}
    /** Returns the first exposed sticker. */
    [[nodiscard]] Iterator begin() const noexcept { return Iterator{size_, 0}; }
    /** Returns the end sentinel for this surface. */
    [[nodiscard]] Iterator end() const noexcept { return Iterator{size_, size()}; }
    /** Returns the number of exposed stickers. */
    [[nodiscard]] std::size_t size() const noexcept
    { return size_ > 0 ? kFaceCount * static_cast<std::size_t>(size_) * static_cast<std::size_t>(size_) : 0; }
private:
    int size_;
};

/** Returns the native surface traversal without constructing a coordinate array. */
[[nodiscard]] inline SurfaceCells surface_cells(int size) noexcept { return SurfaceCells{size}; }

/** An allocation-free traversal of just the surface touched by selected layers. */
class LayerCells {
public:
    /** Walks selected face strips and any selected outer face caps. */
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = SurfaceSticker;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = SurfaceSticker;
        /** Constructs an empty default iterator. */
        Iterator() noexcept : Iterator(Axis::X, 0, 0, true) {}
        /** Starts a selected-layer traversal, or constructs its end sentinel. */
        Iterator(Axis axis, LayerMask layers, int size, bool end) noexcept;
        /** Returns the current selected surface sticker. */
        [[nodiscard]] SurfaceSticker operator*() const noexcept;
        /** Advances to the next selected surface sticker. */
        Iterator& operator++() noexcept;
        /** Advances while returning the preceding iterator. */
        Iterator operator++(int) noexcept { const Iterator previous = *this; ++*this; return previous; }
        /** Whether two selected-layer iterators denote the same cell. */
        [[nodiscard]] bool operator==(const Iterator& other) const noexcept;
        /** Whether two selected-layer iterators denote different cells. */
        [[nodiscard]] bool operator!=(const Iterator& other) const noexcept { return !(*this == other); }
    private:
        /** Skips unselected strips without walking their individual cells. */
        void seek() noexcept;
        /** Whether the row coordinate on the current face follows this axis. */
        [[nodiscard]] bool row_along_axis() const noexcept;
        Axis axis_;
        LayerMask layers_;
        int size_;
        std::size_t face_;
        int line_ = 0;
        int offset_ = 0;
    };
    /** Selects layers of a cube; out-of-range mask bits do not select a surface. */
    LayerCells(Axis axis, LayerMask layers, int size) noexcept : axis_(axis), layers_(layers), size_(size) {}
    /** Returns the first selected sticker. */
    [[nodiscard]] Iterator begin() const noexcept { return Iterator{axis_, layers_, size_, false}; }
    /** Returns the end sentinel. */
    [[nodiscard]] Iterator end() const noexcept { return Iterator{axis_, layers_, size_, true}; }
private:
    Axis axis_;
    LayerMask layers_;
    int size_;
};

/** Returns a lazy traversal of selected surface layers using native face coordinates. */
[[nodiscard]] inline LayerCells layer_cells(Axis axis, LayerMask layers, int size) noexcept
{ return LayerCells{axis, layers, size}; }

/** A quarter turn's source and destination in the native colour buffer. */
struct SurfaceTurn {
    std::size_t from;
    std::size_t to;
};

/** Derives and caches a layer permutation once per thread and cube size. */
[[nodiscard]] const std::vector<SurfaceTurn>& surface_turns(Axis axis, int layer, int size);

/**
 * Carries a native surface's colours through a move, in place.
 *
 * `colors` holds 6N^2 colours in native order, and `scratch` is a buffer of
 * the same length that each quarter turn reads them back from, so no colour
 * is overwritten before it has moved. The cube's storage and the solver's
 * search scratch both turn through this, each with a buffer of its own kind.
 */
void turn_colors(FaceColor* colors, FaceColor* scratch, const CubeMove& move,
                 int size) noexcept;

[[nodiscard]] constexpr bool operator==(const CubiePosition& a,
                                        const CubiePosition& b) noexcept
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

[[nodiscard]] constexpr bool operator==(const SurfaceSticker& a,
                                        const SurfaceSticker& b) noexcept
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.face == b.face;
}

/** The axis a face points along. */
[[nodiscard]] Axis axis_of(Face face) noexcept;

/** The layer along a face's own axis where that face is the outer surface. */
[[nodiscard]] int outer_layer(Face face, int size) noexcept;

/** Whether a face is the one at the positive end of its axis. */
[[nodiscard]] bool is_positive_face(Face face) noexcept;

/** The face at one end of an axis: the positive end, or the one opposite. */
[[nodiscard]] Face end_face(Axis axis, bool positive) noexcept;

/**
 * The layers a run of depths measured from `face` selects.
 *
 * Depth 1 is the face itself and depths grow inwards, which is how every
 * notation of a slice or a wide move reads: `R` is depth 1, `Rw` is 1 through
 * 2, `2R` is 2 through 2. Written here rather than at either caller because
 * the two callers are inverses of each other -- a command turns a depth range
 * into layers and a move log turns layers back into one -- and a convention
 * with two implementations is a convention with two chances to differ.
 *
 * Which end of the axis a face sits at is the whole of the difference between
 * the two directions, and `is_positive_face()` below already answers it.
 *
 * @return zero for a run that is empty, reversed, or reaches past the far
 *         side, which is the mask no move can have.
 */
[[nodiscard]] LayerMask depth_layers(Face face, int first_depth, int last_depth,
                                     int size) noexcept;

/**
 * The move a face makes, taken as deep as a run of depths says.
 *
 * `turns` counts quarter turns clockwise as seen from outside `face`, which
 * is how a written move reads: `(Right, 1, 1, 1)` is R and `(Right, 1, 2, 1)`
 * is Rw. Clockwise from outside is a positive turn about the axis only for
 * the face at its far end; the face opposite reads the axis backwards, and
 * this is where a move is given that sign.
 *
 * A run `depth_layers()` turns down comes back as a move with no layers.
 * Whether the whole cube turned at once counts as a move is the caller's
 * rule, and is not decided here.
 */
[[nodiscard]] CubeMove face_move(Face face, int first_depth, int last_depth,
                                 int turns, int size) noexcept;

[[nodiscard]] int coordinate_on(Axis axis, const CubiePosition& p) noexcept;
[[nodiscard]] int coordinate_on(Axis axis, const SurfaceSticker& s) noexcept;

/** Writes the one coordinate of a position that lies along `axis`. */
void set_coordinate(CubiePosition& position, Axis axis, int value) noexcept;

/** Where a cubie lands after one positive quarter turn about `axis`. */
[[nodiscard]] CubiePosition turned_position(Axis axis, const CubiePosition& p,
                                            int size) noexcept;

/**
 * The face a sticker shows from after one positive quarter turn about `axis`.
 *
 * The two faces perpendicular to the axis keep their sticker, so they map to
 * themselves.
 */
[[nodiscard]] Face turned_face(Axis axis, Face face) noexcept;

/** The same, after any number of positive quarter turns, taken modulo four. */
[[nodiscard]] Face turned_face(Axis axis, Face face, int quarter_turns) noexcept;

/** Where a sticker is after one positive quarter turn about `axis`. */
[[nodiscard]] SurfaceSticker turned_sticker(const SurfaceSticker& sticker,
                                            Axis axis, int size) noexcept;

/**
 * Where a sticker is after a move: carried round as many quarter turns as the
 * move makes when its layer is one the move selects, and left where it is
 * otherwise.
 */
[[nodiscard]] SurfaceSticker moved_sticker(const SurfaceSticker& sticker,
                                           const CubeMove& move,
                                           int size) noexcept;

/**
 * One of the twenty-four ways of holding the whole cube.
 *
 * `destinations[face]` is the face that `face` ends up as, and `turns` is a
 * run of positive whole-cube quarter turns, about the axes listed in order,
 * that reaches this way of holding it from the home one.
 */
struct CubeOrientation {
    std::array<Face, kFaceCount> destinations;
    std::vector<Axis> turns;
};

/**
 * The twenty-four ways of holding the cube, home first.
 *
 * Grown breadth first from the home orientation by quarter turns about X, Y
 * and Z in that order, so the list and its numbering are fixed. A painting is
 * read in each of them in this order, and a solver finds an odd cube's frame
 * among them.
 */
[[nodiscard]] const std::vector<CubeOrientation>& cube_orientations();

/**
 * Every sticker of the cube's surface, once each, face by face.
 *
 * Screen-free like the rest of this header, which is why it lives here rather
 * than beside one of the two views that draw them: both need to walk all 6N^2
 * of them, and a drawing has no say in which ones exist.
 *
 * Empty for a size below 1.
 */
[[nodiscard]] std::vector<SurfaceSticker> surface_stickers(int size);

/**
 * The 4N stickers one layer cycles, in the order the positive turn moves them.
 *
 * Consecutive slots are neighbours on the surface, so the sequence is a closed
 * ring that a drawing can lay a curve along. A quarter turn advances every
 * sticker by exactly N slots, which is what makes one turn a quarter of the
 * ring whatever the axis.
 *
 * Only this one canonical direction is returned; a negative turn is the same
 * ring walked backwards, which is the caller's index arithmetic rather than
 * a second cycle to keep in step.
 *
 * Empty for a size below 2 or a layer outside the cube.
 */
[[nodiscard]] std::vector<SurfaceSticker> ring_slots(Axis axis, int layer,
                                                     int size);

namespace detail {

/** Native face directions, shared by forward and inverse coordinate conversion. */
struct FaceBasis {
    Axis normal;
    bool positive;
    Axis row;
    bool row_reversed;
    Axis col;
    bool col_reversed;
};

/** One orientation table: normal, downward row direction and rightward column direction. */
inline constexpr std::array<FaceBasis, kFaceCount> kFaceBases{{
    {Axis::X, true,  Axis::Y, true,  Axis::Z, true},
    {Axis::X, false, Axis::Y, true,  Axis::Z, false},
    {Axis::Y, true,  Axis::Z, false, Axis::X, false},
    {Axis::Y, false, Axis::Z, true,  Axis::X, false},
    {Axis::Z, true,  Axis::Y, true,  Axis::X, false},
    {Axis::Z, false, Axis::Y, true,  Axis::X, true},
}};

}  // namespace detail

inline Axis axis_of(Face face) noexcept
{ return detail::kFaceBases[face_index(face)].normal; }

inline bool is_positive_face(Face face) noexcept
{ return detail::kFaceBases[face_index(face)].positive; }

inline int outer_layer(Face face, int size) noexcept
{ return is_positive_face(face) ? size - 1 : 0; }

inline Face end_face(Axis axis, bool positive) noexcept
{
    for (const Face face : faces()) {
        if (axis_of(face) == axis && is_positive_face(face) == positive) return face;
    }
    assert(false && "every axis has a face at each end");
    return Face::Right;
}

inline int coordinate_on(Axis axis, const CubiePosition& position) noexcept
{
    switch (axis) {
        case Axis::X: return position.x;
        case Axis::Y: return position.y;
        case Axis::Z: return position.z;
    }
    return position.z;
}

inline int coordinate_on(Axis axis, const SurfaceSticker& sticker) noexcept
{ return coordinate_on(axis, sticker.position()); }

inline void set_coordinate(CubiePosition& position, Axis axis, int value) noexcept
{
    switch (axis) {
        case Axis::X:
            position.x = value;
            return;
        case Axis::Y:
            position.y = value;
            return;
        case Axis::Z:
            break;
    }
    position.z = value;
}

inline SurfaceSticker face_sticker(Face face, int row, int col, int size) noexcept
{
    const detail::FaceBasis& basis = detail::kFaceBases[face_index(face)];
    const int last = size - 1;
    std::array<int, kAxisCount> position{};
    position[axis_index(basis.normal)] = basis.positive ? last : 0;
    position[axis_index(basis.row)] = basis.row_reversed ? last - row : row;
    position[axis_index(basis.col)] = basis.col_reversed ? last - col : col;
    return SurfaceSticker{position[0], position[1], position[2], face};
}

inline bool is_surface_sticker(const SurfaceSticker& sticker, int size) noexcept
{
    if (size < 1 || !face_from(static_cast<int>(sticker.face))) return false;
    if (sticker.x < 0 || sticker.x >= size || sticker.y < 0 || sticker.y >= size ||
        sticker.z < 0 || sticker.z >= size) return false;
    return coordinate_on(axis_of(sticker.face), sticker) == outer_layer(sticker.face, size);
}

inline FaceCell face_cell(const SurfaceSticker& sticker, int size) noexcept
{
    assert(is_surface_sticker(sticker, size));
    const detail::FaceBasis& basis = detail::kFaceBases[face_index(sticker.face)];
    const int row = coordinate_on(basis.row, sticker);
    const int col = coordinate_on(basis.col, sticker);
    return FaceCell{sticker.face, basis.row_reversed ? size - 1 - row : row,
                                 basis.col_reversed ? size - 1 - col : col};
}

inline std::size_t surface_index(const SurfaceSticker& sticker, int size) noexcept
{
    const FaceCell cell = face_cell(sticker, size);
    return native_index(cell.face, cell.row, cell.col, size);
}

}  // namespace rubiks::cube
