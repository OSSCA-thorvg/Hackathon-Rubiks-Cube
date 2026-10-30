#include "cube/solver/Reduction.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <map>
#include <cstddef>
#include <cstdint>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "cube/Face.hpp"
#include "cube/Permutation.hpp"
#include "cube/Surface.hpp"
#include "cube/solver/LayerByLayer.hpp"
#include "cube/solver/Projection.hpp"
#include "cube/solver/ReductionStages.hpp"
#include "cube/solver/Turning.hpp"

namespace rubiks::cube::solver {
namespace {

/** Keeps the solver's piece order stable while traversing native face storage. */
void order_by_piece(std::vector<SurfaceSticker>& cells)
{
    std::sort(cells.begin(), cells.end(), [](const SurfaceSticker& a, const SurfaceSticker& b) -> bool {
        return std::make_tuple(a.x, a.y, a.z, face_index(a.face)) <
               std::make_tuple(b.x, b.y, b.z, face_index(b.face));
    });
}

/** Enumerates face interiors directly, without visiting the cube's volume. */
[[nodiscard]] std::vector<SurfaceSticker> centre_cells(int size)
{
    std::vector<SurfaceSticker> cells;
    for (const Face face : faces()) {
        for (int row = 1; row < size - 1; ++row) {
            for (int col = 1; col < size - 1; ++col) {
                cells.push_back(face_sticker(face, row, col, size));
            }
        }
    }
    order_by_piece(cells);
    return cells;
}

/** Enumerates edge stickers from face borders, excluding their corner cells. */
[[nodiscard]] std::vector<SurfaceSticker> edge_cells(int size)
{
    std::vector<SurfaceSticker> cells;
    const int last = size - 1;
    for (const Face face : faces()) {
        for (int offset = 1; offset < last; ++offset) {
            cells.push_back(face_sticker(face, 0, offset, size));
            cells.push_back(face_sticker(face, last, offset, size));
            cells.push_back(face_sticker(face, offset, 0, size));
            cells.push_back(face_sticker(face, offset, last, size));
        }
    }
    order_by_piece(cells);
    return cells;
}

/** The colour each of these cells is being asked for. */
[[nodiscard]] std::vector<FaceColor> home_colours(
    const std::vector<SurfaceSticker>& cells)
{
    std::vector<FaceColor> wants;
    wants.reserve(cells.size());
    for (const auto& cell : cells) wants.push_back(home_colour(cell.face));
    return wants;
}

/** A run of turns, taken back: reversed, and each one undone. */
[[nodiscard]] std::vector<Turn> taken_back(const std::vector<Turn>& steps)
{
    std::vector<Turn> back;
    back.reserve(steps.size());
    for (auto step = steps.rbegin(); step != steps.rend(); ++step) {
        back.push_back(undone(*step));
    }
    return back;
}

/**
 * Two runs of turns, used one against the other.
 *
 * `A B A' B'`, which is the only shape any of this needs. Whatever A does to
 * the cube, doing it and taking it back leaves only what B changed in between
 * -- so the pieces that end up moved are the few both of them reach, and every
 * other piece on the cube is where it was. That is why nothing in this file
 * keeps track of what it has already finished: a commutator of turns that miss
 * a piece cannot touch it.
 */
struct Commutator {
    std::vector<Turn> first;
    std::vector<Turn> second;
};

[[nodiscard]] std::vector<Turn> written_out(const Commutator& tool)
{
    std::vector<Turn> steps = tool.first;
    for (const auto& step : tool.second) steps.push_back(step);
    for (const auto& step : taken_back(tool.first)) steps.push_back(step);
    for (const auto& step : taken_back(tool.second)) steps.push_back(step);
    return steps;
}

/**
 * Every commutator worth trying on the centres.
 *
 * **Coarse** is a slice against a face it cuts. It exchanges a strip between
 * that face and one neighbour and leaves the other four alone -- a lot of
 * ground for four moves, and how most of the work gets done.
 *
 * **Fine** is two slices about different axes. It moves exactly one piece on
 * each of the six faces, whatever the size of the cube, which is what the
 * coarse one cannot do: near the end of a face the coarse one gives back more
 * of a colour than it fetches, because every strip through a nearly finished
 * face is nearly all right already.
 *
 * **Narrow** is the same idea with the first half conjugated -- a face turn, a
 * slice, and the face turn taken back. Six pieces becomes two or three, on two
 * or three faces, which is the only thing that finishes the last cells: once
 * the rest of the cube is right, a six-cycle takes four pieces off faces that
 * were finished and can only make matters worse.
 *
 * Half turns are in the list for a reason worth writing down: a slice turned
 * once carries a piece to the next face round, and only a slice turned twice
 * carries one to the face opposite. Without them, two pieces that belong on
 * opposite sides of the cube and are sitting on each other's faces cannot be
 * put right at all -- which is exactly how this stage failed before they were
 * there.
 */
[[nodiscard]] std::vector<Commutator> centre_shapes(int size)
{
    std::vector<Commutator> tools;

    for (const Face face : faces()) {
        for (const int face_turns : {1, -1, 2}) {
            for (const Axis axis : other_axes(axis_of(face))) {
                for (int layer = 1; layer <= size - 2; ++layer) {
                    for (const int slice_turns : {1, -1, 2}) {
                        tools.push_back(Commutator{
                            {layer_turn(axis, layer, slice_turns, size)},
                            {face_turn(face, face_turns)}});
                    }
                }
            }
        }
    }

    for (const Axis first : axes()) {
        for (const Axis second : axes()) {
            if (first == second) continue;
            for (int a = 1; a <= size - 2; ++a) {
                for (int b = 1; b <= size - 2; ++b) {
                    for (const int first_turns : {1, -1, 2}) {
                        for (const int second_turns : {1, -1, 2}) {
                            tools.push_back(Commutator{
                                {layer_turn(first, a, first_turns, size)},
                                {layer_turn(second, b, second_turns, size)}});
                        }
                    }
                }
            }
        }
    }

    for (const Axis axis : axes()) {
        for (const Face face : faces()) {
            if (axis_of(face) == axis) continue;
            for (const int hold : {1, -1, 2}) {
                for (int a = 1; a <= size - 2; ++a) {
                    for (int b = 1; b <= size - 2; ++b) {
                        for (const int first_turns : {1, -1, 2}) {
                            for (const int second_turns : {1, -1, 2}) {
                                tools.push_back(Commutator{
                                    {face_turn(face, hold),
                                     layer_turn(axis, a, first_turns, size),
                                     face_turn(face, -hold)},
                                    {layer_turn(axis, b, second_turns, size)}});
                            }
                        }
                    }
                }
            }
        }
    }
    return tools;
}

/**
 * The colour asked of each edge sticker, and the one place it is not its own.
 *
 * Every edge piece home is what this stage aims at, and a cube that cannot get
 * there is not a cube with something wrong: it is a cube whose corners sit in
 * an odd order, which happens when pieces a three by three cannot tell apart
 * -- the centres of one face -- have quietly taken an odd permutation of their
 * own. The stages that follow cannot mend that, and neither can anything here,
 * because every tool in the list is an even permutation of the pieces.
 *
 * So the target moves instead of the cube. Two rows are told to want each
 * other's colours, which is one swap of the three by three's edges and lands
 * the same oddness on the other side of the ledger -- and getting there from
 * "everything home" is an even permutation, which the tools do have. The two
 * rows chosen share the up face, so only the stickers on their other face
 * change what they are asking for.
 */
[[nodiscard]] std::vector<FaceColor> edge_wants(
    const std::vector<SurfaceSticker>& cells, int size, bool swap_two_pairs)
{
    auto wants = home_colours(cells);
    if (!swap_two_pairs) return wants;

    // Exactly two rows, and no more: two swaps would be even again and leave
    // the ledger where it was. They share the up face, so it is only the
    // stickers on their other face that change what they are asking for.
    const auto one = edge_row(Face::Up, Face::Front, size);
    const auto other = edge_row(Face::Up, Face::Back, size);

    for (std::size_t i = 0; i < cells.size(); ++i) {
        const auto& cell = cells[i];
        if (cell.face == Face::Front) {
            for (const auto& at : one) {
                if (at == cell.position()) wants[i] = home_colour(Face::Back);
            }
        }
        if (cell.face == Face::Back) {
            for (const auto& at : other) {
                if (at == cell.position()) wants[i] = home_colour(Face::Front);
            }
        }
    }
    return wants;
}

/**
 * Every run of turns worth trying on the edges.
 *
 * Two shapes, and the difference between them is how much they disturb.
 *
 * **Coarse** is one or two face turns. A face turn carries whole rows about
 * and is how a piece crosses the cube, but it can never take one piece out of
 * a row, so nothing is ever paired by these alone.
 *
 * **Fine** is a commutator whose first half is a face turn and a slice
 * together, against a face turn. A slice on its own cuts across the rows,
 * which is the only way a piece leaves one -- but a slice also carries a strip
 * of centre pieces onto the next face round, and putting it back is what the
 * face turn in front of it is for. Nothing shorter works: a slice against a
 * face turn, which is what this file uses on the centres, always leaves the
 * centres somewhere else, and the search below throws every one of them away.
 *
 * Which of these keep the centres is not argued here. It is asked of each one,
 * by looking at where it actually sends them.
 */
[[nodiscard]] std::vector<std::vector<Turn>> edge_shapes(int size)
{
    std::vector<std::vector<Turn>> tools;

    for (const Face face : faces()) {
        for (const int turns : {1, -1, 2}) {
            tools.push_back({face_turn(face, turns)});
        }
    }

    for (const Face first : faces()) {
        for (const int first_turns : {1, -1, 2}) {
            for (const Face second : faces()) {
                if (second == first) continue;
                for (const int second_turns : {1, -1, 2}) {
                    tools.push_back({face_turn(first, first_turns),
                                     face_turn(second, second_turns)});
                }
            }
        }
    }

    for (const Axis axis : axes()) {
        for (int layer = 1; layer <= size - 2; ++layer) {
            for (const int slice_turns : {1, -1, 2}) {
                const Turn slice = layer_turn(axis, layer, slice_turns, size);

                for (const Face held : faces()) {
                    for (const int hold : {1, -1, 2}) {
                        const Turn holding = face_turn(held, hold);

                        for (const Face against : faces()) {
                            for (const int turns : {1, -1, 2}) {
                                const Turn other = face_turn(against, turns);

                                tools.push_back(written_out(
                                    Commutator{{holding, slice}, {other}}));
                                tools.push_back(written_out(
                                    Commutator{{slice, holding}, {other}}));
                                tools.push_back(written_out(Commutator{
                                    {holding, slice, undone(holding)},
                                    {other}}));
                            }
                        }
                    }
                }
            }
        }
    }
    return tools;
}

/**
 * The turns a tool may be preceded by, standing still first.
 *
 * For the centres a setup is free: a face turn cannot move a centre piece off
 * its face, so it never changes how many are home -- it only changes *which*
 * of a face's pieces sits where a tool will reach, and one is never taken back
 * because leaving it on costs a move and nothing else.
 */
[[nodiscard]] std::vector<Turn> face_setups()
{
    std::vector<Turn> before{Turn{Face::Up, 1, 0}};
    for (const Face face : faces()) {
        for (const int turns : {1, -1, 2}) {
            before.push_back(face_turn(face, turns));
        }
    }
    return before;
}

/** Whether a turn is one of the six outer faces rather than an inner slice. */
[[nodiscard]] bool is_face_turn(const Turn& turn) noexcept
{
    return turn.depth == 1;
}

/** Where a sticker ends up after one turn. */
[[nodiscard]] SurfaceSticker carried_one(SurfaceSticker cell, const Turn& step, int size)
{
    return moved_sticker(cell, to_move(step, size), size);
}

/** Where a sticker ends up after a run of turns. */
[[nodiscard]] SurfaceSticker carried_through(SurfaceSticker cell,
                                      const std::vector<Turn>& steps, int size)
{
    for (const auto& step : steps) cell = carried_one(cell, step, size);
    return cell;
}

/**
 * Which faces a run of turns carries pieces from, and to.
 *
 * Bit `j` of entry `i` means: some sticker of face `i` ends up on face `j`.
 * The count at home can only rise if a piece that is on the wrong face lands
 * on its own, so a tool that carries nothing from a face holding a stranger to
 * that stranger's home cannot help -- and most rounds, most of them carry
 * nothing of the sort.
 */
using Carries = std::array<std::uint8_t, kFaceCount>;

/**
 * The same carries, said in the frame a setup leaves behind.
 *
 * A body's cells are numbered before the setup and reached after it, so what
 * a round knows about the cube -- which faces hold strangers, which faces want
 * them -- has to be carried across to be compared with what a body does.
 */
[[nodiscard]] Carries carried_into(const Carries& wanted,
                                   const std::array<Face, kFaceCount>& carried)
{
    Carries out{};
    for (std::size_t from = 0; from < kFaceCount; ++from) {
        for (std::size_t to = 0; to < kFaceCount; ++to) {
            if ((wanted[from] & (1U << to)) == 0) continue;
            out[face_index(carried[from])] |= static_cast<std::uint8_t>(
                1U << face_index(carried[to]));
        }
    }
    return out;
}

[[nodiscard]] bool worth_trying(const Carries& tool, const Carries& wanted)
{
    for (std::size_t i = 0; i < kFaceCount; ++i) {
        if ((tool[i] & wanted[i]) != 0) return true;
    }
    return false;
}

/**
 * One kind of sticker, as a puzzle of numbered cells rather than a cube.
 *
 * Every tool is a fixed permutation of the cells -- moves shuffle slots, and
 * which slot goes where has nothing to do with what is sitting in it. So a
 * tool is worked out once, as a short list of "the sticker here ends up
 * there", and what a round then costs is a handful of lookups per tool rather
 * than a copy of the cube and eight turns of it. That difference is the
 * difference between a nine by nine taking seconds and taking minutes.
 */
class Board {
public:
    Board(int size, std::vector<SurfaceSticker> cells)
        : Board(size, cells, home_colours(cells))
    {
    }

