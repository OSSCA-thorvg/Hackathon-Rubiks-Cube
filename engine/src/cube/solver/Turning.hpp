#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <utility>
#include <vector>

#include "cube/CubeMove.hpp"
#include "cube/Cube.hpp"
#include "cube/Face.hpp"
#include "cube/Surface.hpp"

/**
 * What every solver in here turns a cube with.
 *
 * Internal to this target: a solver is asked for moves and hands back moves,
 * and this is the vocabulary it thinks in on the way. A `CubeMove` names an
 * axis and a mask, which is the right thing to leave with and the wrong thing
 * to reason in -- "the second layer in from the right, turned clockwise as I
 * look at it" is what a written sequence means, and turning that into a mask
 * is a conversion rather than a thought.
 */
namespace rubiks::cube::solver {

/** One rigid orientation: where each face lands and the turns producing it. */
struct FrameOrientation {
    std::array<Face, kFaceCount> destinations;
    std::vector<Axis> turns;
};

/** Enumerates the 24 proper orientations once using the domain's face turns. */
[[nodiscard]] inline const std::vector<FrameOrientation>& frame_orientations()
{
    static const std::vector<FrameOrientation> orientations =
        []() -> std::vector<FrameOrientation> {
            std::vector<FrameOrientation> made;
            made.reserve(24);
            made.push_back(FrameOrientation{faces(), {}});
            for (std::size_t head = 0; head < made.size(); ++head) {
                for (const Axis axis : axes()) {
                    FrameOrientation next = made[head];
                    for (Face& face : next.destinations) {
                        face = turned_face(axis, face);
                    }
                    const bool already = std::any_of(
                        made.begin(), made.end(),
                        [&next](const FrameOrientation& other) -> bool {
                            return other.destinations == next.destinations;
                        });
                    if (already) continue;
                    next.turns.push_back(axis);
                    made.push_back(std::move(next));
                }
            }
            assert(made.size() == 24);
            return made;
        }();
    return orientations;
}

/** A solver's canonical copy and the inverse mapping back to the input frame. */
class SolverFrame {
public:
    /** Normalizes an odd cube's six fixed centres without revalidating it. */
    explicit SolverFrame(Cube state) : cube_(std::move(state))
    {
        if (cube_.size() % 2 == 0) return;

        const int middle = cube_.size() / 2;
        for (const FrameOrientation& orientation : frame_orientations()) {
            bool canonical = true;
            for (const Face face : faces()) {
                const Face destination = orientation.destinations[face_index(face)];
                if (cube_.face(face).at(middle, middle) != solved_color(destination)) {
                    canonical = false;
                    break;
                }
            }
            if (!canonical) continue;

            destinations_ = orientation.destinations;
            const LayerMask all = layers_through(0, cube_.size() - 1);
            for (const Axis axis : orientation.turns) {
                cube_.apply(CubeMove{axis, all, 1});
            }
            return;
        }
        assert(false && "a reachable odd cube has a rigid fixed-centre frame");
    }

    /** Reads the canonical copy used by algorithms with fixed colour targets. */
    [[nodiscard]] const Cube& cube() const noexcept { return cube_; }

