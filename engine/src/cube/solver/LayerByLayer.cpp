#include "cube/solver/LayerByLayer.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

#include "cube/Cubie.hpp"
#include "cube/Surface.hpp"

namespace rubiks::cube::solver {
namespace {

constexpr int kSize = 3;
constexpr int kLast = kSize - 1;

/**
 * The four faces around the up-down axis, in the order a U turn carries them.
 *
 * Read off `turned_face(Axis::Y, ...)` rather than written from memory, which
 * is what makes "the frame rotated by one" below the same rotation the cube
 * itself performs.
 */
constexpr std::array<Face, 4> kSides{Face::Front, Face::Left, Face::Back,
                                     Face::Right};

/**
 * One quarter-turn count of one outer face, as seen from outside that face.
 *
 * The form the written sequences are in. A `CubeMove` is the general thing --
 * any run of layers about any axis -- and a solver that writes in the general
 * form would be writing a layer mask where it means "R". The narrowing is also
 * the promise this solver makes to the rest of the application: what comes out
 * is outer faces and nothing else, which is what a shared link can carry and a
 * move log can write down.
 */
struct FaceTurn {
    Face face;
    int turns;
};

[[nodiscard]] int normalized_turns(int quarter_turns) noexcept
{
    return ((quarter_turns % 4) + 4) % 4;
}

/** Whether a face is the one at the far end of its axis. */
[[nodiscard]] bool positive_face(Face face) noexcept
{
    return outer_layer(face, kSize) == kLast;
}

/**
 * The move a face turn is.
 *
 * Clockwise from outside a face at the near end of its axis is
 * counter-clockwise about that axis, which is the whole of the conversion --
 * the same one `moves::L`, `D` and `B` make.
 */
[[nodiscard]] CubeMove to_move(const FaceTurn& turn) noexcept
{
    return CubeMove{axis_of(turn.face), depth_layers(turn.face, 1, 1, kSize),
                    positive_face(turn.face) ? turn.turns : -turn.turns};
}

/** A face carried round by `quarters` turns of the up face. */
[[nodiscard]] Face about_up(Face face, int quarters) noexcept
{
    Face carried = face;
    for (int i = 0, n = normalized_turns(quarters); i < n; ++i) {
        carried = turned_face(Axis::Y, carried);
    }
    return carried;
}

/** Where a cubie lands after one face turn. */
[[nodiscard]] CubiePosition moved(const CubiePosition& position,
                                  const FaceTurn& turn) noexcept
{
    const auto move = to_move(turn);
    CubiePosition landed = position;
    for (int i = 0, n = normalized_turns(move.quarter_turns); i < n; ++i) {
        landed = turned_position(move.axis, landed, kSize);
    }
    return landed;
}

void set_axis(CubiePosition& position, Axis axis, int value) noexcept
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

/** The slot at which every one of `on` is the outer surface. */
[[nodiscard]] CubiePosition slot_of(std::initializer_list<Face> on) noexcept
{
    CubiePosition position{1, 1, 1};
    for (const Face face : on) {
        set_axis(position, axis_of(face), outer_layer(face, kSize));
    }
    return position;
}

/** The faces a slot shows: three for a corner, two for an edge, one centre. */
[[nodiscard]] std::vector<Face> exposed_faces(const CubiePosition& position)
{
    std::vector<Face> showing;
    for (const Face face : faces()) {
        if (coordinate_on(axis_of(face), position) ==
            outer_layer(face, kSize)) {
            showing.push_back(face);
        }
    }
    return showing;
}

[[nodiscard]] FaceColor sticker_at(const CubeState& cube,
                                   const CubiePosition& position,
                                   Face face) noexcept
{
    return cube.at(position.x, position.y, position.z).sticker(face);
}

/**
 * The colour a face wears on this cube, read off its centre.
 *
 * Not `solved_color()`, which says what the colour is in the enum's own order.
 * A solver has no business knowing that order: what it needs is which face a
 * colour belongs to, and the centre of a 3x3 is the cube's own answer to that.
 */
[[nodiscard]] FaceColor centre_colour(const CubeState& cube, Face face) noexcept
{
    const auto centre = slot_of({face});
    return sticker_at(cube, centre, face);
}

[[nodiscard]] Face face_of_colour(const CubeState& cube,
                                  FaceColor colour) noexcept
{
    for (const Face face : faces()) {
        if (centre_colour(cube, face) == colour) return face;
    }
    assert(false);
    return Face::Up;
}

/** Every slot of the cube, once each. */
[[nodiscard]] std::vector<CubiePosition> all_slots()
{
    std::vector<CubiePosition> slots;
    slots.reserve(static_cast<std::size_t>(kSize) * kSize * kSize);
    for (int x = 0; x < kSize; ++x) {
        for (int y = 0; y < kSize; ++y) {
            for (int z = 0; z < kSize; ++z) {
                slots.push_back(CubiePosition{x, y, z});
            }
        }
    }
    return slots;
}

/** Whether a slot's showing colours are exactly `wanted`, in any order. */
[[nodiscard]] bool shows_colours(const CubeState& cube,
                                 const CubiePosition& position,
                                 const std::vector<FaceColor>& wanted)
{
    const auto showing = exposed_faces(position);
    if (showing.size() != wanted.size()) return false;

    std::vector<bool> taken(wanted.size(), false);
    for (const Face face : showing) {
        const auto colour = sticker_at(cube, position, face);
        bool matched = false;
        for (std::size_t i = 0; i < wanted.size(); ++i) {
            if (taken[i] || wanted[i] != colour) continue;
            taken[i] = true;
            matched = true;
            break;
        }
        if (!matched) return false;
    }
    return true;
}

/**
 * Where the piece wearing exactly these colours is.
 *
 * The only place a colour is used for anything but naming a face: on a 3x3 a
 * set of two or three colours picks out one piece and no other. Only the
 * showing faces are read -- a cubie carries a colour on the faces it hides as
 * well, and those turn with it, so looking at all six would find the same
 * colours on several pieces at once.
 */
[[nodiscard]] CubiePosition find_piece(const CubeState& cube,
                                       const std::vector<FaceColor>& colours)
{
    for (const auto& slot : all_slots()) {
        if (shows_colours(cube, slot, colours)) return slot;
    }
    assert(false);
    return CubiePosition{1, 1, 1};
}

/** Which of a piece's showing faces wears `colour`. */
[[nodiscard]] Face face_showing(const CubeState& cube,
                                const CubiePosition& position,
                                FaceColor colour)
{
    for (const Face face : exposed_faces(position)) {
        if (sticker_at(cube, position, face) == colour) return face;
    }
    assert(false);
    return Face::Up;
}

/** Whether every showing sticker of a slot is the colour of its own face. */
[[nodiscard]] bool piece_home(const CubeState& cube,
                              const CubiePosition& position)
{
    for (const Face face : exposed_faces(position)) {
        if (sticker_at(cube, position, face) != centre_colour(cube, face)) {
            return false;
        }
    }
    return true;
}

/** Whether a slot holds its own piece, however that piece is turned. */
[[nodiscard]] bool piece_in_place(const CubeState& cube,
                                  const CubiePosition& position)
{
    std::vector<FaceColor> wanted;
    for (const Face face : exposed_faces(position)) {
        wanted.push_back(centre_colour(cube, face));
    }
    return shows_colours(cube, position, wanted);
}

/** How many turns of the up face carry `from` to `to`; 0 when they are one. */
[[nodiscard]] int up_turns_between(const CubiePosition& from,
                                   const CubiePosition& to) noexcept
{
    CubiePosition carried = from;
    for (int turns = 0; turns < 4; ++turns) {
        if (carried.x == to.x && carried.y == to.y && carried.z == to.z) {
            return turns;
        }
        carried = moved(carried, FaceTurn{Face::Up, 1});
    }
    assert(false);
    return 0;
}

/** How many turns of the up face carry face `from` to face `to`. */
[[nodiscard]] int up_turns_between(Face from, Face to) noexcept
{
    for (int turns = 0; turns < 4; ++turns) {
        if (about_up(from, turns) == to) return turns;
    }
    assert(false);
    return 0;
}

/**
 * A cube being solved, and the moves that got it here.
 *
 * The solver turns its own copy as it writes, so the sequence it returns and
 * the sequence it reasoned about are the same one by construction. What is
 * left to get wrong is a stage that never finishes, and nothing else.
 */
class Work {
public:
    explicit Work(CubeState cube) : cube_(std::move(cube)) {}

