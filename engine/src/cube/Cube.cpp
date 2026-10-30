#include "cube/Cube.hpp"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace rubiks::cube {
namespace detail {

FaceStorage::FaceStorage(int size) : size_(size)
{
    if (size < kMinCubeSize || size >= kLayerMaskBits) {
#if defined(__cpp_exceptions)
        throw std::invalid_argument("cube size must fit the layer mask");
#else
        std::abort();
#endif
    }
    colors_.resize(kFaceCount * static_cast<std::size_t>(size * size));
    for (const Face face : faces()) fill(face, solved_color(face));
}

FaceStorage::FaceStorage(int size, std::vector<FaceColor> colors) noexcept
    : size_(size), colors_(std::move(colors))
{
    assert(size >= kMinCubeSize && size < kLayerMaskBits);
    assert(colors_.size() ==
           kFaceCount * static_cast<std::size_t>(size * size));
}

void FaceStorage::set(Face face, int row, int col, FaceColor color) noexcept
{
    assert(row >= 0 && row < size_ && col >= 0 && col < size_);
    colors_[native_index(face, row, col, size_)] = color;
}

void FaceStorage::fill(Face face, FaceColor color) noexcept
{
    const std::size_t area = static_cast<std::size_t>(size_ * size_);
    const std::vector<FaceColor>::iterator first =
        colors_.begin() + static_cast<std::ptrdiff_t>(native_index(face, 0, 0, size_));
    std::fill(first, first + area, color);
}

void FaceStorage::apply(const CubeMove& move) noexcept
{
    // Reuse the workspace across search nodes; the state owns only its colours.
    static thread_local std::vector<FaceColor> scratch;
    scratch.resize(colors_.size());
    turn_colors(colors_.data(), scratch.data(), move, size_);
}

}  // namespace detail

Cube::Cube(detail::FaceStorage storage) noexcept : storage_(std::move(storage))
{
}

bool Cube::is_solved() const noexcept
{
    const std::array<Face, kFaceCount> all = faces();
    return std::all_of(all.begin(), all.end(), [this](Face face) -> bool {
        const FaceView colors = this->face(face);
        const FaceColor first = *colors.begin();
        return std::all_of(colors.begin(), colors.end(),
                           [first](FaceColor color) -> bool {
                               return color == first;
                           });
    });
}

void Cube::apply(const CubeMove& move) noexcept
{
    storage_.apply(move);
}

bool operator==(const Cube& a, const Cube& b) noexcept
{
    return a.size() == b.size() && a.storage_.colors() == b.storage_.colors();
}

}  // namespace rubiks::cube
