#pragma once

#include <cassert>
#include <utility>
#include <vector>

#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"
#include "cube/Cubie.hpp"
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

/**
 * A run of layers measured inwards from one face, turned as seen from it.
 *
 * Depth 1 is the face itself and depths grow inwards, exactly as the
 * application's own turn command counts them. A face turn is depth 1 to 1; an
 * inner slice is one depth further in; nothing here ever asks for a run,
 * because a solver that turns two layers at once has to put the second one
 * back.
 */
struct Turn {
    Face face;
    int first_depth;
    int last_depth;
    int turns;
};

[[nodiscard]] inline int normalized_turns(int quarter_turns) noexcept
{
    return ((quarter_turns % 4) + 4) % 4;
}

/** Whether a face is the one at the far end of its axis. */
[[nodiscard]] inline bool positive_face(Face face, int size) noexcept
{
    return outer_layer(face, size) == size - 1;
}

/** One outer face, turned as seen from outside it. */
[[nodiscard]] inline Turn face_turn(Face face, int turns) noexcept
{
    return Turn{face, 1, 1, turns};
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
    return Turn{from, depth, depth, turns};
}

/**
 * The move a turn is.
 *
 * Clockwise from outside a face at the near end of its axis is
 * counter-clockwise about that axis, which is the whole of the conversion --
 * the same one `moves::L`, `D` and `B` make.
 */
[[nodiscard]] inline CubeMove to_move(const Turn& turn, int size) noexcept
{
    return CubeMove{
        axis_of(turn.face),
        depth_layers(turn.face, turn.first_depth, turn.last_depth, size),
        positive_face(turn.face, size) ? turn.turns : -turn.turns};
}

/** The same turn, taken back. */
[[nodiscard]] inline Turn undone(const Turn& turn) noexcept
{
    return Turn{turn.face, turn.first_depth, turn.last_depth, -turn.turns};
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
    for (const Axis axis : {Axis::X, Axis::Y, Axis::Z}) {
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

[[nodiscard]] inline FaceColor sticker_at(const CubeState& cube,
                                          const CubiePosition& position,
                                          Face face) noexcept
{
    return cube.at(position.x, position.y, position.z).sticker(face);
}

/**
 * The colour a face wears on a solved cube.
 *
 * The domain says what solved means -- `is_solved()` holds every sticker
 * against this same function -- so this is the cube's own answer rather than a
 * guess about the order an enum happens to be written in. It is also the only
 * answer that exists at every size: a two by two has no centres, and the
 * centres of a bigger one are not yet one colour while it is being reduced.
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
    explicit Work(CubeState cube) : cube_(std::move(cube)) {}

    [[nodiscard]] const CubeState& cube() const noexcept { return cube_; }

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
    CubeState cube_;
    std::vector<CubeMove> moves_;
};

}  // namespace rubiks::cube::solver
