#pragma once

#include <cassert>
#include <cstddef>
#include <optional>
#include <vector>

#include "cube/CubeMove.hpp"
#include "cube/Face.hpp"
#include "cube/Surface.hpp"

namespace rubiks::cube {

/**
 * A borrowed, read-only run of colours. Rotation and draft edits preserve it;
 * destruction, moving or assigning its owner requires acquiring a new view.
 */
class ColorView {
public:
    /** Borrows count colours; the owner must outlive the view. */
    constexpr ColorView(const FaceColor* data, std::size_t count) noexcept
        : data_(data), count_(count)
    {
    }
    /** Returns the underlying colour buffer without copying. */
    [[nodiscard]] constexpr const FaceColor* data() const noexcept
    {
        return data_;
    }
    /** Returns the number of colours in the run. */
    [[nodiscard]] constexpr std::size_t size() const noexcept
    {
        return count_;
    }
    /** Whether this run contains no colours. */
    [[nodiscard]] constexpr bool empty() const noexcept
    {
        return count_ == 0;
    }
    /** Starts a flat traversal of the borrowed colours. */
    [[nodiscard]] constexpr const FaceColor* begin() const noexcept
    {
        return data_;
    }
    /** Ends a flat traversal of the borrowed colours. */
    [[nodiscard]] constexpr const FaceColor* end() const noexcept
    {
        return count_ == 0 ? data_ : data_ + count_;
    }
    /** Reads a colour at an index inside this run. */
    [[nodiscard]] FaceColor operator[](std::size_t index) const noexcept
    {
        assert(index < count_);
        return data_[index];
    }

private:
    const FaceColor* data_;
    std::size_t count_;
};

/** One square face, with rows counted down and columns right in the net. */
class FaceView {
public:
    /** Borrows one row-major face from its owner. */
    constexpr FaceView(const FaceColor* data, int size) noexcept
        : data_(data), size_(size)
    {
    }
    /** Returns the side length of this face. */
    [[nodiscard]] constexpr int size() const noexcept
    {
        return size_;
    }
    /** Reads a colour at a row and column inside this face. */
    [[nodiscard]] FaceColor at(int row, int col) const noexcept
    {
        assert(row >= 0 && row < size_ && col >= 0 && col < size_);
        return data_[static_cast<std::size_t>(row * size_ + col)];
    }
    /** Reads the same two-dimensional coordinate as at(). */
    [[nodiscard]] FaceColor operator()(int row, int col) const noexcept
    {
        return at(row, col);
    }
    /** Borrows one row of this face. */
    [[nodiscard]] ColorView row(int row) const noexcept
    {
        assert(row >= 0 && row < size_);
        return ColorView{data_ + row * size_, static_cast<std::size_t>(size_)};
    }
    /** Borrows every colour of this face in row-major order. */
    [[nodiscard]] ColorView flat() const noexcept
    {
        return ColorView{data_, static_cast<std::size_t>(size_ * size_)};
    }
    /** Starts a flat traversal of this face. */
    [[nodiscard]] const FaceColor* begin() const noexcept
    {
        return flat().begin();
    }
    /** Ends a flat traversal of this face. */
    [[nodiscard]] const FaceColor* end() const noexcept
    {
        return flat().end();
    }

private:
    const FaceColor* data_;
    int size_;
};

class PieceView;

/**
 * A borrowed surface with no physical-validity guarantee. Its owner must remain
 * alive and must not be moved or assigned while the view is in use.
 */
class CubeView {
public:
    /** Borrows six native row-major faces; the owner must outlive the view. */
    constexpr CubeView(int size, const FaceColor* data) noexcept
        : size_(size), data_(data)
    {
    }
    /** Returns the side length shared by the six faces. */
    [[nodiscard]] constexpr int size() const noexcept
    {
        return size_;
    }
    /** Borrows one face without reordering or copying colours. */
    [[nodiscard]] FaceView face(Face face) const noexcept
    {
        const std::size_t offset = native_index(face, 0, 0, size_);
        return FaceView{data_ == nullptr ? nullptr : data_ + offset, size_};
    }
    /** Borrows every face in Face enum order, each in row-major order. */
    [[nodiscard]] ColorView flat() const noexcept
    {
        return ColorView{data_,
                         kFaceCount * static_cast<std::size_t>(size_ * size_)};
    }
    /** Reads a named, exposed surface sticker through the common mapping. */
    [[nodiscard]] FaceColor sticker(
        const SurfaceSticker& sticker) const noexcept
    {
        assert(is_surface_sticker(sticker, size_));
        return data_[surface_index(sticker, size_)];
    }
    /** Borrows the exposed stickers at a three-dimensional piece coordinate. */
    [[nodiscard]] PieceView at(int x, int y, int z) const noexcept;

private:
    int size_;
    const FaceColor* data_;
};

/** Reads the exposed faces of a piece without storing or inventing hidden
 * colours. */
class PieceView {
public:
    /** Borrows the surface at an index coordinate. */
    constexpr PieceView(CubeView view, CubiePosition position) noexcept
        : view_(view), position_(position)
    {
    }
    /** Reads an exposed face of this piece. */
    [[nodiscard]] FaceColor sticker(Face face) const noexcept
    {
        return view_.sticker(
            SurfaceSticker{position_.x, position_.y, position_.z, face});
    }

private:
    CubeView view_;
    CubiePosition position_;
};

inline PieceView CubeView::at(int x, int y, int z) const noexcept
{
    return PieceView{*this, CubiePosition{x, y, z}};
}

namespace detail {

/** Storage shared by the two domain types, with no physical-validity policy. */
class FaceStorage {
public:
    /** Allocates six solved faces for a supported size. */
    explicit FaceStorage(int size);
    /** Takes ownership of six native faces whose shape has already been
     * checked. */
    FaceStorage(int size, std::vector<FaceColor> colors) noexcept;
    /** Returns the common side length. */
    [[nodiscard]] int size() const noexcept
    {
        return size_;
    }
    /** Borrows this storage for read-only queries. */
    [[nodiscard]] CubeView view() const noexcept
    {
        return CubeView{size_, colors_.data()};
    }
    /** Returns the owned native colours for internal copying or comparison. */
    [[nodiscard]] const std::vector<FaceColor>& colors() const noexcept
    {
        return colors_;
    }
    /** Changes one colour; callers decide which mutations their type permits.
     */
    void set(Face face, int row, int col, FaceColor color) noexcept;
    /** Fills one native face with a colour. */
    void fill(Face face, FaceColor color) noexcept;
    /** Permutes selected surface stickers using the common coordinate rules. */
    void rotate_quarter(Axis axis, LayerMask layers) noexcept;

private:
    int size_;
    std::vector<FaceColor> colors_;
};

}  // namespace detail

class Cube;
class CubeDraft;

/** Builds a cube only if a legacy-order painting is physically reachable. */
[[nodiscard]] std::optional<Cube> assembled(
    int size, const std::vector<FaceColor>& painting);

/** A physically reachable cube, stored as six outward-facing two-dimensional
 * faces. */
class Cube final {
public:
    /** Builds a solved cube of size 2 ... the layer mask width minus one. */
    explicit Cube(int size) : storage_(size)
    {
    }
    /** Returns the side length. */
    [[nodiscard]] int size() const noexcept
    {
        return storage_.size();
    }
    /** Borrows one native face for two-dimensional or flat traversal. */
    [[nodiscard]] FaceView face(Face face) const noexcept
    {
        return view().face(face);
    }
    /** Borrows all six faces for display without weakening this type's
     * invariant. */
    [[nodiscard]] CubeView view() const noexcept
    {
        return storage_.view();
    }
    /** Allows readers to accept a cube or a draft through the weaker view type.
     */
    [[nodiscard]] operator CubeView() const noexcept
    {
        return view();
    }
    /** Reads an exposed sticker by its three-dimensional identity. */
    [[nodiscard]] FaceColor sticker(
        const SurfaceSticker& sticker) const noexcept
    {
        return view().sticker(sticker);
    }
    /** Borrows the exposed stickers of one piece at an index coordinate. */
    [[nodiscard]] PieceView at(int x, int y, int z) const noexcept
    {
        return view().at(x, y, z);
    }
    /** Whether every face is uniform, independent of whole-cube orientation. */
    [[nodiscard]] bool is_solved() const noexcept;
    /** Applies an axis/layer rotation, preserving physical reachability. */
    void apply(const CubeMove& move) noexcept;

private:
    /** Accepts owned storage only after the assembly boundary has validated it.
     */
    explicit Cube(detail::FaceStorage storage) noexcept;
    detail::FaceStorage storage_;
    friend class CubeDraft;
    friend std::optional<Cube> assembled(
        int size, const std::vector<FaceColor>& painting);
    friend bool operator==(const Cube& a, const Cube& b) noexcept;
};

/** Compares visible colours, independent of construction history. */
[[nodiscard]] bool operator==(const Cube& a, const Cube& b) noexcept;

}  // namespace rubiks::cube
