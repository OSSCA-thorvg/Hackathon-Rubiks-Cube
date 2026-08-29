#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "cube/CubeMove.hpp"
#include "cube/Cubie.hpp"

namespace rubiks::cube {

class CubeState;

/**
 * The cube a painting of stickers is, when it is one.
 *
 * Declared here because it is the only thing besides turning that may build a
 * cube, and the class below has to say so. What it is and what it refuses is
 * in `Assembly.hpp`.
 */
[[nodiscard]] std::optional<CubeState> assembled(
    int size, const std::vector<FaceColor>& painting);

/**
 * An N x N x N cube as a grid of cubies, each coordinate an index 0 ... N-1.
 *
 * Index coordinates rather than signed {-1, 0, +1} offsets: the signed form is
 * only integral for odd N, while indices keep one formula for every size. The
 * center and interior cubies occupy grid slots they never show; uniform
 * indexing is simpler than a hole, and at N = 3 it wastes one cubie.
 *
 * The size is a runtime value so that consumers such as the scene builders
 * stay ordinary functions instead of templates.
 */
class CubeState {
public:
    /** Builds a solved cube. */
    explicit CubeState(int size = 3);

    [[nodiscard]] int size() const noexcept { return size_; }

    /** The cubie at an index coordinate; each of x, y, z is 0 ... N-1. */
    [[nodiscard]] const Cubie& at(int x, int y, int z) const noexcept;

    /** Returns true when every exposed sticker matches its solved face. */
    [[nodiscard]] bool is_solved() const noexcept;

    void apply(const CubeMove& move) noexcept;
    void apply(const std::vector<CubeMove>& sequence) noexcept;

private:
    /**
     * A cube from cubies somebody else has already vouched for.
     *
     * Private, and with exactly one friend, because what keeps unreachable
     * cubes out of this domain is not that turning is the only way in -- it is
     * that every cube here is one that turning could have reached. Turning was
     * simply the only proof of that available. `assembled()` is a second
     * proof: it hands back nothing at all unless the stickers it was given are
     * a cube, so a `CubeState` still cannot be built that a solver could not
     * solve.
     */
    CubeState(int size, std::vector<Cubie> cubies) noexcept;

    [[nodiscard]] std::size_t index(int x, int y, int z) const noexcept;
    void rotate_quarter(Axis axis, LayerMask layers) noexcept;

    int size_;
    std::vector<Cubie> cubies_;

    friend bool operator==(const CubeState& a, const CubeState& b) noexcept;
    friend std::optional<CubeState> assembled(
        int size, const std::vector<FaceColor>& painting);
};

[[nodiscard]] bool operator==(const CubeState& a, const CubeState& b) noexcept;
[[nodiscard]] bool operator!=(const CubeState& a, const CubeState& b) noexcept;

}  // namespace rubiks::cube
