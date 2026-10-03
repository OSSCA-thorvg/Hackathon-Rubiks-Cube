#include <array>
#include <cstddef>
#include <set>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "CubeSupport.hpp"
#include "cube/Cube.hpp"
#include "cube/CubeMove.hpp"
#include "cube/PackedMove.hpp"
#include "cube/solver/LayerByLayer.hpp"
#include "cube/solver/Reduction.hpp"
#include "cube/solver/ReductionStages.hpp"

namespace {

using rubiks::cube::Axis;
using rubiks::cube::Cube;
using rubiks::cube::CubeMove;
using rubiks::cube::Face;
using rubiks::cube::FaceColor;
using rubiks::cube::LayerMask;
using rubiks::cube::solver::LayerByLayer;
using rubiks::cube::solver::Reduction;
using rubiks::cube::solver::Solver;

/** Every rigid holding frame, deduplicated by the six colours of a solved cube. */
[[nodiscard]] std::vector<std::vector<CubeMove>> holding_frames(int size)
{
    constexpr std::array<Face, 6> faces{
        Face::Right, Face::Left, Face::Up, Face::Down, Face::Front, Face::Back};
    const LayerMask whole = rubiks::cube::layers_through(0, size - 1);
    std::set<std::array<FaceColor, 6>> seen;
    std::vector<std::vector<CubeMove>> frames;
    for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
            for (int z = 0; z < 4; ++z) {
                const std::vector<CubeMove> turns{
                    CubeMove{Axis::X, whole, x}, CubeMove{Axis::Y, whole, y},
                    CubeMove{Axis::Z, whole, z}};
                Cube held(size);
                rubiks::test::apply_all(held, turns);
                std::array<FaceColor, 6> signature{};
                for (std::size_t index = 0; index < faces.size(); ++index) {
                    signature[index] = held.face(faces[index]).at(0, 0);
                }
                if (seen.insert(signature).second) frames.push_back(turns);
            }
        }
    }
    return frames;
}

/** Chooses the same small/large solver boundary as the application. */
[[nodiscard]] const Solver& solver_for(int size, const LayerByLayer& small,
                                       const Reduction& big) noexcept
{
    return size <= 3 ? static_cast<const Solver&>(small)
                     : static_cast<const Solver&>(big);
}

/** Applies returned moves to the original frame and checks the recorded-move contract. */
void require_solution(Cube original, const Solver& solver)
{
    REQUIRE(solver.supports(original.size()));
    REQUIRE_FALSE(original.is_solved());
    const std::vector<CubeMove> moves = solver.solve(original);
    REQUIRE_FALSE(moves.empty());
    for (const CubeMove& move : moves) {
        REQUIRE(rubiks::cube::is_layer_run(move.layers, original.size()));
        REQUIRE(move.layers != rubiks::cube::layers_through(0, original.size() - 1));
        REQUIRE(rubiks::cube::pack(move) != 0);
        original.apply(move);
    }
    REQUIRE(original.is_solved());
}

/** A legal odd cube whose centre frame is changed by thin and wide middle-layer moves. */
[[nodiscard]] Cube with_middle_moves(int size, Axis axis, int turns)
{
    Cube original = rubiks::test::scrambled(0xabc123U, size, 10);
    const Axis next = axis == Axis::X ? Axis::Y
                      : axis == Axis::Y ? Axis::Z : Axis::X;
    const Axis last = axis == Axis::X ? Axis::Z
                      : axis == Axis::Y ? Axis::X : Axis::Y;
    const int middle = size / 2;
    const std::array<CubeMove, 4> moves{
        CubeMove{axis, LayerMask{1} << middle, turns},
        CubeMove{next, LayerMask{1} << (size - 1), -1},
        CubeMove{last, rubiks::cube::layers_through(0, middle), 1},
        CubeMove{next, LayerMask{1}, 2}};
    for (const CubeMove& move : moves) original.apply(move);
    return original;
}

}  // namespace

TEST_CASE("every holding frame of a uniform cube needs no solver moves",
          "[solver-frame]")
{
    const LayerByLayer small;
    const Reduction big;
    for (const int size : {2, 3, 4, 5}) {
        const std::vector<std::vector<CubeMove>> frames = holding_frames(size);
        REQUIRE(frames.size() == 24);
        for (std::size_t index = 0; index < frames.size(); ++index) {
            INFO("size " << size << " holding frame " << index);
            Cube held(size);
            rubiks::test::apply_all(held, frames[index]);
            REQUIRE(held.is_solved());
            REQUIRE(solver_for(size, small, big).solve(held).empty());
            REQUIRE(rubiks::cube::solver::solve_as_three_layers(held).empty());
        }
    }
}

TEST_CASE("a scrambled three by three solves in all twenty-four holding frames",
          "[solver-frame]")
{
    const LayerByLayer solver;
    const Cube scrambled = rubiks::test::scrambled(0x123456U, 3, 20);
    const std::vector<std::vector<CubeMove>> frames = holding_frames(3);
    REQUIRE(frames.size() == 24);
    for (std::size_t index = 0; index < frames.size(); ++index) {
        INFO("holding frame " << index);
        Cube held = scrambled;
        rubiks::test::apply_all(held, frames[index]);
        require_solution(held, solver);
    }
}

TEST_CASE("odd cubes solve after thin and wide moves carry their fixed centres",
          "[solver-frame]")
{
    const LayerByLayer small;
    const Reduction big;
    for (const int size : {3, 5}) {
        for (const Axis axis : {Axis::X, Axis::Y, Axis::Z}) {
            for (const int turns : {-1, 1, 2}) {
                INFO("size " << size << " central axis " << static_cast<int>(axis)
                             << " turns " << turns);
                const Cube original = with_middle_moves(size, axis, turns);
                require_solution(original, solver_for(size, small, big));
            }
        }
    }
}