    Board(int size, std::vector<SurfaceSticker> cells, std::vector<FaceColor> wants)
        : size_(size),
          cells_(std::move(cells)),
          wants_(std::move(wants)),
          index_(kFaceCount * static_cast<std::size_t>(size * size), -1),
          color_slots_(cells_.size())
    {
        for (std::size_t i = 0; i < cells_.size(); ++i) {
            const std::size_t slot = slot_of(cells_[i]);
            index_[slot] = static_cast<int>(i);
            color_slots_[i] = slot;
        }

        // Whether every cell here shows exactly one face, which is what a
        // centre is. Told from the position rather than asked of the caller:
        // a cell at an extreme on one axis and inside on the other two is on
        // one face and no other.
        centres_only_ = true;
        for (const auto& cell : cells_) {
            int extremes = 0;
            for (const Axis axis : axes()) {
                const int at = coordinate_on(axis, cell.position());
                if (at == 0 || at == size - 1) ++extremes;
            }
            if (extremes != 1) centres_only_ = false;
        }

        // Which cells lie at each coordinate along each axis, which is what
        // `hops` reads a turn's layers with.
        for (auto& axis : along_) {
            axis.assign(static_cast<std::size_t>(size), {});
        }
        for (std::size_t i = 0; i < cells_.size(); ++i) {
            for (const Axis axis : axes()) {
                const auto at = static_cast<std::size_t>(
                    coordinate_on(axis, cells_[i].position()));
                along_[axis_index(axis)][at].push_back(static_cast<int>(i));
            }
        }

        // Which faces have a cell asking for each colour. Nearly always the
        // one face that wears it, but a target that has two rows swapped puts
        // a colour on a face it does not belong to -- and this is read to say
        // whether a tool could carry a stranger somewhere it is wanted.
        for (std::size_t i = 0; i < cells_.size(); ++i) {
            asked_for_[static_cast<std::size_t>(wants_[i])] |=
                static_cast<std::uint8_t>(1U << face_index(cells_[i].face));
        }
    }

