#include "cube/CubeDraft.hpp"

#include <vector>

namespace rubiks::cube {
namespace {

/** Copies only the source cube's six outward faces. */
[[nodiscard]] std::vector<FaceColor> copied_colors(const Cube& cube)
{
    const ColorView colors = cube.view().flat();
    return std::vector<FaceColor>(colors.begin(), colors.end());
}

/** Whether a draft edit names one real face and one real colour. */
[[nodiscard]] bool valid_edit(Face face, FaceColor color) noexcept
{
    return face_from(static_cast<int>(face)).has_value() &&
           face_color_from(static_cast<int>(color)).has_value();
}

}  // namespace

CubeDraft::CubeDraft(const Cube& cube)
    : storage_(cube.size(), copied_colors(cube))
{
    for (const FaceColor color : storage_.view().flat()) {
        ++counts_[static_cast<std::size_t>(color)];
    }
}

bool CubeDraft::set_color(Face face, int row, int col, FaceColor color) noexcept
{
    if (!valid_edit(face, color)) return false;
    if (row < 0 || row >= size() || col < 0 || col >= size()) return false;

    const FaceColor previous = storage_.view().face(face).at(row, col);
    if (previous == color) return false;

    storage_.set(face, row, col, color);
    --counts_[static_cast<std::size_t>(previous)];
    ++counts_[static_cast<std::size_t>(color)];
    reading_current_ = false;
    return true;
}

bool CubeDraft::fill_face(Face face, FaceColor color) noexcept
{
    if (!valid_edit(face, color)) return false;

    const ColorView previous = storage_.view().face(face).flat();
    bool changed = false;
    for (const FaceColor before : previous) {
        changed = changed || before != color;
    }
    if (!changed) return false;

    for (const FaceColor before : previous) {
        --counts_[static_cast<std::size_t>(before)];
    }
    counts_[static_cast<std::size_t>(color)] += previous.size();
    storage_.fill(face, color);
    reading_current_ = false;
    return true;
}

std::size_t CubeDraft::count(FaceColor color) const noexcept
{
    if (!face_color_from(static_cast<int>(color))) return 0;
    return counts_[static_cast<std::size_t>(color)];
}

const PaintReading& CubeDraft::validate()
{
    if (!reading_current_) {
        reading_ = read_painting(size(), painting_of(view()));
        reading_current_ = true;
    }
    return reading_;
}

}  // namespace rubiks::cube
