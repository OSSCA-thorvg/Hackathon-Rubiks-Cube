#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"
#include "cube/Cubie.hpp"
#include "cube/Scramble.hpp"
#include "cube/Surface.hpp"

/**
 * Which quantities a scramble cannot change, found by scrambling and looking.
 *
 * Phase 18 has to decide what makes a painted cube a possible cube, and the
 * rule differs with the size in ways that are easy to state wrongly. A rule
 * too tight refuses somebody's real cube; a rule too loose hands the solver a
 * position it will never finish. Neither is a thing to settle from memory, so
 * this settles it by measurement: scramble a great many times, work out every
 * candidate quantity each time, and report which ones never moved.
 *
 * Nothing here asks the cube what colour anything is. Colour cannot tell two
 * stickers of a face apart, and every quantity below is about *which piece*
 * sits where -- so the permutation is followed directly, by carrying the
 * sticker identities of `Surface` through the same turns the cube gets. What
 * comes out is ground truth rather than an inference from six colours.
 *
 * Hidden behind its own tag: it is a survey whose output is a table for the
 * plan, not a check that anything is right. Run it with
 * `cube-test "[.survey]"`.
 */

namespace {

using rubiks::cube::Axis;
using rubiks::cube::coordinate_on;
using rubiks::cube::CubeMove;
using rubiks::cube::CubiePosition;
using rubiks::cube::Face;
using rubiks::cube::faces;
using rubiks::cube::layer;
using rubiks::cube::make_scramble;
using rubiks::cube::outer_layer;
using rubiks::cube::SurfaceSticker;
using rubiks::cube::surface_stickers;
using rubiks::cube::turned_sticker;

constexpr std::array<Axis, 3> kAxes{Axis::X, Axis::Y, Axis::Z};

/** The stickers of a size, and the index each one answers to. */
class Slots {
public:
    explicit Slots(int size) : size_(size), slots_(surface_stickers(size))
    {
        for (std::size_t i = 0; i < slots_.size(); ++i) {
            index_[key(slots_[i])] = static_cast<int>(i);
        }

        // The stickers of one cubie, gathered under the cubie they belong to
        // and kept in the order the faces are declared, so that "the first
        // sticker of this piece" means the same thing at every slot.
        for (std::size_t i = 0; i < slots_.size(); ++i) {
            const auto& s = slots_[i];
            pieces_[{s.x, s.y, s.z}].push_back(static_cast<int>(i));
        }
        for (auto& [where, list] : pieces_) {
            std::sort(list.begin(), list.end(),
                      [&](int a, int b) {
                          return static_cast<int>(slots_[static_cast<std::size_t>(a)].face) <
                                 static_cast<int>(slots_[static_cast<std::size_t>(b)].face);
                      });
            piece_of_.emplace(where, static_cast<int>(order_.size()));
            order_.push_back(where);
        }
    }

    [[nodiscard]] int size() const noexcept { return size_; }
    [[nodiscard]] const std::vector<SurfaceSticker>& all() const noexcept
    {
        return slots_;
    }
    [[nodiscard]] int at(const SurfaceSticker& s) const
    {
        return index_.at(key(s));
    }
    [[nodiscard]] const std::vector<int>& stickers_of(int piece) const
    {
        return pieces_.at(order_[static_cast<std::size_t>(piece)]);
    }
    [[nodiscard]] int piece_at(int slot) const
    {
        const auto& s = slots_[static_cast<std::size_t>(slot)];
        return piece_of_.at({s.x, s.y, s.z});
    }
    [[nodiscard]] int piece_count() const noexcept
    {
        return static_cast<int>(order_.size());
    }
    /** How many faces a piece shows: three for a corner, one for a centre. */
    [[nodiscard]] int arms(int piece) const
    {
        return static_cast<int>(stickers_of(piece).size());
    }

private:
    struct Key {
        int x, y, z, face;
        [[nodiscard]] bool operator<(const Key& other) const noexcept
        {
            return std::tie(x, y, z, face) <
                   std::tie(other.x, other.y, other.z, other.face);
        }
    };
    struct Where {
        int x, y, z;
        [[nodiscard]] bool operator<(const Where& other) const noexcept
        {
            return std::tie(x, y, z) < std::tie(other.x, other.y, other.z);
        }
    };
    [[nodiscard]] static Key key(const SurfaceSticker& s) noexcept
    {
        return Key{s.x, s.y, s.z, static_cast<int>(s.face)};
    }