    [[nodiscard]] const std::vector<SurfaceSticker>& cells() const noexcept
    {
        return cells_;
    }

    /** Read a prepared cell directly from the cube's native face storage. */
    [[nodiscard]] FaceColor color_at(const Cube& cube,
                                    std::size_t cell) const noexcept
    {
        assert(cube.size() == size_ && cell < color_slots_.size());
        return cube.view().flat().data()[color_slots_[cell]];
    }

    /** How many stickers there are to get right. */
    [[nodiscard]] int target() const noexcept
    {
        return static_cast<int>(cells_.size());
    }

    /** How many of them are showing the colour asked of them. */
    [[nodiscard]] int at_home(const Cube& cube) const
    {
        int count = 0;
        for (std::size_t i = 0; i < cells_.size(); ++i) {
            if (color_at(cube, i) == wants_[i]) {
                ++count;
            }
        }
        return count;
    }

    /** Which cell a sticker at each cell lands in, after a run of turns. */
    [[nodiscard]] std::vector<int> landing(const std::vector<Turn>& steps) const
    {
        std::vector<int> lands(cells_.size(), 0);
        for (std::size_t i = 0; i < cells_.size(); ++i) {
            lands[i] = index_[slot_of(carried_through(cells_[i], steps, size_))];
            assert(lands[i] >= 0);
        }
        return lands;
    }

