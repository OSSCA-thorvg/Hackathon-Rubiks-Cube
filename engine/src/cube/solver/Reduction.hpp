#pragma once

#include <vector>

#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"

/**
 * Turning a big cube into a three by three, which is how a big cube is solved.
 *
 * A four by four and up have pieces a three by three does not: a block of
 * centres on each face instead of one fixed piece, and a row of pieces along
 * each edge instead of one. Reduction makes each of those blocks one colour
 * and each of those rows one pair of colours; after that the cube answers to
 * outer face turns exactly as a three by three does, and the stages that solve
 * one solve this.
 */
namespace rubiks::cube::solver {

/**
 * The moves that make every face's centre block one colour.
 *
 * The first half of a reduction, and the half that has no written sequences at
 * all. Two facts about a cube shape it. A face turn cannot move a centre piece
 * off its face -- only a slice can -- so face turns are free and slices are
 * the whole cost. And a commutator of two turns moves centre pieces in one of
 * exactly two sizes: a slice against a face it cuts exchanges a strip between
 * that face and one neighbour, leaving the other four untouched, while two
 * slices about different axes exchange one piece on each of the six faces
 * whatever the size of the cube.
 *
 * The coarse one cannot finish a face -- near the end it gives back more of
 * the colour than it fetches -- so the fine one is needed, and it touches
 * every face. That is why nothing here works face by face: it counts every
 * centre piece that is home, over the whole cube, and takes whichever
 * commutator raises that count the most.
 */
[[nodiscard]] std::vector<CubeMove> solve_centres(const CubeState& state);

}  // namespace rubiks::cube::solver
