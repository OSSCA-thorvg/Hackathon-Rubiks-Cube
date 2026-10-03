#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include "cube/Assembly.hpp"
#include "cube/Cube.hpp"

namespace rubiks::cube {

/** Editable face colours, kept separate from a reachable Cube. */
class CubeDraft final {
public:
    /** Copies the cube's colours; later edits never change the source cube. */
    explicit CubeDraft(const Cube& cube);

    /** The side length of each face. */
    [[nodiscard]] int size() const noexcept
    {
        return storage_.size();
    }

    /** Reads a face in the same outward orientation used by the net. */
    [[nodiscard]] FaceView face(Face id) const noexcept
    {
        return storage_.view().face(id);
    }

    /** Borrows the draft's colours for rendering without validating them. */
    [[nodiscard]] CubeView view() const noexcept
    {
        return storage_.view();
    }

    /** Changes one valid cell; returns false for invalid input or no change. */
    [[nodiscard]] bool set_color(Face face, int row, int col,
                                 FaceColor color) noexcept;

    /** Changes a whole face; returns false for invalid input or no change. */
    [[nodiscard]] bool fill_face(Face face, FaceColor color) noexcept;

    /** The maintained number of stickers carrying this colour. */
    [[nodiscard]] std::size_t count(FaceColor color) const noexcept;

    /** The maintained totals, indexed by FaceColor. */
    [[nodiscard]] const std::array<std::size_t, kFaceCount>& counts()
        const noexcept
    {
        return counts_;
    }

    /** Validates the current colours once, reusing the result until an edit. */
    [[nodiscard]] const PaintReading& validate();

    /** The last requested reading; edits retain its diagnostic highlights. */
    [[nodiscard]] const PaintReading& reading() const noexcept
    {
        return reading_;
    }

    /** Builds a normalized Cube after validation, leaving the draft intact. */
    [[nodiscard]] std::optional<Cube> assemble();

private:
    detail::FaceStorage storage_;
    std::array<std::size_t, kFaceCount> counts_{};
    PaintReading reading_;
    bool reading_current_ = false;
};

}  // namespace rubiks::cube