    /**
     * The cells a run of turns actually moves, as "from here to there".
     *
     * Read from the layers the turns name rather than from the whole board.
     * A cell no step of the run ever has in its layers is skipped by every
     * one of them, so it is where it started -- and a commutator names four
     * or eight layers out of a cube's worth. Working the whole board out to
     * discover that is what made building a size's tables cost a minute at
     * twenty-eight by twenty-eight, for an answer that is eight cells long.
     *
     * A step's layers are a set of *positions*, and a cell has reached its
     * position by the time that step runs, so each one is carried back
     * through the steps in front of it to say which cell started there. The
     * result is a superset of what moves -- every candidate is then asked
     * properly -- which is all it has to be.
     *
     * The pairs come out in cell order, as they did when the whole board was
     * walked. That is not cosmetic: the edge stage puts these vectors in a
     * set to tell two tools that do the same thing apart, and two orderings
     * of the same permutation would be two entries.
     */
    [[nodiscard]] std::vector<std::pair<int, int>> hops(
        const std::vector<Turn>& steps) const
    {
        // A face turn's layer is a whole face, which on a board of centres is
        // most of what there is -- and walking it is what a run of eight turns
        // spends nearly all of its preparation on. It can be left out, and
        // exactly when is worth saying rather than assuming.
        //
        // On a board of centres a face turn moves that face's centres and no
        // others: a centre of another face is inside on the turning axis, so
        // the layer misses it. And it leaves them on their own face. So a cell
        // that never falls in a *slice* is only ever spun by its own face's
        // turns, and if those add up to nothing it ends where it began. Which
        // is the condition below -- and when it does not hold, the face is
        // walked after all.
        std::array<int, kFaceCount> spun{};
        for (const auto& step : steps) {
            if (is_face_turn(step)) spun[face_index(step.face)] += step.turns;
        }
        bool faces_cancel = true;
        for (const int turns : spun) {
            if (normalized_turns(turns) != 0) faces_cancel = false;
        }
        const bool skip_faces = centres_only_ && faces_cancel;

        // How many cells the layers name between them, before any of them are
        // walked. A face turn on a small cube names most of the board, and
        // carrying every one of those back through the steps in front of it
        // costs more than reading the board straight through -- so when the
        // layers are not the smaller half, the board is read straight through.
        // Both roads end at the same pairs; this only picks the shorter.
        std::size_t named = 0;
        for (std::size_t k = 0; k < steps.size(); ++k) {
            if (skip_faces && is_face_turn(steps[k])) continue;
            const auto move = to_move(steps[k], size_);
            const auto axis = axis_index(move.axis);
            for (int at = 0; at < size_; ++at) {
                if (!in_layers(move.layers, at)) continue;
                named += along_[axis][static_cast<std::size_t>(at)].size();
            }
        }

        std::vector<int> candidates;
        if (named >= cells_.size()) {
            candidates.resize(cells_.size());
            for (std::size_t i = 0; i < candidates.size(); ++i) {
                candidates[i] = static_cast<int>(i);
            }
            std::vector<std::pair<int, int>> moved;
            for (const int i : candidates) {
                const auto& cell = cells_[static_cast<std::size_t>(i)];
                const int lands =
                    index_[slot_of(carried_through(cell, steps, size_))];
                assert(lands >= 0);
                if (lands != i) moved.emplace_back(i, lands);
            }
            return moved;
        }

        std::vector<char> seen(cells_.size(), 0);

        for (std::size_t k = 0; k < steps.size(); ++k) {
            if (skip_faces && is_face_turn(steps[k])) continue;
            const auto move = to_move(steps[k], size_);
            const auto axis = axis_index(move.axis);

            for (int at = 0; at < size_; ++at) {
                if (!in_layers(move.layers, at)) continue;
                for (const int cell : along_[axis][static_cast<std::size_t>(at)]) {
                    SurfaceSticker origin = cells_[static_cast<std::size_t>(cell)];
                    for (std::size_t j = k; j-- > 0;) {
                        origin = carried_one(origin, undone(steps[j]), size_);
                    }
                    const int started = index_[slot_of(origin)];
                    assert(started >= 0);
                    if (seen[static_cast<std::size_t>(started)]) continue;
                    seen[static_cast<std::size_t>(started)] = 1;
                    candidates.push_back(started);
                }
            }
        }

        std::sort(candidates.begin(), candidates.end());

        std::vector<std::pair<int, int>> moved;
        for (const int i : candidates) {
            const auto& cell = cells_[static_cast<std::size_t>(i)];
            const int lands = index_[slot_of(carried_through(cell, steps, size_))];
            assert(lands >= 0);
            if (lands != i) moved.emplace_back(i, lands);
        }
        return moved;
    }

    [[nodiscard]] Carries carries(
        const std::vector<std::pair<int, int>>& moved) const
    {
        Carries out{};
        for (const auto& [from, to] : moved) {
            const Face left = cells_[static_cast<std::size_t>(from)].face;
            const Face arrived = cells_[static_cast<std::size_t>(to)].face;
            if (left == arrived) continue;
            out[face_index(left)] |=
                static_cast<std::uint8_t>(1U << face_index(arrived));
        }
        return out;
    }

    /** The carries that would put a stranger home, read off the cube as it is. */
    [[nodiscard]] Carries wanted(const Cube& cube) const
    {
        Carries out{};
        for (std::size_t i = 0; i < cells_.size(); ++i) {
            const FaceColor colour = color_at(cube, i);
            if (colour == wants_[i]) continue;
            out[face_index(cells_[i].face)] |=
                asked_for_[static_cast<std::size_t>(colour)];
        }
        return out;
    }

    /** Whether a run of turns leaves every one of these stickers on its face. */
    [[nodiscard]] bool keeps_faces(
        const std::vector<std::pair<int, int>>& moved) const
    {
        for (const auto& [from, to] : moved) {
            if (cells_[static_cast<std::size_t>(from)].face !=
                cells_[static_cast<std::size_t>(to)].face) {
                return false;
            }
        }
        return true;
    }

    /**
     * How many more stickers would be home after a setup and a tool.
     *
     * Only the cells the tool moves can change: a setup shuffles a face's
     * pieces among that face's own cells, and a cell's own colour is the same
     * all over one face.
     */
    [[nodiscard]] int gain(const Cube& cube,
                           const std::vector<int>& setup_source,
                           const std::vector<std::pair<int, int>>& moved) const
    {
        int delta = 0;
        for (const auto& [from, to] : moved) {
            const auto source = static_cast<std::size_t>(
                setup_source[static_cast<std::size_t>(from)]);
            const FaceColor colour = color_at(cube, source);
            if (colour == wants_[static_cast<std::size_t>(to)]) ++delta;
            if (colour == wants_[static_cast<std::size_t>(source)]) --delta;
        }
        return delta;
    }

    /**
     * How many more stickers would be home after a setup, a tool, and the
     * setup taken back.
     *
     * A hop of the tool is read in the frame the setup left behind, so both
     * ends of it come back through the setup: the sticker that moves is the
     * one sitting where the setup sent this cell's, and it lands where the
     * setup sent that cell's. Nothing outside those cells changes at all,
     * which is the whole reason a setup is safe to try here.
     */
    [[nodiscard]] int gain_within(
        const Cube& cube, const std::vector<int>& source,
        const std::vector<std::pair<int, int>>& moved) const
    {
        int delta = 0;
        for (const auto& [from, to] : moved) {
            const auto here = static_cast<std::size_t>(
                source[static_cast<std::size_t>(from)]);
            const auto lands =
                static_cast<std::size_t>(source[static_cast<std::size_t>(to)]);

            const FaceColor colour = color_at(cube, here);
            if (colour == wants_[lands]) ++delta;
            if (colour == wants_[here]) --delta;
        }
        return delta;
    }

private:
    [[nodiscard]] std::size_t slot_of(const SurfaceSticker& cell) const noexcept
    {
        return surface_index(cell, size_);
    }

