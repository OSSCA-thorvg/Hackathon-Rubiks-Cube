#include "cube/solver/Reduction.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "cube/Cubie.hpp"
#include "cube/Surface.hpp"
#include "cube/solver/Turning.hpp"

namespace rubiks::cube::solver {
namespace {

constexpr std::array<Axis, 3> kAxes{Axis::X, Axis::Y, Axis::Z};

/** One cell of one face's centre block. */
struct CentreCell {
    CubiePosition at;
    Face face;
};

/**
 * Every centre cell of the cube, once each.
 *
 * A centre is a slot that shows exactly one face, which is a description that
 * needs no size: a three by three has six of them, a nine by nine has six
 * times forty-nine, and a two by two has none.
 */
[[nodiscard]] std::vector<CentreCell> centre_cells(int size)
{
    std::vector<CentreCell> cells;
    for (int x = 0; x < size; ++x) {
        for (int y = 0; y < size; ++y) {
            for (int z = 0; z < size; ++z) {
                const CubiePosition at{x, y, z};
                const auto showing = exposed_faces(at, size);
                if (showing.size() != 1) continue;
                cells.push_back(CentreCell{at, showing.front()});
            }
        }
    }
    return cells;
}

/** How many centre pieces are on the face they belong to. */
[[nodiscard]] int centres_home(const CubeState& cube,
                               const std::vector<CentreCell>& cells)
{
    int count = 0;
    for (const auto& cell : cells) {
        if (sticker_at(cube, cell.at, cell.face) == home_colour(cell.face)) {
            ++count;
        }
    }
    return count;
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
[[nodiscard]] std::vector<Commutator> centre_tools(int size)
{
    std::vector<Commutator> tools;

    for (const Face face : faces()) {
        for (const int face_turns : {1, -1, 2}) {
            for (const Axis axis : kAxes) {
                if (axis == axis_of(face)) continue;
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

    for (const Axis first : kAxes) {
        for (const Axis second : kAxes) {
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

    for (const Axis axis : kAxes) {
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
 * The turns a commutator may be preceded by, standing still first.
 *
 * A face turn cannot move a centre piece off its face, so it never changes how
 * many are home -- it only changes *which* of a face's pieces sits where a
 * commutator will reach. That is the whole of what a setup is for here, and
 * why one is never taken back: leaving it on costs a move and nothing else.
 */
[[nodiscard]] std::vector<Turn> setups()
{
    std::vector<Turn> before{Turn{Face::Up, 1, 1, 0}};
    for (const Face face : faces()) {
        for (const int turns : {1, -1, 2}) {
            before.push_back(face_turn(face, turns));
        }
    }
    return before;
}

/** Where a slot ends up after a run of turns. */
[[nodiscard]] CubiePosition carried_through(CubiePosition at,
                                            const std::vector<Turn>& steps,
                                            int size)
{
    for (const auto& step : steps) {
        const auto move = to_move(step, size);
        if ((move.layers & layer(coordinate_on(move.axis, at))) == 0) continue;
        for (int i = 0, n = normalized_turns(move.quarter_turns); i < n; ++i) {
            at = turned_position(move.axis, at, size);
        }
    }
    return at;
}

/**
 * Which faces a run of turns carries centre pieces from, and to.
 *
 * Bit `j` of entry `i` means: some centre cell of face `i` ends up on face
 * `j`. The count of centre pieces at home can only rise if a piece that is on
 * the wrong face lands on its own, so a tool that carries nothing from a face
 * holding a stranger to that stranger's home cannot help -- and most rounds,
 * most of them carry nothing of the sort.
 */
using Carries = std::array<std::uint8_t, kFaceCount>;

[[nodiscard]] bool worth_trying(const Carries& tool, const Carries& wanted)
{
    for (std::size_t i = 0; i < kFaceCount; ++i) {
        if ((tool[i] & wanted[i]) != 0) return true;
    }
    return false;
}

/**
 * The centres, as a puzzle of numbered cells rather than a cube.
 *
 * Every tool is a fixed permutation of the cells -- moves shuffle slots, and
 * which slot goes where has nothing to do with what is sitting in it. So a
 * tool is worked out once, as a short list of "the piece here ends up there",
 * and what a round then costs is a handful of lookups per tool rather than a
 * copy of the cube and eight turns of it. That difference is the difference
 * between a nine by nine taking seconds and taking minutes.
 */
class CentreBoard {
public:
    explicit CentreBoard(int size)
        : size_(size),
          cells_(centre_cells(size)),
          index_(static_cast<std::size_t>(size) * size * size, -1)
    {
        for (std::size_t i = 0; i < cells_.size(); ++i) {
            index_[slot_of(cells_[i].at)] = static_cast<int>(i);
        }
    }

    [[nodiscard]] const std::vector<CentreCell>& cells() const noexcept
    {
        return cells_;
    }

    /** Which cell a piece at each cell lands in, after a run of turns. */
    [[nodiscard]] std::vector<int> landing(const std::vector<Turn>& steps) const
    {
        std::vector<int> lands(cells_.size(), 0);
        for (std::size_t i = 0; i < cells_.size(); ++i) {
            lands[i] = index_[slot_of(carried_through(cells_[i].at, steps,
                                                      size_))];
            assert(lands[i] >= 0);
        }
        return lands;
    }

    /** The cells a run of turns actually moves, as "from here to there". */
    [[nodiscard]] std::vector<std::pair<int, int>> hops(
        const std::vector<Turn>& steps) const
    {
        std::vector<std::pair<int, int>> moved;
        const auto lands = landing(steps);
        for (std::size_t i = 0; i < lands.size(); ++i) {
            if (lands[i] != static_cast<int>(i)) {
                moved.emplace_back(static_cast<int>(i), lands[i]);
            }
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
    [[nodiscard]] Carries wanted(const CubeState& cube) const
    {
        Carries out{};
        for (const auto& cell : cells_) {
            const auto colour = sticker_at(cube, cell.at, cell.face);
            if (colour == home_colour(cell.face)) continue;
            for (const Face face : faces()) {
                if (home_colour(face) != colour) continue;
                out[face_index(cell.face)] |=
                    static_cast<std::uint8_t>(1U << face_index(face));
            }
        }
        return out;
    }

    /**
     * How many more pieces would be home after a setup and a tool.
     *
     * Only the cells the tool moves can change: a setup shuffles a face's
     * pieces among that face's own cells, and a cell's own colour is the same
     * all over one face.
     */
    [[nodiscard]] int gain(const CubeState& cube,
                           const std::vector<int>& setup_source,
                           const std::vector<std::pair<int, int>>& moved) const
    {
        int delta = 0;
        for (const auto& [from, to] : moved) {
            const auto source =
                static_cast<std::size_t>(setup_source[static_cast<std::size_t>(
                    from)]);
            const auto& was = cells_[source];
            const auto colour = sticker_at(cube, was.at, was.face);
            if (colour == home_colour(cells_[static_cast<std::size_t>(to)].face)) {
                ++delta;
            }
            if (colour == home_colour(was.face)) --delta;
        }
        return delta;
    }

private:
    [[nodiscard]] std::size_t slot_of(const CubiePosition& at) const noexcept
    {
        return static_cast<std::size_t>((at.x * size_ + at.y) * size_ + at.z);
    }

    int size_;
    std::vector<CentreCell> cells_;
    std::vector<int> index_;
};

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

void build_centres(Work& work)
{
    const int size = work.size();
    const CentreBoard board{size};
    if (board.cells().empty()) return;

    std::vector<Prepared> tools;
    for (const auto& tool : centre_tools(size)) {
        Prepared prepared;
        prepared.steps = written_out(tool);
        prepared.moved = board.hops(prepared.steps);
        prepared.carries = board.carries(prepared.moved);
        if (prepared.moved.empty()) continue;
        tools.push_back(std::move(prepared));
    }

    // For each setup, where the piece now sitting in a cell came from. That is
    // the inverse of what the setup did, which is what reading a tool's cells
    // needs: the tool says "the piece here goes there", and here is after the
    // setup.
    const auto before = setups();
    std::vector<std::vector<int>> sources;
    for (const auto& setup : before) {
        std::vector<int> source(board.cells().size(), 0);
        if (normalized_turns(setup.turns) == 0) {
            for (std::size_t i = 0; i < source.size(); ++i) {
                source[i] = static_cast<int>(i);
            }
        } else {
            const auto lands = board.landing({setup});
            for (std::size_t i = 0; i < lands.size(); ++i) {
                source[static_cast<std::size_t>(lands[i])] =
                    static_cast<int>(i);
            }
        }
        sources.push_back(std::move(source));
    }

    const auto target = static_cast<int>(board.cells().size());

    for (int guard = 0; centres_home(work.cube(), board.cells()) < target;
         ++guard) {
        assert(guard < target * 2 + 8);
        const auto wanted = board.wanted(work.cube());

        Choice choice;
        int best = 0;

        for (std::size_t i = 0; i < tools.size(); ++i) {
            if (!worth_trying(tools[i].carries, wanted)) continue;
            for (std::size_t s = 0; s < sources.size(); ++s) {
                const int delta = board.gain(work.cube(), sources[s],
                                             tools[i].moved);
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

}  // namespace

std::vector<CubeMove> solve_centres(const CubeState& state)
{
    Work work{state};
    build_centres(work);
    return work.take();
}

}  // namespace rubiks::cube::solver
