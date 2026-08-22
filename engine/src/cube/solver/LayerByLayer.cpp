#include "cube/solver/LayerByLayer.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

#include "cube/Cubie.hpp"
#include "cube/Surface.hpp"
#include "cube/solver/Projection.hpp"
#include "cube/solver/Turning.hpp"

namespace rubiks::cube::solver {
namespace {

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
 * the promise this file makes to the rest of the application: what comes out
 * is outer faces and nothing else, which is what a shared link can carry and a
 * move log can write down.
 */
struct FaceTurn {
    Face face;
    int turns;
};

[[nodiscard]] Face face_wearing(FaceColor colour) noexcept
{
    for (const Face face : faces()) {
        if (home_colour(face) == colour) return face;
    }
    assert(false);
    return Face::Up;
}

/**
 * Where a piece is, said in faces rather than in coordinates.
 *
 * The stages never ask "which cubie is this" -- they ask which faces a piece
 * shows and what colour is on each, and those two answers are the same shape
 * on a two by two and on a nine by nine.
 */
struct Placement {
    std::vector<Face> faces;
    std::vector<FaceColor> colours;
    CubiePosition at{};
};

[[nodiscard]] bool shows(const Placement& piece, Face face) noexcept
{
    for (const Face on : piece.faces) {
        if (on == face) return true;
    }
    return false;
}

[[nodiscard]] FaceColor colour_on(const Placement& piece, Face face) noexcept
{
    for (std::size_t i = 0; i < piece.faces.size(); ++i) {
        if (piece.faces[i] == face) return piece.colours[i];
    }
    assert(false);
    return FaceColor::Red;
}

/** The face this piece wears `colour` on. */
[[nodiscard]] Face wearing(const Placement& piece, FaceColor colour) noexcept
{
    for (std::size_t i = 0; i < piece.faces.size(); ++i) {
        if (piece.colours[i] == colour) return piece.faces[i];
    }
    assert(false);
    return Face::Up;
}

/** The faces of a piece other than `apart`. */
[[nodiscard]] std::vector<Face> besides(const Placement& piece, Face apart)
{
    std::vector<Face> rest;
    for (const Face face : piece.faces) {
        if (face != apart) rest.push_back(face);
    }
    return rest;
}

/** Whether every showing sticker of a piece is the colour of its own face. */
[[nodiscard]] bool at_home(const Placement& piece) noexcept
{
    for (std::size_t i = 0; i < piece.faces.size(); ++i) {
        if (piece.colours[i] != home_colour(piece.faces[i])) return false;
    }
    return true;
}

[[nodiscard]] Placement read(const CubeState& cube,
                             const CubiePosition& position)
{
    Placement piece;
    piece.at = position;
    piece.faces = exposed_faces(position, cube.size());
    for (const Face face : piece.faces) {
        piece.colours.push_back(sticker_at(cube, position, face));
    }
    return piece;
}

/** Whether a piece's colours are exactly `wanted`, in any order. */
[[nodiscard]] bool wears(const Placement& piece,
                         const std::vector<FaceColor>& wanted)
{
    if (piece.colours.size() != wanted.size()) return false;

    std::vector<bool> taken(wanted.size(), false);
    for (const auto colour : piece.colours) {
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
 * The only place a colour is used for anything but naming a face: a set of two
 * or three colours picks out one piece of a three by three and no other. On a
 * bigger cube a pair of colours picks out a whole row along one edge, and any
 * one of that row answers for it -- which is true once the cube is reduced,
 * and this is only ever asked after it has been.
 *
 * Only the showing faces are read. A cubie carries a colour on the faces it
 * hides as well, and those turn with it, so looking at all six would find the
 * same colours on several pieces at once.
 */
[[nodiscard]] Placement find_piece(const CubeState& cube,
                                   const std::vector<FaceColor>& colours)
{
    const int size = cube.size();
    for (int x = 0; x < size; ++x) {
        for (int y = 0; y < size; ++y) {
            for (int z = 0; z < size; ++z) {
                const auto piece = read(cube, CubiePosition{x, y, z});
                if (piece.faces.size() != colours.size()) continue;
                if (wears(piece, colours)) return piece;
            }
        }
    }
    assert(false);
    return Placement{};
}

/** The frame whose front and right faces are these two, in either order. */
[[nodiscard]] int frame_of(Face a, Face b) noexcept
{
    for (int frame = 0; frame < 4; ++frame) {
        const Face front = about_up(Face::Front, frame);
        const Face right = about_up(Face::Right, frame);
        if ((front == a && right == b) || (front == b && right == a)) {
            return frame;
        }
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

/** How many turns of the up face carry one pair of side faces onto another. */
[[nodiscard]] int up_turns_between(const std::vector<Face>& from, Face to_first,
                                   Face to_second) noexcept
{
    assert(from.size() == 2);
    for (int turns = 0; turns < 4; ++turns) {
        const Face first = about_up(from[0], turns);
        const Face second = about_up(from[1], turns);
        if ((first == to_first && second == to_second) ||
            (first == to_second && second == to_first)) {
            return turns;
        }
    }
    assert(false);
    return 0;
}

/** Runs a written sequence with every face carried round by `frame`. */
void run(Work& work, const std::vector<FaceTurn>& sequence, int frame)
{
    for (const auto& step : sequence) {
        work.turn(about_up(step.face, frame), step.turns);
    }
}

// The written sequences. Nine of them, and everything else in this file is
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
    {Face::Up, 1},    {Face::Right, 1},  {Face::Up, -1},   {Face::Right, -1},
    {Face::Up, -1},   {Face::Front, -1}, {Face::Up, 1},    {Face::Front, 1}};

/** The mirror of it, for the slot on the left. */
const std::vector<FaceTurn> kInsertLeft{
    {Face::Up, -1},  {Face::Left, -1}, {Face::Up, 1},   {Face::Left, 1},
    {Face::Up, 1},   {Face::Front, 1}, {Face::Up, -1},  {Face::Front, -1}};

/** Turns top edges the right way up, a dot to a line to a cross. */
const std::vector<FaceTurn> kTopCross{{Face::Front, 1},  {Face::Right, 1},
                                      {Face::Up, 1},     {Face::Right, -1},
                                      {Face::Up, -1},    {Face::Front, -1}};

/**
 * Turns three of the top corners up, and moves them about while it does.
 *
 * Seven moves for three corners, where turning them one at a time from
 * underneath costs eight for one. What it gives up is where they end up, which
 * is why the stage that places them comes after this one rather than before:
 * a sequence that moves corners without turning them exists (below), while the
 * other way round the placement would be undone.
 */
const std::vector<FaceTurn> kCornerTurn{
    {Face::Right, 1},  {Face::Up, 1}, {Face::Right, -1}, {Face::Up, 1},
    {Face::Right, 1},  {Face::Up, 2}, {Face::Right, -1}};

/**
 * Moves three top corners round, leaving how they are turned alone.
 *
 * The one thing the seven above cannot do, and the reason the two are in this
 * order. Nine moves, and it never touches the layers underneath.
 */
const std::vector<FaceTurn> kCornerSwap{
    {Face::Right, -1}, {Face::Front, 1},  {Face::Right, -1},
    {Face::Back, 2},   {Face::Right, 1},  {Face::Front, -1},
    {Face::Right, -1}, {Face::Back, 2},   {Face::Right, 2}};

/** Moves three top edges round, leaving everything else where it is. */
const std::vector<FaceTurn> kEdgeCycle{
    {Face::Right, 1},  {Face::Up, -1}, {Face::Right, 1},  {Face::Up, 1},
    {Face::Right, 1},  {Face::Up, 1},  {Face::Right, 1},  {Face::Up, -1},
    {Face::Right, -1}, {Face::Up, -1}, {Face::Right, 2}};

[[nodiscard]] int top_edges_turned_up(const CubeState& cube)
{
    int count = 0;
    for (const Face side : kSides) {
        const auto slot = edge_slot(Face::Up, side, cube.size());
        if (sticker_at(cube, slot, Face::Up) == home_colour(Face::Up)) ++count;
    }
    return count;
}

/** How many of the top corners are already showing the top colour. */
[[nodiscard]] int top_corners_turned_up_count(const CubeState& cube)
{
    int count = 0;
    for (int frame = 0; frame < 4; ++frame) {
        const auto slot =
            corner_slot(Face::Up, about_up(Face::Front, frame),
                        about_up(Face::Right, frame), cube.size());
        if (sticker_at(cube, slot, Face::Up) == home_colour(Face::Up)) {
            ++count;
        }
    }
    return count;
}

[[nodiscard]] int top_corners_in_place(const CubeState& cube)
{
    int count = 0;
    for (int frame = 0; frame < 4; ++frame) {
        const auto front = about_up(Face::Front, frame);
        const auto right = about_up(Face::Right, frame);
        const auto piece =
            read(cube, corner_slot(Face::Up, front, right, cube.size()));
        if (wears(piece, {home_colour(Face::Up), home_colour(front),
                          home_colour(right)})) {
            ++count;
        }
    }
    return count;
}

[[nodiscard]] int top_edges_in_place(const CubeState& cube)
{
    int count = 0;
    for (const Face side : kSides) {
        const auto piece = read(cube, edge_slot(Face::Up, side, cube.size()));
        if (wears(piece, {home_colour(Face::Up), home_colour(side)})) ++count;
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
[[nodiscard]] std::vector<FaceTurn> reversed(
    const std::vector<FaceTurn>& sequence)
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
                run(trial, both[way], frame);

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
    run(work, both[best_way], best_frame);
    return true;
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
/**
 * The same, looking two applications ahead when one is not enough.
 *
 * Turning the last corners up is the one place where the best single use of a
 * sequence can be a step backwards: the way a person is taught it, two of the
 * cases want one sequence and then its opposite, and after the first of them
 * fewer corners are up than before. A search that only ever accepts an
 * improvement never finds the pair.
 *
 * So the pairs are tried too -- a thousand of them, each a few turns of a copy,
 * which is nothing next to what it saves. It is the same knowledge a person
 * has as "these two cases take two algorithms", held as a search rather than
 * as a list of pictures.
 */
void repeat_until_deep(Work& work, const std::vector<FaceTurn>& sequence,
                       int (*score)(const CubeState&), int target)
{
    const std::array<std::vector<FaceTurn>, 2> both{sequence,
                                                    reversed(sequence)};

    for (int guard = 0; score(work.cube()) < target; ++guard) {
        assert(guard < 4);
        if (apply_best(work, sequence, score)) continue;

        int best = -1;
        int first_align = 0;
        int first_frame = 0;
        std::size_t first_way = 0;

        for (std::size_t way = 0; way < both.size(); ++way) {
            for (int align = 0; align < 4; ++align) {
                for (int frame = 0; frame < 4; ++frame) {
                    Work trial{work.cube()};
                    trial.turn(Face::Up, align);
                    run(trial, both[way], frame);

                    // What the best second use of the sequence would reach
                    // from there, which is what makes the first one worth it.
                    Work after{trial.cube()};
                    apply_best(after, sequence, score);

                    const int reached = score(after.cube());
                    if (reached <= best) continue;
                    best = reached;
                    first_align = align;
                    first_frame = frame;
                    first_way = way;
                }
            }
        }

        assert(best >= 0);
        work.turn(Face::Up, first_align);
        run(work, both[first_way], first_frame);
    }
}

void repeat_until(Work& work, const std::vector<FaceTurn>& sequence,
                  int (*score)(const CubeState&), int target)
{
    for (int guard = 0; score(work.cube()) < target; ++guard) {
        assert(guard < 8);
        if (apply_best(work, sequence, score)) continue;
        run(work, sequence, 0);
    }
}

/**
 * Turns the top so the slot above `face` holds nothing already finished.
 *
 * The bottom cross is built in the top layer first, and every way of bringing
 * the next edge up turns one side face, which sends whatever is above that
 * face down with it. There is always a slot to spare: the piece being fetched
 * is not up there yet, so at most three of the four are.
 */
void free_slot_above(Work& work, Face face)
{
    for (int align = 0; align < 4; ++align) {
        const auto slot = edge_slot(Face::Up, face, work.size());
        if (sticker_at(work.cube(), slot, Face::Up) !=
            home_colour(Face::Down)) {
            return;
        }
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
    const auto bottom = home_colour(Face::Down);

    for (const Face side : kSides) {
        for (int guard = 0;; ++guard) {
            assert(guard < 8);
            const auto piece =
                find_piece(work.cube(), {bottom, home_colour(side)});

            if (shows(piece, Face::Up)) {
                if (colour_on(piece, Face::Up) == bottom) break;

                // Turned the wrong way up there. One turn of the face it
                // leans against drops it into the middle, which is the case
                // below and the only one that knows how to turn it over.
                work.turn(besides(piece, Face::Up).front(), 1);
                continue;
            }

            if (shows(piece, Face::Down)) {
                const Face against = besides(piece, Face::Down).front();
                free_slot_above(work, against);
                work.turn(against,
                          colour_on(piece, Face::Down) == bottom ? 2 : 1);
                continue;
            }

            // In the middle. Lifting it with the face its bottom colour is
            // *not* on is what brings that colour out facing up: a quarter
            // turn carries the sticker from the side face to the top one.
            const Face carrying = wearing(piece, bottom);
            const Face lift = besides(piece, carrying).front();
            free_slot_above(work, lift);
            work.turn(lift, carried_by(lift, 1, carrying, work.size()) ==
                                    Face::Up
                                ? 1
                                : -1);
        }
    }
}

/** The flower folded down into the bottom cross, one half turn each. */
void fold_flower(Work& work)
{
    const auto bottom = home_colour(Face::Down);

    for (const Face side : kSides) {
        const auto piece = find_piece(work.cube(), {bottom, home_colour(side)});
        work.turn(Face::Up,
                  up_turns_between(besides(piece, Face::Up).front(), side));
        work.turn(side, 2);
    }
}

void place_bottom_corners(Work& work)
{
    const auto bottom = home_colour(Face::Down);

    for (int frame = 0; frame < 4; ++frame) {
        const auto front = about_up(Face::Front, frame);
        const auto right = about_up(Face::Right, frame);
        const std::vector<FaceColor> colours{bottom, home_colour(front),
                                             home_colour(right)};

        for (int guard = 0;; ++guard) {
            assert(guard < 32);
            const auto piece = find_piece(work.cube(), colours);
            if (at_home(piece)) break;

            if (shows(piece, Face::Down)) {
                // Down there but not right. Lifting it out from where it sits
                // leaves the rest of the bottom layer alone, so the corners
                // already finished stay finished.
                const auto sides = besides(piece, Face::Down);
                run(work, kCornerLift, frame_of(sides.front(), sides.back()));
                continue;
            }

            // Above its slot, the bottom colour points one of three ways, and
            // each has its own way down.
            work.turn(Face::Up,
                      up_turns_between(besides(piece, Face::Up), front, right));

            const auto waiting =
                read(work.cube(), corner_slot(Face::Up, front, right,
                                              work.size()));
            const Face leaning = wearing(waiting, bottom);
            if (leaning == right) {
                run(work, kCornerFromRight, frame);
            } else if (leaning == front) {
                run(work, kCornerFromFront, frame);
            } else {
                run(work, kCornerFromTop, frame);
            }
        }
    }
}

void place_middle_edges(Work& work)
{
    for (int frame = 0; frame < 4; ++frame) {
        const auto front = about_up(Face::Front, frame);
        const auto right = about_up(Face::Right, frame);
        const std::vector<FaceColor> colours{home_colour(front),
                                             home_colour(right)};

        for (int guard = 0;; ++guard) {
            assert(guard < 8);
            const auto piece = find_piece(work.cube(), colours);
            if (at_home(piece)) break;

            if (!shows(piece, Face::Up)) {
                // Stuck in a middle slot, its own or another's. The insertion
                // for that slot, used on an occupied one, puts the occupant
                // back on top -- so there is no second sequence for getting
                // one out.
                run(work, kInsertRight,
                         frame_of(piece.faces.front(), piece.faces.back()));
                continue;
            }

            // On top. Line its side colour up with that colour's own face;
            // then the colour facing up names the slot it belongs in, and
            // which side of the front face that is chooses the sequence.
            const Face leaning = besides(piece, Face::Up).front();
            const Face matching = face_wearing(colour_on(piece, leaning));
            work.turn(Face::Up, up_turns_between(leaning, matching));

            const auto now =
                read(work.cube(), edge_slot(Face::Up, matching, work.size()));
            const Face belongs = face_wearing(colour_on(now, Face::Up));
            const int here = up_turns_between(Face::Front, matching);

            if (about_up(Face::Right, here) == belongs) {
                run(work, kInsertRight, here);
            } else {
                run(work, kInsertLeft, here);
            }
        }
    }
}

void turn_top_edges_up(Work& work)
{
    repeat_until(work, kTopCross, top_edges_turned_up, 4);
}

void turn_top_corners_up(Work& work)
{
    repeat_until_deep(work, kCornerTurn, top_corners_turned_up_count, 4);
}

void place_top_corners(Work& work)
{
    repeat_until(work, kCornerSwap, top_corners_in_place, 4);
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
    const auto bottom = home_colour(Face::Down);
    for (const Face side : kSides) {
        const auto piece = find_piece(cube, {bottom, home_colour(side)});
        if (!shows(piece, Face::Up)) return false;
        if (colour_on(piece, Face::Up) != bottom) return false;
    }
    return true;
}

[[nodiscard]] bool bottom_cross_built(const CubeState& cube)
{
    for (const Face side : kSides) {
        if (!at_home(read(cube, edge_slot(Face::Down, side, cube.size())))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool bottom_corners_built(const CubeState& cube)
{
    for (int frame = 0; frame < 4; ++frame) {
        const auto slot =
            corner_slot(Face::Down, about_up(Face::Front, frame),
                        about_up(Face::Right, frame), cube.size());
        if (!at_home(read(cube, slot))) return false;
    }
    return true;
}

[[nodiscard]] bool middle_layer_built(const CubeState& cube)
{
    for (int frame = 0; frame < 4; ++frame) {
        const auto slot = edge_slot(about_up(Face::Front, frame),
                                    about_up(Face::Right, frame), cube.size());
        if (!at_home(read(cube, slot))) return false;
    }
    return true;
}

[[nodiscard]] bool top_corners_turned_up(const CubeState& cube)
{
    for (int frame = 0; frame < 4; ++frame) {
        const auto slot =
            corner_slot(Face::Up, about_up(Face::Front, frame),
                        about_up(Face::Right, frame), cube.size());
        if (sticker_at(cube, slot, Face::Up) != home_colour(Face::Up)) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool LayerByLayer::supports(int size) const noexcept
{
    return size == 2 || size == 3;
}

std::vector<CubeMove> LayerByLayer::solve(const CubeState& state) const
{
    assert(supports(state.size()));
    return compress(solve_as_three_layers(state));
}

std::vector<CubeMove> solve_as_three_layers(const CubeState& state)
{
    // What "reduced" was only a word for until there was something that could
    // ask. A cube that fails either of these has no answer here rather than a
    // long one: the stages turn outer faces alone, and no run of those reaches
    // a position that an even cube can hold and a three by three cannot.
    assert(reduced(state));
    assert(state.size() < 3 || reachable(parities(projected(state))));

    if (state.is_solved()) return {};

    Work work{state};

    // A two by two is corners and nothing else: no cross to build, no middle
    // layer between the two it has, and no edges on top to finish with. So the
    // stages that deal with edges are not skipped by a flag here -- there is
    // simply nothing for them to find.
    if (state.size() > 2) {
        build_flower(work);
        assert(flower_built(work.cube()));

        fold_flower(work);
        assert(bottom_cross_built(work.cube()));
    }

    place_bottom_corners(work);
    assert(bottom_corners_built(work.cube()));

    if (state.size() > 2) {
        place_middle_edges(work);
        assert(middle_layer_built(work.cube()));

        turn_top_edges_up(work);
        assert(top_edges_turned_up(work.cube()) == 4);
    }

    turn_top_corners_up(work);
    assert(top_corners_turned_up(work.cube()));

    place_top_corners(work);
    assert(top_corners_in_place(work.cube()) == 4);
    assert(top_corners_turned_up(work.cube()));

    if (state.size() > 2) {
        place_top_edges(work);
    }

    assert(work.cube().is_solved());
    return work.take();
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
        if (turns != 0) {
            joined.push_back(CubeMove{move.axis, move.layers, turns});
        }
    }

    return joined;
}

}  // namespace rubiks::cube::solver