    int size_;
    std::vector<SurfaceSticker> cells_;
    std::vector<FaceColor> wants_;
    std::array<std::uint8_t, kFaceCount> asked_for_{};
    std::vector<int> index_;
    std::vector<std::size_t> color_slots_;
    std::array<std::vector<std::vector<int>>, kAxisCount> along_;
    bool centres_only_ = false;
};

/**
 * Which of the separate closed sets each cell of a board lives in.
 *
 * A cell's set is everywhere the sticker in it can be carried to. Read by
 * walking rather than by arithmetic: each generator is asked where it sends
 * every cell, and cells reached from one another are one set. The answer is a
 * fact about a size rather than about any position of it, so it is worked out
 * once and kept.
 *
 * Which turns count as generators is the caller's, and it matters. The edges
 * are walked with the face turns alone, because a slice keeps an edge piece at
 * its own depth and so merges nothing. The centres need the slices as well: a
 * face turn only spins a face's centres among that face's own cells, and
 * without the slices every face would come out as a puzzle of its own.
 */
[[nodiscard]] std::vector<int> orbits_under(const Board& board,
                                            const std::vector<Turn>& generators)
{
    std::vector<std::vector<int>> steps;
    steps.reserve(generators.size());
    for (const auto& turn : generators) steps.push_back(board.landing({turn}));
    return orbits(steps, board.cells().size());
}

/** One tool, worked out once and read every round. */
struct Prepared {
    std::vector<Turn> steps;
    std::vector<std::pair<int, int>> moved;
    Carries carries{};
};

/** One choice of setup and tool, or nothing when none was any good. */
struct Choice {
    std::size_t tool = 0;
    std::size_t setup = 0;
    bool found = false;
};

/**
 * Everything a size needs worked out, once.
 *
 * Which cells there are, which tools move which of them, where a setup leaves
 * each cell: none of it depends on what the cube is showing, so none of it
 * belongs inside a round or inside a call. A reduction runs the two stages
 * several times over -- twice for the two targets, again after a perturbation
 * -- and without this each of those runs would begin by working out the same
 * tens of thousands of permutations again.
 *
 * This is what the `Solver` interface has always been shaped for. The comment
 * on it says a search-based solver builds its tables once and holds them, and
 * this is that; the only wrinkle is that the size is not known until a cube
 * arrives, so a solver holds one of these per size it has been asked about
 * rather than one built in its constructor.
 */
class CentreWorkshop {
public:
    explicit CentreWorkshop(int size) : centres_(size, centre_cells(size))
    {
        for (const auto& tool : centre_shapes(size)) {
            Prepared prepared;
            prepared.steps = written_out(tool);
            prepared.moved = centres_.hops(prepared.steps);
            prepared.carries = centres_.carries(prepared.moved);
            if (prepared.moved.empty()) continue;
            centre_tools_.push_back(std::move(prepared));
        }

        centre_setups_ = face_setups();
        for (const auto& setup : centre_setups_) {
            centre_sources_.push_back(
                normalized_turns(setup.turns) == 0
                    ? standing_still(centres_)
                    : sources_after(centres_, {setup}));
        }

    }

    [[nodiscard]] const Board& centres() const noexcept { return centres_; }
    [[nodiscard]] const std::vector<Prepared>& tools() const noexcept
    {
        return centre_tools_;
    }
    [[nodiscard]] const std::vector<Turn>& setups() const noexcept
    {
        return centre_setups_;
    }
    [[nodiscard]] const std::vector<std::vector<int>>& sources() const noexcept
    {
        return centre_sources_;
    }

private:
    /** Each cell answering for itself, which is what no setup at all does. */
    [[nodiscard]] static std::vector<int> standing_still(const Board& board)
    {
        std::vector<int> source(board.cells().size(), 0);
        for (std::size_t i = 0; i < source.size(); ++i) {
            source[i] = static_cast<int>(i);
        }
        return source;
    }

    /**
     * Where the sticker now in each cell came from, after a setup.
     *
     * The inverse of what the setup did, which is what reading a tool's cells
     * needs: a tool says "the piece here goes there", and here is after the
     * setup.
     */
    [[nodiscard]] static std::vector<int> sources_after(
        const Board& board, const std::vector<Turn>& setup)
    {
        std::vector<int> source(board.cells().size(), 0);
        const auto lands = board.landing(setup);
        for (std::size_t i = 0; i < lands.size(); ++i) {
            source[static_cast<std::size_t>(lands[i])] = static_cast<int>(i);
        }
        return source;
    }