    /** Maps a solution back to the caller's axis, layers, and turn direction. */
    [[nodiscard]] std::vector<CubeMove> restore(std::vector<CubeMove> moves) const
    {
        constexpr std::array<Face, kAxisCount> positive{
            Face::Right, Face::Up, Face::Front};
        for (CubeMove& move : moves) {
            const Face destination = positive[static_cast<std::size_t>(move.axis)];
            Face source = destination;
            for (const Face face : faces()) {
                if (destinations_[face_index(face)] == destination) {
                    source = face;
                    break;
                }
            }

            if (outer_layer(source, cube_.size()) == 0) {
                LayerMask reversed = 0;
                for (int index = 0; index < cube_.size(); ++index) {
                    if ((move.layers & layer(index)) != 0) {
                        reversed |= layer(cube_.size() - 1 - index);
                    }
                }
                move.layers = reversed;
                move.quarter_turns = -move.quarter_turns;
            }
            move.axis = axis_of(source);
        }
        return moves;
    }

private:
    Cube cube_;
    std::array<Face, kFaceCount> destinations_ = faces();
};

/**
 * One layer measured inwards from one face, turned as seen from it.
 *
 * Depth 1 is the face itself and depths grow inwards, exactly as the
 * application's own turn command counts them. A face turn is depth 1 and an
 * inner slice is further in. A turn names one depth and never a run of them,
 * because a solver that turns two layers at once has to put the second one
 * back.
 */
struct Turn {
    Face face;
    int depth;
    int turns;
};

/** Whether a face is the one at the far end of its axis. */
[[nodiscard]] inline bool positive_face(Face face, int size) noexcept
{
    return outer_layer(face, size) == size - 1;
}

/** One outer face, turned as seen from outside it. */
[[nodiscard]] inline Turn face_turn(Face face, int turns) noexcept
{
    return Turn{face, 1, turns};
}

/**
 * One inner layer, named by its index along an axis rather than by a depth.
 *
 * The two ways of naming a layer meet here: everything below counts layers the
 * way the cube stores them, and a `Turn` counts depths the way a person reads
 * them, so exactly one place has to know that the two run opposite ways.
 */
[[nodiscard]] inline Turn layer_turn(Axis axis, int layer, int turns,
                                     int size) noexcept
{
    const Face from = axis == Axis::X   ? Face::Right
                      : axis == Axis::Y ? Face::Up
                                        : Face::Front;
    const int depth = size - layer;
    return Turn{from, depth, turns};
}

/**
 * The move a turn is.
 *
 * Clockwise from outside a face at the near end of its axis is
 * counter-clockwise about that axis, which is the whole of the conversion,
 * and the cube's own `face_move()` is where it is made.
 */
[[nodiscard]] inline CubeMove to_move(const Turn& turn, int size) noexcept
{
    return face_move(turn.face, turn.depth, turn.depth, turn.turns, size);
}

/** The same turn, taken back. */
[[nodiscard]] inline Turn undone(const Turn& turn) noexcept
{
    return Turn{turn.face, turn.depth, -turn.turns};
}

/** A face carried round by `quarters` turns of the up face. */
[[nodiscard]] inline Face about_up(Face face, int quarters) noexcept
{
    Face carried = face;
    for (int i = 0, n = normalized_turns(quarters); i < n; ++i) {
        carried = turned_face(Axis::Y, carried);
    }
    return carried;
}

/**
 * Which face a piece shows on after one face is turned.
 *
 * The two faces on the turning axis keep their stickers, so they answer with
 * themselves. Asking this rather than asking for a coordinate is what lets one
 * body of code serve every size: a cube of any size has the same six faces.
 */
[[nodiscard]] inline Face carried_by(Face turning, int turns, Face showing,
                                     int size) noexcept
{
    if (axis_of(showing) == axis_of(turning)) return showing;

    const int quarters =
        normalized_turns(positive_face(turning, size) ? turns : -turns);
    Face landed = showing;
    for (int i = 0; i < quarters; ++i) {
        landed = turned_face(axis_of(turning), landed);
    }
    return landed;
}

inline void set_axis(CubiePosition& position, Axis axis, int value) noexcept
{
    switch (axis) {
        case Axis::X:
            position.x = value;
            return;
        case Axis::Y:
            position.y = value;
            return;
        case Axis::Z:
            break;
    }
    position.z = value;
}

/** The axis that is neither of these two. */
[[nodiscard]] inline Axis remaining_axis(Axis a, Axis b) noexcept
{
    for (const Axis axis : axes()) {
        if (axis != a && axis != b) return axis;
    }
    assert(false);
    return Axis::X;
}

/** The one cubie that shows all three of these faces. */
[[nodiscard]] inline CubiePosition corner_slot(Face a, Face b, Face c,
                                               int size) noexcept
{
    CubiePosition position{0, 0, 0};
    for (const Face face : {a, b, c}) {
        set_axis(position, axis_of(face), outer_layer(face, size));
    }
    return position;
}

/**
 * One cubie of the run that shows both of these faces.
 *
 * A three by three has exactly one; a bigger cube has a row of them along the
 * edge, all of the same two colours once the cube has been reduced. Any of
 * them answers for the row, so the first is taken and the choice is written
 * down here rather than at each caller.
 */
[[nodiscard]] inline CubiePosition edge_slot(Face a, Face b, int size) noexcept
{
    CubiePosition position{1, 1, 1};
    set_axis(position, axis_of(a), outer_layer(a, size));
    set_axis(position, axis_of(b), outer_layer(b, size));
    return position;
}

/**
 * Every cubie of that run, from one end of the edge to the other.
 *
 * What `edge_slot` takes the first of. A reduction has to look at all of them
 * -- a row is paired or it is not, and one wing cannot say which -- while the
 * stages that follow a reduction need only ask one.
 *
 * Empty for a two by two, which has no edge pieces at all.
 */
[[nodiscard]] inline std::vector<CubiePosition> edge_row(Face a, Face b,
                                                         int size)
{
    const Axis along = remaining_axis(axis_of(a), axis_of(b));

    std::vector<CubiePosition> row;
    for (int i = 1; i <= size - 2; ++i) {
        auto position = edge_slot(a, b, size);
        set_axis(position, along, i);
        row.push_back(position);
    }
    return row;
}

/** The faces a slot shows: three for a corner, two for an edge, one centre. */
[[nodiscard]] inline std::vector<Face> exposed_faces(
    const CubiePosition& position, int size)
{
    std::vector<Face> showing;
    for (const Face face : faces()) {
        if (coordinate_on(axis_of(face), position) == outer_layer(face, size)) {
            showing.push_back(face);
        }
    }
    return showing;
}

[[nodiscard]] inline FaceColor sticker_at(const Cube& cube,
                                          const CubiePosition& position,
                                          Face face) noexcept
{
    return cube.at(position.x, position.y, position.z).sticker(face);
}

/**
 * The target colour of a face in the solver's canonical frame.
 *
 * Cube::is_solved() also accepts other rigid orientations. Algorithms use
 * this fixed target after SolverFrame has normalized an odd cube's centres.
 */
[[nodiscard]] inline FaceColor home_colour(Face face) noexcept
{
    return solved_color(face);
}

/**
 * A cube being solved, and the moves that got it here.
 *
 * A solver turns its own copy as it writes, so the sequence it returns and the
 * sequence it reasoned about are the same one by construction. What is left to
 * get wrong is a stage that never finishes, and nothing else.
 */
class Work {
public:
    explicit Work(Cube cube) : cube_(std::move(cube)) {}

    [[nodiscard]] const Cube& cube() const noexcept { return cube_; }

    [[nodiscard]] int size() const noexcept { return cube_.size(); }

    [[nodiscard]] std::vector<CubeMove> take() noexcept
    {
        return std::move(moves_);
    }

    void apply(const Turn& step)
    {
        if (normalized_turns(step.turns) == 0) return;
        const auto move = to_move(step, cube_.size());
        assert(move.layers != 0);
        cube_.apply(move);
        moves_.push_back(move);
    }

    void turn(Face face, int turns) { apply(face_turn(face, turns)); }

private:
    Cube cube_;
    std::vector<CubeMove> moves_;
};

}  // namespace rubiks::cube::solver
