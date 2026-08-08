#include "cube/CubeMove.hpp"

#include <algorithm>
#include <iterator>

namespace rubiks::cube {

std::vector<CubeMove> inverse(const std::vector<CubeMove>& sequence)
{
    std::vector<CubeMove> reversed;
    reversed.reserve(sequence.size());

    std::transform(sequence.rbegin(), sequence.rend(),
                   std::back_inserter(reversed),
                   [](const CubeMove& move) { return inverse(move); });
    return reversed;
}

namespace moves {

CubeMove R(int size) noexcept
{
    return CubeMove{Axis::X, layer(size - 1), 1};
}

CubeMove L(int) noexcept
{
    return CubeMove{Axis::X, layer(0), -1};
}

CubeMove U(int size) noexcept
{
    return CubeMove{Axis::Y, layer(size - 1), 1};
}

CubeMove D(int) noexcept
{
    return CubeMove{Axis::Y, layer(0), -1};
}

CubeMove F(int size) noexcept
{
    return CubeMove{Axis::Z, layer(size - 1), 1};
}

CubeMove B(int) noexcept
{
    return CubeMove{Axis::Z, layer(0), -1};
}

}  // namespace moves

}  // namespace rubiks::cube