    Board centres_;
    std::vector<Prepared> centre_tools_;
    std::vector<Turn> centre_setups_;
    std::vector<std::vector<int>> centre_sources_;
};

/**
 * Which of the separate worlds each edge sticker lives in.
 *
 * An edge piece can never leave the pair of depths it starts at. A face turn
 * carries a whole row to another row and the piece keeps its distance from the
 * corner, so the pieces one layer in stay one layer in for ever, the ones two
 * layers in stay two, and a big cube's edges are not one puzzle but several
 * side by side -- one for each pair of depths, three of them on an eight by
 * eight.
 *
 * Worse, a piece carried to the *other* end of its own row comes back with its
 * two colours the other way round, which is what makes the two ends tell
 * apart: they are two worlds rather than one, and this counts six of them on
 * an eight by eight where the depths alone would say three.
 *
 * Read by walking, not by arithmetic, which `orbits_under` does. The face
 * turns are generators enough: a slice keeps an edge piece at the depth it
 * found it, so adding the slices would merge nothing.
 */
[[nodiscard]] std::vector<int> facelet_worlds(const Board& board)
{
    std::vector<Turn> generators;
    for (const Face face : faces()) {
        for (const int turns : {1, -1}) {
            generators.push_back(face_turn(face, turns));
        }
    }
    return orbits_under(board, generators);
}

/**
 * The edge half of the same idea, held apart from the centre half.
 *
 * Apart because the two are wanted at different times and cost different
 * amounts. The centre stage is asked for on its own -- by a test, and by the
 * reduction each time it perturbs the cube -- and building the edge tables it
 * would never look at is most of the work. Sizing them tells the story: for a
 * nine by nine the centre tables are thousands of permutations and the edge
 * tables are tens of thousands, because every one of those has to be asked
 * whether it puts the centres back.
 */
class EdgeWorkshop {
public:
    explicit EdgeWorkshop(int size)
        : edges_(size, edge_cells(size),
                 edge_wants(edge_cells(size), size, false)),
          swapped_(size, edge_cells(size),
                   edge_wants(edge_cells(size), size, true))
    {
        const Board centres{size, centre_cells(size)};

        // Which world each sticker lives in, the other sticker of its own
        // piece, and where a piece of each pair of colours belongs in each
        // world. Between them these say, of any cube, how many swaps each
        // world is from finished -- which is the one thing about a big cube
        // that no tool here can change and every tool here has to respect.
        worlds_ = facelet_worlds(edges_);
        for (const int world : worlds_) {
            if (world + 1 > static_cast<int>(homes_.size())) {
                homes_.resize(static_cast<std::size_t>(world) + 1,
                              std::vector<int>(kFaceCount * kFaceCount, -1));
                depths_.resize(static_cast<std::size_t>(world) + 1);
                classes_.resize(static_cast<std::size_t>(world) + 1, 0);
            }
        }

        const auto& cells = edges_.cells();
        partner_.assign(cells.size(), 0);
        for (std::size_t i = 0; i < cells.size(); ++i) {
            for (std::size_t j = 0; j < cells.size(); ++j) {
                if (j != i && cells[j].position() == cells[i].position()) partner_[i] = j;
            }
        }

        for (std::size_t i = 0; i < cells.size(); ++i) {
            const auto world = static_cast<std::size_t>(worlds_[i]);
            homes_[world][pair_of(home_colour(cells[i].face),
                                  home_colour(cells[partner_[i]].face))] =
                static_cast<int>(i);

            // The turn that changes this world's count and no other's: a
            // quarter of the one inner layer its pieces sit in. Which end of
            // the cube that layer is measured from does not matter -- a piece
            // one layer in from either end is in the same world.
            for (const Axis axis : axes()) {
                const int at = coordinate_on(axis, cells[i].position());
                if (at == 0 || at == size - 1) continue;
                depths_[world] = layer_turn(axis, at, 1, size);
                classes_[world] = at < size - 1 - at ? at : size - 1 - at;
            }
        }

        // The narrowest a fine tool is allowed to be broad. A commutator that
        // moves more edge pieces than this is doing coarse work in six moves,
        // and one or two face turns already do coarse work in one or two.
        constexpr std::size_t kNarrow = 12;

        std::set<std::vector<std::pair<int, int>>> seen;
        for (const auto& steps : edge_shapes(size)) {
            Prepared prepared;
            prepared.steps = steps;
            prepared.moved = edges_.hops(steps);
            prepared.carries = edges_.carries(prepared.moved);
            if (prepared.moved.empty()) continue;
            if (steps.size() > 2 && prepared.moved.size() > kNarrow) continue;

            // The centres are already home when this stage runs, and every
            // piece of one face's block is the same colour, so a tool keeps
            // them exactly when it sends each centre sticker to a cell of the
            // same face. Asked rather than argued.
            if (!centres.keeps_faces(centres.hops(steps))) continue;

            // Two tools that permute the same cells the same way are one tool,
            // and the first of them is the shortest: the coarse ones are
            // generated first. Without this the fine families arrive in their
            // thousands, all saying the same few things.
            if (!seen.insert(prepared.moved).second) continue;
            bodies_.push_back(std::move(prepared));
        }

        // Each body is tried inside every setup: the setup, the body, the
        // setup taken back. Undoing it is what makes a setup free to try -- a
        // conjugated tool moves exactly the pieces the body moves, carried to
        // wherever the setup put them, and nothing else on the cube knows it
        // happened. So the handful of shapes above become every place on the
        // cube they could apply, and none of them leaves a mess to be scored.
        setups_.push_back({});
        for (const Face face : faces()) {
            for (const int turns : {1, -1, 2}) {
                setups_.push_back({face_turn(face, turns)});
            }
        }

        // Where the cheap setups end and the rest begin. Almost every round is
        // decided by a tool sitting under one face turn or none, and looking
        // at the two-turn setups as well costs sixteen times as much for an
        // answer that is nearly always the same. So they are held back for the
        // rounds where nothing else worked.
        shallow_ = setups_.size();

        for (const Face face : faces()) {
            for (const int turns : {1, -1, 2}) {
                for (const Face second : faces()) {
                    if (second == face) continue;
                    for (const int again : {1, -1, 2}) {
                        setups_.push_back({face_turn(face, turns),
                                           face_turn(second, again)});
                    }
                }
            }
        }

        for (const auto& setup : setups_) {
            // Where a setup sends each face. A body's cells are read in the
            // frame the setup leaves behind, so this is what turns "which
            // strangers are out there" into a question about the body itself.
            std::array<Face, kFaceCount> carried{};
            for (const Face face : faces()) {
                Face lands = face;
                for (const auto& step : setup) {
                    lands = carried_by(step.face, step.turns, lands);
                }
                carried[face_index(face)] = lands;
            }
            face_maps_.push_back(carried);

            std::vector<int> source(edges_.cells().size(), 0);
            if (setup.empty()) {
                for (std::size_t i = 0; i < source.size(); ++i) {
                    source[i] = static_cast<int>(i);
                }
            } else {
                const auto lands = edges_.landing(setup);
                for (std::size_t i = 0; i < lands.size(); ++i) {
                    source[static_cast<std::size_t>(lands[i])] =
                        static_cast<int>(i);
                }
            }
            sources_.push_back(std::move(source));
        }
    }

    [[nodiscard]] const Board& edges(bool swap_two_pairs) const noexcept
    {
        return swap_two_pairs ? swapped_ : edges_;
    }
    [[nodiscard]] const std::vector<Prepared>& bodies() const noexcept
    {
        return bodies_;
    }
    [[nodiscard]] const std::vector<std::vector<Turn>>& setups() const noexcept
    {
        return setups_;
    }
    [[nodiscard]] const std::vector<std::vector<int>>& sources() const noexcept
    {
        return sources_;
    }
    [[nodiscard]] const std::vector<std::array<Face, kFaceCount>>& face_maps()
        const noexcept
    {
        return face_maps_;
    }
    [[nodiscard]] std::size_t shallow() const noexcept { return shallow_; }

    /**
     * The turns that put every world an even number of swaps from finished.
     *
     * Every tool the edge stage has moves each world an even number of swaps
     * -- a face turn carries four rows round in cycles of four, a commutator
     * undoes itself -- so a world sitting an odd number away can never be
     * finished, and the stage would look for ever. The one move that changes
     * a world's count is a quarter turn of an inner layer, and it changes that
     * world alone. So they are counted and turned, once, rather than guessed
     * at by trying.
     */
    [[nodiscard]] std::vector<Turn> evening_turns(const Cube& cube) const
    {
        const auto& cells = edges_.cells();

        std::vector<int> home(cells.size(), 0);
        for (std::size_t i = 0; i < cells.size(); ++i) {
            const FaceColor colour = edges_.color_at(cube, i);
            const FaceColor other = edges_.color_at(cube, partner_[i]);

            const int lands =
                homes_[static_cast<std::size_t>(worlds_[i])]
                      [pair_of(colour, other)];
            assert(lands >= 0);
            home[i] = lands;
        }

        std::vector<Turn> turns;
        std::vector<bool> counted(homes_.size(), false);
        for (std::size_t world = 0; world < homes_.size(); ++world) {
            if (counted[world]) continue;

            // This world's cells numbered among themselves, and the cell
            // each one's piece belongs in, numbered the same way.
            std::vector<int> place(cells.size(), -1);
            std::vector<std::size_t> members;
            for (std::size_t i = 0; i < cells.size(); ++i) {
                if (static_cast<std::size_t>(worlds_[i]) != world) continue;
                place[i] = static_cast<int>(members.size());
                members.push_back(i);
            }
            std::vector<int> belongs;
            belongs.reserve(members.size());
            for (const std::size_t i : members) {
                const int there = place[static_cast<std::size_t>(home[i])];
                assert(there >= 0 && "a piece stays in its own world");
                belongs.push_back(there);
            }
            const bool odd = is_odd_permutation(belongs);

            // The two worlds of one pair of depths are the same pieces read
            // from their two ends, so they are one count and one turn.
            for (std::size_t other = world; other < homes_.size(); ++other) {
                if (classes_[other] == classes_[world]) counted[other] = true;
            }
            if (odd) turns.push_back(depths_[world]);
        }
        return turns;
    }

private:
    Board edges_;
    Board swapped_;
    std::vector<Prepared> bodies_;
    std::vector<std::vector<Turn>> setups_;
    std::vector<std::vector<int>> sources_;
    std::vector<std::array<Face, kFaceCount>> face_maps_;
    std::vector<int> worlds_;
    std::vector<std::size_t> partner_;
    std::vector<std::vector<int>> homes_;
    std::vector<Turn> depths_;
    std::vector<int> classes_;
    std::size_t shallow_ = 0;