    int size_;
    std::vector<SurfaceSticker> slots_;
    std::map<Key, int> index_;
    std::map<Where, std::vector<int>> pieces_;
    std::map<Where, int> piece_of_;
    std::vector<Where> order_;
};

/** Where each sticker ends up after one move. */
[[nodiscard]] std::vector<int> landing_of(const Slots& slots,
                                          const CubeMove& move)
{
    std::vector<int> lands(slots.all().size(), 0);
    const int turns = ((move.quarter_turns % 4) + 4) % 4;

    for (std::size_t i = 0; i < slots.all().size(); ++i) {
        SurfaceSticker s = slots.all()[i];
        if ((move.layers & layer(coordinate_on(move.axis, s))) != 0) {
            for (int t = 0; t < turns; ++t) {
                s = turned_sticker(s, move.axis, slots.size());
            }
        }
        lands[i] = slots.at(s);
    }
    return lands;
}

/** Where each sticker ends up after a whole run of moves. */
[[nodiscard]] std::vector<int> landing_of(const Slots& slots,
                                          const std::vector<CubeMove>& moves)
{
    std::vector<int> lands(slots.all().size(), 0);
    for (std::size_t i = 0; i < lands.size(); ++i) lands[i] = static_cast<int>(i);

    for (const auto& move : moves) {
        const auto step = landing_of(slots, move);
        for (std::size_t i = 0; i < lands.size(); ++i) {
            lands[i] = step[static_cast<std::size_t>(lands[i])];
        }
    }
    return lands;
}

/** Every quarter turn a size has: the six faces and every inner slice. */
[[nodiscard]] std::vector<CubeMove> quarter_turns(int size)
{
    std::vector<CubeMove> moves;
    for (const Axis axis : kAxes) {
        for (int at = 0; at < size; ++at) {
            moves.push_back(CubeMove{axis, layer(at), 1});
        }
    }
    return moves;
}

/** Which closed set of slots each slot belongs to, walked rather than argued. */
[[nodiscard]] std::vector<int> sticker_orbits(const Slots& slots)
{
    std::vector<std::vector<int>> steps;
    for (const auto& move : quarter_turns(slots.size())) {
        steps.push_back(landing_of(slots, move));
    }

    std::vector<int> which(slots.all().size(), -1);
    int next = 0;
    for (std::size_t start = 0; start < which.size(); ++start) {
        if (which[start] >= 0) continue;
        which[start] = next;
        std::vector<int> waiting{static_cast<int>(start)};
        while (!waiting.empty()) {
            const auto here = static_cast<std::size_t>(waiting.back());
            waiting.pop_back();
            for (const auto& lands : steps) {
                const auto there = static_cast<std::size_t>(lands[here]);
                if (which[there] >= 0) continue;
                which[there] = next;
                waiting.push_back(static_cast<int>(there));
            }
        }
        ++next;
    }
    return which;
}

/** Whether a permutation takes an odd number of swaps to undo. */
[[nodiscard]] int parity_of(const std::vector<int>& perm)
{
    std::vector<bool> seen(perm.size(), false);
    int odd = 0;
    for (std::size_t i = 0; i < perm.size(); ++i) {
        if (seen[i]) continue;
        std::size_t length = 0;
        for (std::size_t at = i; !seen[at];
             at = static_cast<std::size_t>(perm[at])) {
            seen[at] = true;
            ++length;
        }
        if (length % 2 == 0) odd ^= 1;
    }
    return odd;
}

/** One scramble, read as pieces rather than as colours. */
struct Reading {
    /** For each piece slot, the piece that is sitting in it. */
    std::vector<int> piece_from;
    /** For each piece slot, how far round the piece in it is turned. */
    std::vector<int> spin;
    /** For each sticker slot, the slot the sticker in it set out from. */
    std::vector<int> came_from;
};

[[nodiscard]] Reading read(const Slots& slots,
                           const std::vector<CubeMove>& moves)
{
    const auto lands = landing_of(slots, moves);
    std::vector<int> came_from(lands.size(), 0);
    for (std::size_t i = 0; i < lands.size(); ++i) {
        came_from[static_cast<std::size_t>(lands[i])] = static_cast<int>(i);
    }

    Reading out;
    out.piece_from.assign(static_cast<std::size_t>(slots.piece_count()), 0);
    out.spin.assign(static_cast<std::size_t>(slots.piece_count()), 0);

    for (int piece = 0; piece < slots.piece_count(); ++piece) {
        const auto& here = slots.stickers_of(piece);
        const int source_slot = came_from[static_cast<std::size_t>(here[0])];
        const int source = slots.piece_at(source_slot);
        out.piece_from[static_cast<std::size_t>(piece)] = source;

        // How far round: which arm of the piece that arrived is showing
        // through this slot's first face. Nothing here says that is the
        // conventional way to count a twist -- it is a candidate, and the
        // survey's job is to say whether it is one that never changes.
        const auto& home = slots.stickers_of(source);
        for (std::size_t m = 0; m < home.size(); ++m) {
            if (home[m] == source_slot) {
                out.spin[static_cast<std::size_t>(piece)] =
                    static_cast<int>(m);
            }
        }
    }
    out.came_from = std::move(came_from);
    return out;
}

/**
 * How a corner sits in its slot, counted several ways at once.
 *
 * A corner's three stickers, sorted the way the faces are declared, are always
 * one from each axis in the order X, Y, Z -- so entry one is the up-or-down
 * face at every corner of every cube, and "the piece's up-or-down sticker is
 * on the slot's up-or-down face" is a reference that needs no special case.
 *
 * What that leaves open is which way round to count, because the three faces
 * of a corner read clockwise from outside at four of the corners and
 * anticlockwise at the other four -- whichever fixed order they are put in.
 * Counting the wrong way at half of them turns a sum that never moves into one
 * that does, so the handedness is offered as its own candidate rather than
 * assumed: `chirality` is the parity of how many of the corner's coordinates
 * sit at the low end of their axis.
 */
struct CornerSit {
    int raw = 0;      ///< Where the piece's up/down sticker landed: 0, 1 or 2.
    int turned = 0;   ///< The same, measured from the slot's own up/down face.
    int handed = 0;   ///< `turned` counted backwards at left-handed corners.
};

[[nodiscard]] CornerSit corner_sit(const Slots& slots, const Reading& reading,
                                   int piece)
{
    const auto& here = slots.stickers_of(piece);
    const int source = reading.piece_from[static_cast<std::size_t>(piece)];
    const auto& home = slots.stickers_of(source);

    CornerSit sit;
    for (std::size_t k = 0; k < here.size(); ++k) {
        const int from = reading.came_from[static_cast<std::size_t>(here[k])];
        if (from == home[1]) sit.raw = static_cast<int>(k);
    }
    sit.turned = ((sit.raw - 1) % 3 + 3) % 3;

    const auto& s = slots.all()[static_cast<std::size_t>(here.front())];
    const int low = (s.x == 0 ? 1 : 0) + (s.y == 0 ? 1 : 0) + (s.z == 0 ? 1 : 0);
    sit.handed = low % 2 == 0 ? sit.turned : (3 - sit.turned) % 3;
    return sit;
}

/** The pieces of one kind, so a quantity can be asked of that kind alone. */
[[nodiscard]] std::vector<int> pieces_with(const Slots& slots, int arms)
{
    std::vector<int> out;
    for (int piece = 0; piece < slots.piece_count(); ++piece) {
        if (slots.arms(piece) == arms) out.push_back(piece);
    }
    return out;
}

/** The permutation a reading makes of one set of pieces, renumbered from zero. */
[[nodiscard]] std::vector<int> restricted(const Reading& reading,
                                          const std::vector<int>& among)
{
    std::map<int, int> place;
    for (std::size_t i = 0; i < among.size(); ++i) {
        place[among[i]] = static_cast<int>(i);
    }
    std::vector<int> perm(among.size(), 0);
    for (std::size_t i = 0; i < among.size(); ++i) {
        const int from =
            reading.piece_from[static_cast<std::size_t>(among[i])];
        perm[i] = place.at(from);
    }
    return perm;
}

/** Every value one candidate took across every scramble tried. */
using Seen = std::map<std::string, std::set<int>>;

void survey(int size, std::uint32_t scrambles, Seen& seen)
{
    const Slots slots(size);
    const auto orbit = sticker_orbits(slots);
    const int orbits = *std::max_element(orbit.begin(), orbit.end()) + 1;

    const auto corners = pieces_with(slots, 3);
    const auto edges = pieces_with(slots, 2);
    const auto centres = pieces_with(slots, 1);

    // Edge pieces split by how deep they sit, because a piece never leaves
    // its depth and each depth is its own puzzle.
    std::map<int, std::vector<int>> edges_by_depth;
    for (const int piece : edges) {
        const auto& s = slots.all()[static_cast<std::size_t>(
            slots.stickers_of(piece).front())];
        const CubiePosition at{s.x, s.y, s.z};
        int depth = size;
        for (const Axis axis : kAxes) {
            const int c = coordinate_on(axis, at);
            if (c == 0 || c == size - 1) continue;
            depth = std::min({depth, c, size - 1 - c});
        }
        edges_by_depth[depth].push_back(piece);
    }

    // Centre pieces split by their orbit, which is the thing the plan has to
    // count colours inside.
    std::map<int, std::vector<int>> centres_by_orbit;
    for (const int piece : centres) {
        const int slot = slots.stickers_of(piece).front();
        centres_by_orbit[orbit[static_cast<std::size_t>(slot)]].push_back(piece);
    }

    std::cout << "size " << size << ": " << slots.all().size() << " stickers, "
              << orbits << " sticker orbits; " << corners.size() << " corners, "
              << edges.size() << " edges in " << edges_by_depth.size()
              << " depths, " << centres.size() << " centres in "
              << centres_by_orbit.size() << " orbits\n";

    const auto note = [&](const std::string& what, int value) {
        seen["n=" + std::to_string(size) + " " + what].insert(value);
    };

    for (std::uint32_t s = 0; s < scrambles; ++s) {
        const auto moves = make_scramble(size, s, 60);
        const auto reading = read(slots, moves);

        if (!corners.empty()) {
            // Four ways of counting the same thing, because which one is the
            // invariant is exactly what is not known in advance.
            int spin = 0;
            int raw = 0;
            int turned = 0;
            int handed = 0;
            for (const int piece : corners) {
                spin += reading.spin[static_cast<std::size_t>(piece)];
                const auto sit = corner_sit(slots, reading, piece);
                raw += sit.raw;
                turned += sit.turned;
                handed += sit.handed;
            }
            note("corner twist A (arm at slot 0) mod 3", spin % 3);
            note("corner twist B (raw up/down index) mod 3", raw % 3);
            note("corner twist C (from slot's up/down) mod 3", turned % 3);
            note("corner twist D (C, handedness respected) mod 3", handed % 3);
            note("corner permutation parity",
                 parity_of(restricted(reading, corners)));
        }

        for (const auto& [depth, group] : edges_by_depth) {
            int flip = 0;
            for (const int piece : group) {
                flip += reading.spin[static_cast<std::size_t>(piece)];
            }
            // The sum and not the terms. A single piece's `spin` is read off
            // an ordering of faces, and that ordering runs the opposite way
            // round at half the slots, so one piece's number says nothing on
            // its own -- which was tried, and said only that some number was
            // non-zero, every time, at every size. Two of anything cancel
            // under a sum taken modulo two, so the sum survives the ordering
            // even though its terms do not. The corners have the same problem
            // and cannot solve it this way, which is why they carry a
            // handedness and these do not.
            note("edge flip sum mod 2, depth " + std::to_string(depth),
                 flip % 2);
            note("edge permutation parity, depth " + std::to_string(depth),
                 parity_of(restricted(reading, group)));
        }

        for (const auto& [which, group] : centres_by_orbit) {
            note("centre permutation parity, orbit " + std::to_string(which),
                 parity_of(restricted(reading, group)));
        }

        if (!corners.empty() && edges_by_depth.count(1) != 0) {
            note("corner parity XOR depth-1 edge parity",
                 parity_of(restricted(reading, corners)) ^
                     parity_of(restricted(reading, edges_by_depth.at(1))));
        }
    }
}

}  // namespace

TEST_CASE("what a scramble cannot change", "[.survey]")
{
    Seen seen;
    for (const int size : {2, 3, 4, 5, 6, 7}) {
        survey(size, 400, seen);
    }

    std::cout << "\nfixed across every scramble (a rule may use these):\n";
    for (const auto& [what, values] : seen) {
        if (values.size() != 1) continue;
        std::cout << "  " << what << " = " << *values.begin() << "\n";
    }

    std::cout << "\nfree (a rule must NOT use these):\n";
    for (const auto& [what, values] : seen) {
        if (values.size() == 1) continue;
        std::cout << "  " << what << " takes " << values.size() << " values\n";
    }
    std::cout << std::endl;
}