    [[nodiscard]] const CubeState& cube() const noexcept { return cube_; }

    [[nodiscard]] std::vector<CubeMove> take() noexcept
    {
        return std::move(moves_);
    }

    void turn(Face face, int turns)
    {
        if (normalized_turns(turns) == 0) return;
        const auto move = to_move(FaceTurn{face, turns});
        cube_.apply(move);
        moves_.push_back(move);
    }

    /** Runs a written sequence with every face carried round by `frame`. */
    void run(const std::vector<FaceTurn>& sequence, int frame)
    {
        for (const auto& step : sequence) {
            turn(about_up(step.face, frame), step.turns);
        }
    }

private:
    CubeState cube_;
    std::vector<CubeMove> moves_;
};

// The written sequences. Six of them, and everything else in this file is
// working out where to stand before using one.

/**
 * Lifts the bottom front-right corner back out onto the top.
 *
 * Also the shape all three insertions below are built around: a corner leaves
 * its slot and comes back turned differently, and every one of the bottom
 * layer's other seven pieces is where it was afterwards.
 */
const std::vector<FaceTurn> kCornerLift{{Face::Right, 1},
                                        {Face::Up, 1},
                                        {Face::Right, -1},
                                        {Face::Up, -1}};

/**
 * The three ways a corner waiting above its slot goes into it.
 *
 * Which one to use is read off the corner rather than searched for: the bottom
 * colour is on the right face, on the front face, or facing up, and there is
 * no fourth place for it to be. The first two are the same three moves
 * mirrored; the third is longer because a corner facing up has to be tipped
 * over on the way down.
 */
const std::vector<FaceTurn> kCornerFromRight{
    {Face::Right, 1}, {Face::Up, 1}, {Face::Right, -1}};

const std::vector<FaceTurn> kCornerFromFront{
    {Face::Front, -1}, {Face::Up, -1}, {Face::Front, 1}};

const std::vector<FaceTurn> kCornerFromTop{{Face::Right, 1},
                                           {Face::Front, 1},
                                           {Face::Right, 2},
                                           {Face::Front, -1},
                                           {Face::Right, -1}};

/** Drops a top edge into the middle slot on the right of the front face. */
const std::vector<FaceTurn> kInsertRight{
    {Face::Up, 1},    {Face::Right, 1}, {Face::Up, -1},   {Face::Right, -1},
    {Face::Up, -1},   {Face::Front, -1}, {Face::Up, 1},   {Face::Front, 1}};

/** The mirror of it, for the slot on the left. */
const std::vector<FaceTurn> kInsertLeft{
    {Face::Up, -1},  {Face::Left, -1}, {Face::Up, 1},   {Face::Left, 1},
    {Face::Up, 1},   {Face::Front, 1}, {Face::Up, -1},  {Face::Front, -1}};

/** Turns top edges the right way up, a dot to a line to a cross. */
const std::vector<FaceTurn> kTopCross{{Face::Front, 1},  {Face::Right, 1},
                                      {Face::Up, 1},     {Face::Right, -1},
                                      {Face::Up, -1},    {Face::Front, -1}};

/** Moves three top corners round, leaving how they are turned to the next. */
const std::vector<FaceTurn> kCornerCycle{
    {Face::Up, 1},    {Face::Right, 1},  {Face::Up, -1}, {Face::Left, -1},
    {Face::Up, 1},    {Face::Right, -1}, {Face::Up, -1}, {Face::Left, 1}};

/**
 * Turns the front-right top corner in place, over two rounds of four.
 *
 * One round leaves the corner in the bottom layer; two brings it back where it
 * was, turned by a third. So this is only ever used in pairs, which is why the
 * loop that uses it counts pairs rather than moves.
 */
const std::vector<FaceTurn> kCornerTwist{{Face::Right, -1},
                                         {Face::Down, -1},
                                         {Face::Right, 1},
                                         {Face::Down, 1}};

/** Moves three top edges round, leaving everything else where it is. */
const std::vector<FaceTurn> kEdgeCycle{
    {Face::Right, 1},  {Face::Up, -1}, {Face::Right, 1},  {Face::Up, 1},
    {Face::Right, 1},  {Face::Up, 1},  {Face::Right, 1},  {Face::Up, -1},
    {Face::Right, -1}, {Face::Up, -1}, {Face::Right, 2}};

/** The four top edge slots. */
[[nodiscard]] std::vector<CubiePosition> top_edges()
{
    std::vector<CubiePosition> slots;
    for (const Face side : kSides) slots.push_back(slot_of({Face::Up, side}));
    return slots;
}

/** The four top corner slots. */
[[nodiscard]] std::vector<CubiePosition> top_corners()
{
    std::vector<CubiePosition> slots;
    for (int frame = 0; frame < 4; ++frame) {
        slots.push_back(slot_of({Face::Up, about_up(Face::Front, frame),
                                 about_up(Face::Right, frame)}));
    }
    return slots;
}

[[nodiscard]] int top_edges_turned_up(const CubeState& cube)
{
    const auto up = centre_colour(cube, Face::Up);
    int count = 0;
    for (const auto& slot : top_edges()) {
        if (sticker_at(cube, slot, Face::Up) == up) ++count;
    }
    return count;
}

[[nodiscard]] int top_corners_in_place(const CubeState& cube)
{
    int count = 0;
    for (const auto& slot : top_corners()) {
        if (piece_in_place(cube, slot)) ++count;
    }
    return count;
}

[[nodiscard]] int top_edges_in_place(const CubeState& cube)
{
    int count = 0;
    for (const auto& slot : top_edges()) {
        if (piece_in_place(cube, slot)) ++count;
    }
    return count;
}

/**
 * All eight top pieces at once, which is what the last stage has to count.
 *
 * Counting only the edges there would let the top be turned as a whole: four
 * edges can be brought home by a sequence that leaves the corners a quarter
 * out, and a score blind to the corners would call that an improvement.
 */
[[nodiscard]] int top_layer_in_place(const CubeState& cube)
{
    return top_corners_in_place(cube) + top_edges_in_place(cube);
}

/** The sequence that takes another one back. */
[[nodiscard]] std::vector<FaceTurn> reversed(const std::vector<FaceTurn>& sequence)
{
    std::vector<FaceTurn> back;
    back.reserve(sequence.size());
    for (auto step = sequence.rbegin(); step != sequence.rend(); ++step) {
        back.push_back(FaceTurn{step->face, -step->turns});
    }
    return back;
}

/**
 * Uses one written sequence from wherever it does the most good.
 *
 * The last four stages each have exactly one sequence, and what a person does
 * with it is turn the cube until the case matches the picture they learned.
 * The cases are told apart here by trying the four ways round -- and the four
 * alignments of the top -- and keeping the one that leaves the most pieces
 * right. It is the same knowledge as a table of pictures, held once instead of
 * twice: the sequences are already written down, so a table would be a second
 * place for the same fact to be wrong, and this one cannot disagree with the
 * cube because it asks the cube.
 *
 * @return whether anything was applied, which is false only when no way round
 *         improves on standing still.
 */
bool apply_best(Work& work, const std::vector<FaceTurn>& sequence,
                int (*score)(const CubeState&))
{
    // Backwards as well as forwards. A sequence that moves three pieces round
    // moves them the other way when it is taken back, and the four ways round
    // of one direction reach only half the cycles there are -- which is why a
    // person learns these in pairs.
    const std::array<std::vector<FaceTurn>, 2> both{sequence,
                                                    reversed(sequence)};

    int best = score(work.cube());
    int best_align = 0;
    int best_frame = -1;
    std::size_t best_way = 0;

    for (std::size_t way = 0; way < both.size(); ++way) {
        for (int align = 0; align < 4; ++align) {
            for (int frame = 0; frame < 4; ++frame) {
                Work trial{work.cube()};
                trial.turn(Face::Up, align);
                trial.run(both[way], frame);

                const int reached = score(trial.cube());
                if (reached <= best) continue;
                best = reached;
                best_align = align;
                best_frame = frame;
                best_way = way;
            }
        }
    }

    if (best_frame < 0) return false;
    work.turn(Face::Up, best_align);
    work.run(both[best_way], best_frame);
    return true;
}

/**
 * Turns the top so the slot above `face` holds nothing already finished.
 *
 * The bottom cross is built in the top layer first, and every way of bringing
 * the next edge up turns one side face, which sends whatever is above that
 * face down with it. There is always a slot to spare: the piece being fetched
 * is not up there yet, so at most three of the four are.
 */
void free_slot_above(Work& work, Face face, FaceColor bottom)
{
    for (int align = 0; align < 4; ++align) {
        const auto slot = slot_of({Face::Up, face});
        if (sticker_at(work.cube(), slot, Face::Up) != bottom) return;
        work.turn(Face::Up, 1);
    }
    assert(false);
}

/**
 * Every bottom-colour edge into the top layer, bottom colour facing up.
 *
 * The flower a beginner is shown, and the reason for it is that the bottom is
 * empty while it is being built: nothing down there can be broken by a side
 * face turning, so each piece is fetched in one move and no piece has to be
 * put back afterwards.
 */
void build_flower(Work& work)
{
    const auto bottom = centre_colour(work.cube(), Face::Down);

    for (const Face side : kSides) {
        const auto side_colour = centre_colour(work.cube(), side);

        for (int guard = 0;; ++guard) {
            assert(guard < 8);
            const auto at = find_piece(work.cube(), {bottom, side_colour});

            if (at.y == kLast) {
                if (sticker_at(work.cube(), at, Face::Up) == bottom) break;

                // Turned the wrong way up there. One turn of the face it
                // leans against drops it into the middle, which is the case
                // below and the only one that knows how to turn it over.
                for (const Face face : exposed_faces(at)) {
                    if (face == Face::Up) continue;
                    work.turn(face, 1);
                    break;
                }
                continue;
            }

            if (at.y == 0) {
                Face side_face = Face::Down;
                for (const Face face : exposed_faces(at)) {
                    if (face != Face::Down) side_face = face;
                }
                free_slot_above(work, side_face, bottom);
                work.turn(side_face,
                          sticker_at(work.cube(), at, Face::Down) == bottom
                              ? 2
                              : 1);
                continue;
            }

            // In the middle. Lifting it with the face its bottom colour is
            // *not* on is what brings that colour out facing up: a quarter
            // turn carries the sticker from the side face to the top one.
            const auto carrying = face_showing(work.cube(), at, bottom);
            Face lift = Face::Up;
            for (const Face face : exposed_faces(at)) {
                if (face != carrying) lift = face;
            }
            free_slot_above(work, lift, bottom);
            work.turn(lift, moved(at, FaceTurn{lift, 1}).y == kLast ? 1 : -1);
        }
    }
}

/** The flower folded down into the bottom cross, one half turn each. */
void fold_flower(Work& work)
{
    const auto bottom = centre_colour(work.cube(), Face::Down);

    for (const Face side : kSides) {
        const auto side_colour = centre_colour(work.cube(), side);
        const auto at = find_piece(work.cube(), {bottom, side_colour});
        work.turn(Face::Up,
                  up_turns_between(at, slot_of({Face::Up, side})));
        work.turn(side, 2);
    }
}

void place_bottom_corners(Work& work)
{
    const auto bottom = centre_colour(work.cube(), Face::Down);

    for (int frame = 0; frame < 4; ++frame) {
        const auto front = about_up(Face::Front, frame);
        const auto right = about_up(Face::Right, frame);
        const auto home = slot_of({Face::Down, front, right});
        const std::vector<FaceColor> colours{
            bottom, centre_colour(work.cube(), front),
            centre_colour(work.cube(), right)};

        for (int guard = 0;; ++guard) {
            assert(guard < 32);
            const auto at = find_piece(work.cube(), colours);
            if (piece_home(work.cube(), at) &&
                at.x == home.x && at.y == home.y && at.z == home.z) {
                break;
            }

            if (at.y == 0) {
                // Down there but not right. Lifting it out from where it sits
                // leaves the rest of the bottom layer alone, so the corners
                // already finished stay finished.
                for (int other = 0; other < 4; ++other) {
                    const auto slot =
                        slot_of({Face::Down, about_up(Face::Front, other),
                                 about_up(Face::Right, other)});
                    if (slot.x != at.x || slot.y != at.y || slot.z != at.z) {
                        continue;
                    }
                    work.run(kCornerLift, other);
                    break;
                }
                continue;
            }

            // Above its slot, the bottom colour points one of three ways, and
            // each has its own way down.
            work.turn(Face::Up,
                      up_turns_between(
                          at, slot_of({Face::Up, front, right})));

            const auto waiting = slot_of({Face::Up, front, right});
            const auto leaning = face_showing(work.cube(), waiting, bottom);
            if (leaning == right) {
                work.run(kCornerFromRight, frame);
            } else if (leaning == front) {
                work.run(kCornerFromFront, frame);
            } else {
                work.run(kCornerFromTop, frame);
            }
        }
    }
}

void place_middle_edges(Work& work)
{
    for (int frame = 0; frame < 4; ++frame) {
        const auto front = about_up(Face::Front, frame);
        const auto right = about_up(Face::Right, frame);
        const auto home = slot_of({front, right});
        const std::vector<FaceColor> colours{centre_colour(work.cube(), front),
                                             centre_colour(work.cube(), right)};

        for (int guard = 0;; ++guard) {
            assert(guard < 8);
            const auto at = find_piece(work.cube(), colours);
            if (piece_home(work.cube(), at) &&
                at.x == home.x && at.y == home.y && at.z == home.z) {
                break;
            }

            if (at.y != kLast) {
                // Stuck in a middle slot, its own or another's. The insertion
                // for that slot, used on an occupied one, puts the occupant
                // back on top -- so there is no second sequence for getting
                // one out.
                for (int other = 0; other < 4; ++other) {
                    const auto slot = slot_of({about_up(Face::Front, other),
                                               about_up(Face::Right, other)});
                    if (slot.x != at.x || slot.y != at.y || slot.z != at.z) {
                        continue;
                    }
                    work.run(kInsertRight, other);
                    break;
                }
                continue;
            }

            // On top. Line its side colour up with that colour's own face;
            // then the colour facing up names the slot it belongs in, and
            // which side of the front face that is chooses the sequence.
            Face leaning = Face::Up;
            for (const Face face : exposed_faces(at)) {
                if (face != Face::Up) leaning = face;
            }
            const auto side_colour = sticker_at(work.cube(), at, leaning);
            const auto matching = face_of_colour(work.cube(), side_colour);
            work.turn(Face::Up, up_turns_between(leaning, matching));

            const auto now = slot_of({Face::Up, matching});
            const auto up_colour = sticker_at(work.cube(), now, Face::Up);
            const auto belongs = face_of_colour(work.cube(), up_colour);
            const int here = up_turns_between(Face::Front, matching);

            if (about_up(Face::Right, here) == belongs) {
                work.run(kInsertRight, here);
            } else {
                work.run(kInsertLeft, here);
            }
        }
    }
}

/**
 * Uses one sequence over and over until a stage's count is full.
 *
 * The whole of the last four stages, which differ only in the sequence they
 * are written around and in what they are counting. Rounds that improve
 * nothing are still applied: from a top with not one piece in its place there
 * is sometimes no single use of a three-piece cycle that puts one there, and
 * moving them anyway is what lets the next round find its footing. Only the
 * corner stage has ever needed that; the edge stage reaches its target from
 * every arrangement in one or two improving rounds.
 */
void repeat_until(Work& work, const std::vector<FaceTurn>& sequence,
                  int (*score)(const CubeState&), int target)
{
    for (int guard = 0; score(work.cube()) < target; ++guard) {
        assert(guard < 4);
        if (apply_best(work, sequence, score)) continue;
        work.run(sequence, 0);
    }
}

void turn_top_edges_up(Work& work)
{
    repeat_until(work, kTopCross, top_edges_turned_up, 4);
}

void place_top_corners(Work& work)
{
    repeat_until(work, kCornerCycle, top_corners_in_place, 4);
}

void turn_top_corners_up(Work& work)
{
    const auto up = centre_colour(work.cube(), Face::Up);
    const auto corner = slot_of({Face::Up, Face::Front, Face::Right});

    // One trip round the top, a corner at a time, and four single turns of the
    // top add up to none -- so the layer is where it started when this ends,
    // and so is the bottom, which each pair of rounds puts back.
    for (int visited = 0; visited < 4; ++visited) {
        for (int pairs = 0;
             sticker_at(work.cube(), corner, Face::Up) != up; ++pairs) {
            assert(pairs < 3);
            work.run(kCornerTwist, 0);
            work.run(kCornerTwist, 0);
        }
        work.turn(Face::Up, 1);
    }
}

void place_top_edges(Work& work)
{
    repeat_until(work, kEdgeCycle, top_layer_in_place, 8);
}

/**
 * What a stage promises to have made true when it returns.
 *
 * Read only by the debug assertions in `solve()`. A stage that ends one piece
 * short leaves a cube the next stage was not written for, and without these
 * the first sign of it is an unsolved cube at the very end with no clue as to
 * which stage let it through.
 */
[[nodiscard]] bool flower_built(const CubeState& cube)
{
    const auto bottom = centre_colour(cube, Face::Down);
    for (const Face side : kSides) {
        const auto at = find_piece(cube, {bottom, centre_colour(cube, side)});
        if (at.y != kLast) return false;
        if (sticker_at(cube, at, Face::Up) != bottom) return false;
    }
    return true;
}

[[nodiscard]] bool bottom_cross_built(const CubeState& cube)
{
    for (const Face side : kSides) {
        if (!piece_home(cube, slot_of({Face::Down, side}))) return false;
    }
    return true;
}

[[nodiscard]] bool bottom_layer_built(const CubeState& cube)
{
    if (!bottom_cross_built(cube)) return false;
    for (int frame = 0; frame < 4; ++frame) {
        if (!piece_home(cube, slot_of({Face::Down, about_up(Face::Front, frame),
                                       about_up(Face::Right, frame)}))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool middle_layer_built(const CubeState& cube)
{
    if (!bottom_layer_built(cube)) return false;
    for (int frame = 0; frame < 4; ++frame) {
        if (!piece_home(cube, slot_of({about_up(Face::Front, frame),
                                       about_up(Face::Right, frame)}))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool top_corners_turned_up(const CubeState& cube)
{
    const auto up = centre_colour(cube, Face::Up);
    for (const auto& slot : top_corners()) {
        if (sticker_at(cube, slot, Face::Up) != up) return false;
    }
    return true;
}

}  // namespace

bool LayerByLayer::supports(int size) const noexcept { return size == kSize; }

std::vector<CubeMove> LayerByLayer::solve(const CubeState& state) const
{
    assert(supports(state.size()));
    if (state.is_solved()) return {};

    Work work{state};

    build_flower(work);
    assert(flower_built(work.cube()));

    fold_flower(work);
    assert(bottom_cross_built(work.cube()));

    place_bottom_corners(work);
    assert(bottom_layer_built(work.cube()));

    place_middle_edges(work);
    assert(middle_layer_built(work.cube()));

    turn_top_edges_up(work);
    assert(middle_layer_built(work.cube()));
    assert(top_edges_turned_up(work.cube()) == 4);

    place_top_corners(work);
    assert(top_corners_in_place(work.cube()) == 4);

    turn_top_corners_up(work);
    assert(top_corners_turned_up(work.cube()));
    assert(middle_layer_built(work.cube()));
    assert(top_corners_in_place(work.cube()) == 4);
    assert(top_edges_turned_up(work.cube()) == 4);

    place_top_edges(work);
    assert(middle_layer_built(work.cube()));
    assert(top_corners_in_place(work.cube()) == 4);
    assert(top_corners_turned_up(work.cube()));
    assert(top_edges_turned_up(work.cube()) == 4);
    assert(work.cube().is_solved());
    return compress(work.take());
}

std::vector<CubeMove> compress(const std::vector<CubeMove>& moves)
{
    std::vector<CubeMove> joined;

    for (const auto& move : moves) {
        int turns = normalized_turns(move.quarter_turns);

        // Fold this turn into the run of the same layers before it, and keep
        // folding: cancelling one pair can bring two more together.
        while (!joined.empty() && joined.back().axis == move.axis &&
               joined.back().layers == move.layers) {
            turns = normalized_turns(turns + joined.back().quarter_turns);
            joined.pop_back();
            if (turns != 0) break;
        }

        // Three quarters one way is one the other, which is how a reader would
        // write it and one fewer quarter for the cube to turn through.
        if (turns == 3) turns = -1;
        if (turns != 0) joined.push_back(CubeMove{move.axis, move.layers, turns});
    }

    return joined;
}

}  // namespace rubiks::cube::solver