    [[nodiscard]] static std::size_t pair_of(FaceColor one,
                                             FaceColor other) noexcept
    {
        return static_cast<std::size_t>(one) * kFaceCount +
               static_cast<std::size_t>(other);
    }

};

/**
 * Every face's centre block one colour.
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
void build_centres(Work& work, const CentreWorkshop& shop)
{
    const Board& board = shop.centres();
    if (board.cells().empty()) return;

    const auto& tools = shop.tools();
    const auto& before = shop.setups();
    const auto& sources = shop.sources();

    const int target = board.target();

    for (int guard = 0; board.at_home(work.cube()) < target; ++guard) {
        assert(guard < target * 2 + 8);
        const auto wanted = board.wanted(work.cube());

        Choice choice;
        int best = 0;

        for (std::size_t i = 0; i < tools.size(); ++i) {
            if (!worth_trying(tools[i].carries, wanted)) continue;
            for (std::size_t s = 0; s < sources.size(); ++s) {
                const int delta =
                    board.gain(work.cube(), sources[s], tools[i].moved);
                if (delta <= best) continue;
                best = delta;
                choice = Choice{i, s, true};
            }
        }

        assert(choice.found);
        work.apply(before[choice.setup]);
        for (const auto& step : tools[choice.tool].steps) work.apply(step);
    }
}

/**
 * Every edge piece home, without disturbing the centres that are already home.
 *
 * Aimed higher than a reduction needs. Reduction asks only that each row wear
 * one pair of colours; this asks that each row wear *its own* pair, the right
 * way round. The reason is the same one the centres gave for reading their
 * target from `solved_color()` rather than from a centre piece: a score has to
 * be a fact about a cell on its own for a tool to be worked out once and read
 * cheaply ever after, and "the colours this row happens to have settled on" is
 * a fact about the row that changes as the row changes.
 *
 * Two things fall out of aiming there. A pair cannot end up turned round, so
 * of the two parities an even cube can hold only one survives this stage. And
 * the stage that follows starts from edges that are already home, so the moves
 * spent here are not spent twice.
 */
[[nodiscard]] bool build_edges(Work& work, const EdgeWorkshop& shop,
                              bool swap_two_pairs)
{
    const Board& board = shop.edges(swap_two_pairs);
    if (board.cells().empty()) return true;

    const auto& bodies = shop.bodies();
    const auto& before = shop.setups();
    const auto& sources = shop.sources();
    const auto& face_maps = shop.face_maps();
    const std::size_t shallow = shop.shallow();

    const int target = board.target();

    // How long a stage will go on making no headway before it says the target
    // is the wrong one. A round that gains nothing is ordinary -- three pieces
    // in a cycle cannot be put right by any one tool here, since every one of
    // them moves four, so a round that only breaks even is what gives the next
    // one something it can finish. A run of them that never turns into a gain
    // is the other thing, and telling the two apart is a matter of waiting.
    constexpr int kPatience = 24;

    int most_home = board.at_home(work.cube());
    int since_gain = 0;

    for (int guard = 0; board.at_home(work.cube()) < target; ++guard) {
        assert(guard < target * 16 + 256);
        if (since_gain > kPatience) return false;

        // Which strangers are out there, and where each of them would be
        // welcome. A tool that carries nothing from a face holding one to a
        // face asking for it cannot raise the count, and towards the end of a
        // stage that is nearly all of them -- which is where the rounds are.
        const auto wanted = board.wanted(work.cube());

        Choice choice;
        int best = 0;
        int most = -(1 << 20);

        for (const std::size_t reach : {shallow, sources.size()}) {
            for (std::size_t s = 0; s < reach; ++s) {
                const auto here = carried_into(wanted, face_maps[s]);

                for (std::size_t i = 0; i < bodies.size(); ++i) {
                    if (!worth_trying(bodies[i].carries, here)) continue;
                    const int delta = board.gain_within(
                        work.cube(), sources[s], bodies[i].moved);
                    if (delta > most) most = delta;
                    if (delta <= best) continue;
                    best = delta;
                    choice = Choice{i, s, true};
                }
            }
            if (choice.found) break;
        }

        // A round that gains nothing still moves, and which of the tools it
        // takes walks along the list as the rounds go by so that a stall does
        // not sit still. What it cannot do is get anywhere when the target is
        // an odd number of swaps away -- every tool above is an even
        // permutation of the edge pieces, a face turn carrying four rows round
        // and a commutator undoing itself twice over -- and that is what the
        // patience above is counting out.
        if (!choice.found) {
            std::size_t among = 0;
            for (std::size_t s = 0; s < sources.size(); ++s) {
                const auto here = carried_into(wanted, face_maps[s]);
                for (std::size_t i = 0; i < bodies.size(); ++i) {
                    if (!worth_trying(bodies[i].carries, here)) continue;
                    if (board.gain_within(work.cube(), sources[s],
                                          bodies[i].moved) == most) {
                        ++among;
                    }
                }
            }
            assert(among > 0);

            const auto nth = static_cast<std::size_t>(guard) % among;
            std::size_t seen_here = 0;
            for (std::size_t s = 0; s < sources.size() && !choice.found; ++s) {
                const auto here = carried_into(wanted, face_maps[s]);
                for (std::size_t i = 0; i < bodies.size(); ++i) {
                    if (!worth_trying(bodies[i].carries, here)) continue;
                    if (board.gain_within(work.cube(), sources[s],
                                          bodies[i].moved) != most) {
                        continue;
                    }
                    if (seen_here++ != nth) continue;
                    choice = Choice{i, s, true};
                    break;
                }
            }
        }
        assert(choice.found);

        for (const auto& step : before[choice.setup]) work.apply(step);
        for (const auto& step : bodies[choice.tool].steps) work.apply(step);
        for (const auto& step : taken_back(before[choice.setup])) work.apply(step);

        const int now = board.at_home(work.cube());
        since_gain = now > most_home ? 0 : since_gain + 1;
        most_home = now > most_home ? now : most_home;
    }
    return true;
}

/**
 * The reduction proper, on a workshop somebody else is holding: both halves,
 * and the one thing left over when they are done.
 *
 * A cube can come out of a reduction in a position no three by three can be
 * in, because pieces a three by three cannot tell apart -- the centres of one
 * face -- can hold a permutation the rest of the cube then has to answer for.
 * Two things follow from it, and neither is met with a written sequence.
 *
 * One is corners sitting in an odd order, which is seen by projecting the
 * reduced cube and asking. What answers it is a change of target: two rows are
 * told to want each other's colours, which is one swap of the three by three's
 * edges and puts the same oddness on the other side of the ledger.
 *
 * The other is the edge pieces' own arrangement, which the stage cannot reach
 * at all when it is an odd number of swaps from what is being asked -- every
 * tool that keeps the centres is an even permutation of them. What answers
 * that is the one odd move a big cube has, a quarter turn of a single inner
 * slice, after which the centres are gathered again.
 *
 * What is left is always reduced, and always a cube the three-layer stages
 * can finish.
 */
void build_reduction(Work& work, const CentreWorkshop& centres,
                     const EdgeWorkshop& edges)
{
    build_centres(work, centres);

    // What is left over is counted rather than guessed at.
    //
    // A big cube's edges are not one puzzle but several side by side: a piece
    // one layer in from a corner stays one layer in whatever is turned, so the
    // pieces at each pair of depths are a separate world with its own count of
    // how many swaps it is from finished. Every tool the edge stage has moves
    // each world an even number of swaps, so a world sitting an odd number
    // away can never be finished by it -- and the only move that changes a
    // world's count is a quarter turn of an inner layer, which changes that
    // world's and no other's.
    //
    // So: count each world, turn the layer of every world that comes out odd,
    // and gather the centres those turns disturbed. Gathering them is itself
    // even in every world, so the counts stay where they were put.
    const auto evening = edges.evening_turns(work.cube());
    if (!evening.empty()) {
        for (const auto& turn : evening) work.apply(turn);
        build_centres(work, centres);
    }

    // The other thing a reduction can be left holding is the corners sitting
    // in an odd order, which is what an odd permutation of pieces a three by
    // three cannot tell apart -- the centres of one face -- looks like from
    // outside once everything else is home. No move mends that either, so the
    // target moves instead: two rows are asked to wear each other's colours,
    // which is one swap of the three by three's edges and puts the same
    // oddness on the other side of the ledger. Getting there from
    // everything-home is an even number of swaps in every world, which is
    // exactly what the tools can do.
    //
    // Which of the two targets a cube wants cannot be read off it as the
    // counts above can, because it is a fact about pieces that all look alike.
    // So it is tried, and nothing of a failed attempt is kept: its moves are
    // real moves that would be played, and what they were spent on was finding
    // out.
    for (const bool swap_two_pairs : {false, true}) {
        Work trial = work;
        if (!build_edges(trial, edges, swap_two_pairs)) continue;
        if (!reachable(parities(projected(trial.cube())))) continue;

        assert(reduced(trial.cube()));
        work = std::move(trial);
        return;
    }

    // Neither target, which is a stage that walked into a corner rather than
    // a cube that cannot be reduced. Shake it and count again.
    for (int attempt = 0;; ++attempt) {
        assert(attempt < 8);

        work.apply(
            layer_turn(axes()[static_cast<std::size_t>(attempt) % kAxisCount],
                       1 + (attempt / 3) % (work.size() - 2),
                       attempt % 2 == 0 ? 1 : -1, work.size()));
        build_centres(work, centres);

        for (const auto& turn : edges.evening_turns(work.cube())) {
            work.apply(turn);
        }
        build_centres(work, centres);

        for (const bool swap_two_pairs : {false, true}) {
            Work trial = work;
            if (!build_edges(trial, edges, swap_two_pairs)) continue;
            if (!reachable(parities(projected(trial.cube())))) continue;

            assert(reduced(trial.cube()));
            work = std::move(trial);
            return;
        }
    }
}

}  // namespace

std::vector<CubeMove> solve_centres(const Cube& state)
{
    Work work{state};
    build_centres(work, CentreWorkshop{state.size()});
    return work.take();
}

std::vector<CubeMove> reduce(const Cube& state)
{
    Work work{state};
    build_reduction(work, CentreWorkshop{state.size()},
                    EdgeWorkshop{state.size()});
    return work.take();
}

struct Reduction::Workshops {
    std::map<int, CentreWorkshop> centres_by_size;
    std::map<int, EdgeWorkshop> edges_by_size;
};

Reduction::Reduction() : prepared_(std::make_unique<Workshops>()) {}

Reduction::~Reduction() = default;

bool Reduction::supports(int size) const noexcept
{
    // Every size the domain can build that is not already a three by three.
    // The method has no size in it: a workshop is derived from the cube it is
    // asked about, so what bounds this is what the domain can build
    // rather than anything here. Above the packed limit the moves cannot be
    // recorded, which the application's own size cap answers for. What it
    // costs grows with the cube, and that is the caller's to warn about
    // rather than this one's to refuse.
    return size >= 4 && size < kLayerMaskBits;
}

std::vector<CubeMove> Reduction::solve(const Cube& state) const
{
    assert(supports(state.size()));
    if (state.is_solved()) return {};

    // Built the first time a size turns up and kept from then on. What it
    // costs is a second or so at nine by nine, and what it saves is that
    // second on every solve after the first.
    const int size = state.size();
    if (prepared_->centres_by_size.find(size) ==
        prepared_->centres_by_size.end()) {
        prepared_->centres_by_size.emplace(size, CentreWorkshop{size});
        prepared_->edges_by_size.emplace(size, EdgeWorkshop{size});
    }

    const SolverFrame frame{state};
    Work work{frame.cube()};
    build_reduction(work, prepared_->centres_by_size.at(size),
                    prepared_->edges_by_size.at(size));

    std::vector<CubeMove> moves = work.take();
    const std::vector<CubeMove> finish = solve_as_three_layers(work.cube());
    for (const CubeMove& move : finish) moves.push_back(move);
    return frame.restore(compress(moves));
}

}  // namespace rubiks::cube::solver
