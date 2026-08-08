#pragma once

#include <cstddef>
#include <vector>

#include "cube/CubeMove.hpp"
#include "cube/Cubie.hpp"

namespace rubiks::cube {

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

    void apply(const CubeMove& move) noexcept;
    void apply(const std::vector<CubeMove>& sequence) noexcept;

private:
    [[nodiscard]] std::size_t index(int x, int y, int z) const noexcept;
    void rotate_quarter(Axis axis, LayerMask layers) noexcept;

    int size_;
    std::vector<Cubie> cubies_;

    friend bool operator==(const CubeState& a, const CubeState& b) noexcept;
};

[[nodiscard]] bool operator==(const CubeState& a, const CubeState& b) noexcept;
[[nodiscard]] bool operator!=(const CubeState& a, const CubeState& b) noexcept;

}  // namespace rubiks::cube
