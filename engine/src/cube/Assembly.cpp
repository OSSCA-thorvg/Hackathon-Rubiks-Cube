#include "cube/Assembly.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "cube/CubeMove.hpp"
#include "cube/Surface.hpp"

namespace rubiks::cube {
namespace {

constexpr std::array<Axis, 3> kAxes{Axis::X, Axis::Y, Axis::Z};

/** A cubie slot, as something that can be looked up. */
struct Where {
    int x, y, z;
    [[nodiscard]] bool operator<(const Where& other) const noexcept
    {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;
        return z < other.z;
    }
};

/** A sticker, as something that can be looked up. */
struct Which {
    int x, y, z, face;
    [[nodiscard]] bool operator<(const Which& other) const noexcept
    {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;
        if (z != other.z) return z < other.z;
        return face < other.face;
    }
};

[[nodiscard]] Which which_of(const SurfaceSticker& s) noexcept
{
    return Which{s.x, s.y, s.z, static_cast<int>(s.face)};
}

/**
 * Everything about a size that a painting of it can be read against.
 *
 * All of it is geometry: which stickers there are, which of them make up one
 * piece, which closed set each belongs to, and the twenty-four ways the whole
 * cube can be held. Nothing here has an opinion about colour.
 */
class Frame {
public:
    explicit Frame(int size) : size_(size), slots_(surface_stickers(size))
    {
        for (std::size_t i = 0; i < slots_.size(); ++i) {
            at_[which_of(slots_[i])] = static_cast<int>(i);
        }

        // The stickers of one cubie, kept in the order the faces are declared.
        // A corner's three then read as one from each axis, X then Y then Z,
        // at every corner of every size -- which is what lets "the piece's
        // up-or-down sticker" be asked without a special case.
        std::map<Where, std::vector<int>> gathered;
        for (std::size_t i = 0; i < slots_.size(); ++i) {
            const auto& s = slots_[i];
            gathered[Where{s.x, s.y, s.z}].push_back(static_cast<int>(i));
        }
        for (auto& [where, list] : gathered) {
            std::sort(list.begin(), list.end(), [&](int a, int b) {
                return face_of(a) < face_of(b);
            });
            piece_at_[where] = static_cast<int>(pieces_.size());
            pieces_.push_back(std::move(list));
        }

        build_rotations();
        build_orbits();
    }

    [[nodiscard]] int size() const noexcept { return size_; }
    [[nodiscard]] const std::vector<SurfaceSticker>& slots() const noexcept
    {
        return slots_;
    }
    [[nodiscard]] const std::vector<std::vector<int>>& pieces() const noexcept
    {
        return pieces_;
    }
    [[nodiscard]] std::size_t rotations() const noexcept
    {
        return turn_.size();
    }
    /** Where rotation `r` sends sticker `slot`. */
    [[nodiscard]] int turned(std::size_t r, int slot) const noexcept
    {
        return turn_[r][static_cast<std::size_t>(slot)];
    }
    /** Which piece rotation `r` sends to piece slot `p`. */
    [[nodiscard]] int came_to(std::size_t r, int p) const noexcept
    {
        return arrives_[r][static_cast<std::size_t>(p)];
    }
    /** Which closed set of stickers a sticker belongs to. */
    [[nodiscard]] int orbit(int slot) const noexcept
    {
        return orbit_[static_cast<std::size_t>(slot)];
    }
    [[nodiscard]] int orbit_count() const noexcept { return orbits_; }
    /** How many slots one closed set holds. */
    [[nodiscard]] std::size_t orbit_size(int which) const noexcept
    {
        return held_[static_cast<std::size_t>(which)];
    }

    [[nodiscard]] int face_of(int slot) const noexcept
    {
        return static_cast<int>(slots_[static_cast<std::size_t>(slot)].face);
    }
    [[nodiscard]] FaceColor home_colour(int slot) const noexcept
    {
        return solved_color(slots_[static_cast<std::size_t>(slot)].face);
    }
    [[nodiscard]] int piece_of(int slot) const
    {
        const auto& s = slots_[static_cast<std::size_t>(slot)];
        return piece_at_.at(Where{s.x, s.y, s.z});
    }
    /** How many faces a piece shows: three for a corner, one for a centre. */
    [[nodiscard]] int arms(int piece) const noexcept
    {
        return static_cast<int>(pieces_[static_cast<std::size_t>(piece)].size());
    }

    /**
     * How far in from the nearest corner a piece sits.
     *
     * The edges of a big cube are several puzzles side by side: a piece one
     * layer in stays one layer in whatever is turned, so each depth carries
     * its own count of what is turned round.
     */
    [[nodiscard]] int depth_of(int piece) const noexcept
    {
        const auto& s = slots_[static_cast<std::size_t>(
            pieces_[static_cast<std::size_t>(piece)].front())];
        int depth = size_;
        for (const Axis axis : kAxes) {
            const int c = coordinate_on(axis, CubiePosition{s.x, s.y, s.z});
            if (c == 0 || c == size_ - 1) continue;
            depth = std::min(depth, std::min(c, size_ - 1 - c));
        }
        return depth;
    }

    /**
     * Whether a corner's three faces read the short way round.
     *
     * Taken in the order the faces are declared they run one way at four
     * corners and the other way at the remaining four, and a twist counted the
     * wrong way round at half of them turns a sum that never moves into one
     * that takes every value. Measured, not assumed: three other ways of
     * counting were tried and all three moved.
     */
    [[nodiscard]] bool left_handed(int piece) const noexcept
    {
        const auto& s = slots_[static_cast<std::size_t>(
            pieces_[static_cast<std::size_t>(piece)].front())];
        const int low = (s.x == 0 ? 1 : 0) + (s.y == 0 ? 1 : 0) +
                        (s.z == 0 ? 1 : 0);
        return low % 2 != 0;
    }

private:
    void build_rotations()
    {
        // A whole-cube rotation is every layer turned at once, which is not a
        // move the puzzle allows -- it is how the cube is *held*. Three of
        // them generate all twenty-four, so they are grown from the three
        // rather than written out.
        std::vector<std::vector<int>> seeds;
        for (const Axis axis : kAxes) {
            std::vector<int> perm(slots_.size(), 0);
            for (std::size_t i = 0; i < slots_.size(); ++i) {
                perm[i] = at_.at(
                    which_of(turned_sticker(slots_[i], axis, size_)));
            }
            seeds.push_back(std::move(perm));
        }

        std::vector<int> identity(slots_.size(), 0);
        for (std::size_t i = 0; i < identity.size(); ++i) {
            identity[i] = static_cast<int>(i);
        }

        std::set<std::vector<int>> seen{identity};
        turn_.push_back(identity);
        for (std::size_t head = 0; head < turn_.size(); ++head) {
            for (const auto& seed : seeds) {
                std::vector<int> next(slots_.size(), 0);
                for (std::size_t i = 0; i < next.size(); ++i) {
                    next[i] = seed[static_cast<std::size_t>(turn_[head][i])];
                }
                if (seen.insert(next).second) turn_.push_back(std::move(next));
            }
        }

        // The same rotations read as "which piece arrives here", which is the
        // direction a reading asks in.
        arrives_.assign(turn_.size(), {});
        for (std::size_t r = 0; r < turn_.size(); ++r) {
            arrives_[r].assign(pieces_.size(), -1);
            for (std::size_t q = 0; q < pieces_.size(); ++q) {
                const int lands = turn_[r][static_cast<std::size_t>(
                    pieces_[q].front())];
                arrives_[r][static_cast<std::size_t>(piece_of(lands))] =
                    static_cast<int>(q);
            }
        }
    }

    void build_orbits()
    {
        // Every quarter turn the puzzle allows, which is what a piece can
        // actually be carried by -- unlike the rotations above.
        std::vector<std::vector<int>> steps;
        for (const Axis axis : kAxes) {
            for (int at = 0; at < size_; ++at) {
                std::vector<int> lands(slots_.size(), 0);
                for (std::size_t i = 0; i < slots_.size(); ++i) {
                    SurfaceSticker s = slots_[i];
                    if (coordinate_on(axis, s) == at) {
                        s = turned_sticker(s, axis, size_);
                    }
                    lands[i] = at_.at(which_of(s));
                }
                steps.push_back(std::move(lands));
            }
        }

        orbit_.assign(slots_.size(), -1);
        orbits_ = 0;
        for (std::size_t start = 0; start < orbit_.size(); ++start) {
            if (orbit_[start] >= 0) continue;
            orbit_[start] = orbits_;
            std::vector<int> waiting{static_cast<int>(start)};
            while (!waiting.empty()) {
                const auto here = static_cast<std::size_t>(waiting.back());
                waiting.pop_back();
                for (const auto& lands : steps) {
                    const auto there = static_cast<std::size_t>(lands[here]);
                    if (orbit_[there] >= 0) continue;
                    orbit_[there] = orbits_;
                    waiting.push_back(static_cast<int>(there));
                }
            }
            ++orbits_;
        }

        held_.assign(static_cast<std::size_t>(orbits_), 0);
        for (const int which : orbit_) ++held_[static_cast<std::size_t>(which)];
    }

    int size_;
    std::vector<SurfaceSticker> slots_;
    std::map<Which, int> at_;
    std::map<Where, int> piece_at_;
    std::vector<std::vector<int>> pieces_;
    std::vector<std::vector<int>> turn_;
    std::vector<std::vector<int>> arrives_;
    std::vector<int> orbit_;
    std::vector<std::size_t> held_;
    int orbits_ = 0;
};

/** The frames, built once for each size somebody asks about. */
[[nodiscard]] const Frame& frame_for(int size)
{
    static std::map<int, Frame> built;
    const auto found = built.find(size);
    if (found != built.end()) return found->second;
    return built.emplace(size, Frame{size}).first->second;
}

/** Which colours never share a piece, which is a cube's scheme of opposites. */
[[nodiscard]] std::array<int, kFaceCount> opposites_of(
    const Frame& frame, const std::vector<FaceColor>& painting)
{
    std::array<std::array<bool, kFaceCount>, kFaceCount> together{};
    for (const auto& piece : frame.pieces()) {
        for (std::size_t a = 0; a < piece.size(); ++a) {
            for (std::size_t b = 0; b < piece.size(); ++b) {
                if (a == b) continue;
                const auto ca = static_cast<std::size_t>(
                    painting[static_cast<std::size_t>(piece[a])]);
                const auto cb = static_cast<std::size_t>(
                    painting[static_cast<std::size_t>(piece[b])]);
                together[ca][cb] = true;
            }
        }
    }

    std::array<int, kFaceCount> facing{};
    for (std::size_t a = 0; a < kFaceCount; ++a) {
        facing[a] = -1;
        for (std::size_t b = 0; b < kFaceCount; ++b) {
            if (a == b || together[a][b]) continue;
            facing[a] = facing[a] < 0 ? static_cast<int>(b) : -2;
        }
    }
    return facing;
}

/** The scheme this build solves towards: red opposite orange, and so on. */
[[nodiscard]] std::array<int, kFaceCount> standard_opposites()
{
    std::array<int, kFaceCount> facing{};
    for (const Face face : faces()) {
        const auto axis = axis_of(face);
        for (const Face other : faces()) {
            if (other != face && axis_of(other) == axis) {
                facing[static_cast<std::size_t>(solved_color(face))] =
                    static_cast<int>(solved_color(other));
            }
        }
    }
    return facing;
}

/** A painting turned to a different way of holding the cube. */
[[nodiscard]] std::vector<FaceColor> held(const Frame& frame,
                                          const std::vector<FaceColor>& painting,
                                          std::size_t rotation)
{
    std::vector<FaceColor> out(painting.size(), FaceColor::Red);
    for (std::size_t i = 0; i < painting.size(); ++i) {
        out[static_cast<std::size_t>(
            frame.turned(rotation, static_cast<int>(i)))] = painting[i];
    }
    return out;
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

/** One painting read in one way of holding the cube. */
PaintReading read_held(const Frame& frame,
                       const std::vector<FaceColor>& painting)
{
    const int size = frame.size();
    const int per_colour = size * size;
    PaintReading out;

    // Colours, counted over the whole cube and then over each closed set of
    // slots on its own. The second catches what the first cannot: two centres
    // of different colours traded between two orbits leaves every total alone.
    std::array<int, kFaceCount> total{};
    std::vector<std::array<int, kFaceCount>> per_orbit(
        static_cast<std::size_t>(frame.orbit_count()));
    std::vector<std::array<int, kFaceCount>> wanted(
        static_cast<std::size_t>(frame.orbit_count()));

    for (std::size_t i = 0; i < painting.size(); ++i) {
        const auto slot = static_cast<int>(i);
        ++total[static_cast<std::size_t>(painting[i])];
        ++per_orbit[static_cast<std::size_t>(frame.orbit(slot))]
                   [static_cast<std::size_t>(painting[i])];
        ++wanted[static_cast<std::size_t>(frame.orbit(slot))]
                [static_cast<std::size_t>(frame.home_colour(slot))];
    }

    for (const Face face : faces()) {
        const auto colour = solved_color(face);
        const int found = total[static_cast<std::size_t>(colour)];
        if (found == per_colour) continue;
        out.fault = PaintFault::ColourCount;
        out.colour = colour;
        out.expected = per_colour;
        out.found = found;
        for (std::size_t i = 0; i < painting.size(); ++i) {
            if (painting[i] == colour) out.blamed.push_back(static_cast<int>(i));
        }
        return out;
    }

    // Which colours never share a piece, asked before anything else about
    // arrangement. A cube built to another scheme answers differently, and
    // saying so beats picking one of its stickers and calling it wrong.
    //
    // Only when the answer is a *scheme* -- every colour with exactly one
    // partner it never meets. One sticker copied down wrong can also leave two
    // colours sharing a piece that never should, and that is a slip rather
    // than a scheme; the piece checks below name it properly, so this stands
    // aside for it.
    const auto facing = opposites_of(frame, painting);
    bool paired = true;
    for (std::size_t c = 0; c < kFaceCount; ++c) {
        if (facing[c] < 0 || facing[static_cast<std::size_t>(facing[c])] !=
                                 static_cast<int>(c)) {
            paired = false;
        }
    }
    if (paired && facing != standard_opposites()) {
        out.fault = PaintFault::OppositePairs;
        return out;
    }

    for (std::size_t o = 0; o < per_orbit.size(); ++o) {
        for (std::size_t c = 0; c < kFaceCount; ++c) {
            if (per_orbit[o][c] == wanted[o][c]) continue;
            out.fault = PaintFault::OrbitCount;
            out.colour = static_cast<FaceColor>(c);
            out.expected = wanted[o][c];
            out.found = per_orbit[o][c];
            for (std::size_t i = 0; i < painting.size(); ++i) {
                if (frame.orbit(static_cast<int>(i)) == static_cast<int>(o)) {
                    out.blamed.push_back(static_cast<int>(i));
                }
            }
            return out;
        }
    }

    // The middles of the six faces, when a size has them, are the one orbit
    // whose tally is not the whole story. Six slots, one per face, and the
    // moves that carry them act on them exactly as the cube's own turnings do
    // -- so of the seven hundred and twenty ways to deal six colours out to
    // them only twenty-four can be reached, and every one of those is a way of
    // holding the cube rather than a way of muddling it. Every way of holding
    // it is already being tried outside this, so inside it each of the six has
    // to be showing its own colour.
    //
    // Left to the tally alone this is silently free, and an odd cube read from
    // a different way up comes back as a *different* cube -- which is how the
    // omission was found.
    for (std::size_t i = 0; i < painting.size(); ++i) {
        const auto slot = static_cast<int>(i);
        if (frame.arms(frame.piece_of(slot)) != 1) continue;
        if (frame.orbit_size(frame.orbit(slot)) != kFaceCount) continue;
        if (painting[i] == frame.home_colour(slot)) continue;

        out.fault = PaintFault::OrbitCount;
        out.colour = frame.home_colour(slot);
        out.expected = 1;
        out.found = 0;
        out.blamed.push_back(slot);
        return out;
    }

    // Which piece is in each slot, and which of its stickers went where.
    //
    // Asked of the geometry rather than of a table of colour triples: a piece
    // can sit in a slot exactly when some way of holding the cube carries it
    // there, and the holding says which sticker lands on which face. Corner
    // handedness, wing chirality and the orbits all fall out of that one
    // question instead of being three rules to get right separately.
    std::vector<int> home(frame.pieces().size(), -1);
    std::vector<int> came_from(painting.size(), -1);

    for (std::size_t p = 0; p < frame.pieces().size(); ++p) {
        const auto& here = frame.pieces()[p];

        // Centres are left out of this, and out of the count below. A centre
        // shows one colour and nothing else, so two centres of a colour are
        // the same piece as far as any turn or any eye is concerned -- asking
        // which of them is which would only ever invent an answer, and asking
        // for a one-to-one match would refuse every real cube from four by
        // four up. What a centre has to satisfy is its orbit's tally, and that
        // has already been checked above.
        if (here.size() < 2) continue;

        for (std::size_t r = 0; r < frame.rotations(); ++r) {
            const int q = frame.came_to(r, static_cast<int>(p));
            if (q < 0) continue;
            const auto& from = frame.pieces()[static_cast<std::size_t>(q)];
            if (from.size() != here.size()) continue;

            bool fits = true;
            for (const int s : from) {
                const int lands = frame.turned(r, s);
                if (painting[static_cast<std::size_t>(lands)] !=
                    frame.home_colour(s)) {
                    fits = false;
                    break;
                }
            }
            if (!fits) continue;

            home[p] = q;
            for (const int s : from) came_from[static_cast<std::size_t>(
                frame.turned(r, s))] = s;
            break;
        }

        if (home[p] < 0) {
            out.fault = PaintFault::ImpossiblePiece;
            out.blamed = here;
            return out;
        }
    }

    std::vector<int> used(frame.pieces().size(), 0);
    for (std::size_t p = 0; p < home.size(); ++p) {
        if (home[p] < 0) continue;
        if (used[static_cast<std::size_t>(home[p])]++ == 0) continue;
        out.fault = PaintFault::RepeatedPiece;
        out.blamed = frame.pieces()[p];
        return out;
    }

    // The corners' twist, counted the one way that holds still.
    int twist = 0;
    std::vector<int> corners;
    for (std::size_t p = 0; p < frame.pieces().size(); ++p) {
        if (frame.arms(static_cast<int>(p)) != 3) continue;
        corners.push_back(static_cast<int>(p));

        const auto& here = frame.pieces()[p];
        const auto& from = frame.pieces()[static_cast<std::size_t>(home[p])];
        int raw = 0;
        for (std::size_t k = 0; k < here.size(); ++k) {
            if (came_from[static_cast<std::size_t>(here[k])] == from[1]) {
                raw = static_cast<int>(k);
            }
        }
        const int turned = ((raw - 1) % 3 + 3) % 3;
        twist += frame.left_handed(static_cast<int>(p)) ? (3 - turned) % 3
                                                        : turned;
    }
    if (twist % 3 != 0) {
        out.fault = PaintFault::CornerTwist;
        for (const int p : corners) {
            const auto& here = frame.pieces()[static_cast<std::size_t>(p)];
            out.blamed.insert(out.blamed.end(), here.begin(), here.end());
        }
        return out;
    }

    // The edges' flip, per depth and as a sum. One piece's number is tied to
    // the same ordering of faces the corners had to correct for, so it says
    // nothing standing alone; two of them cancel under a sum taken modulo two,
    // which is why the sum survives an ordering its terms do not.
    std::map<int, std::vector<int>> edges_by_depth;
    for (std::size_t p = 0; p < frame.pieces().size(); ++p) {
        if (frame.arms(static_cast<int>(p)) != 2) continue;
        edges_by_depth[frame.depth_of(static_cast<int>(p))].push_back(
            static_cast<int>(p));
    }

    for (const auto& [depth, group] : edges_by_depth) {
        int flip = 0;
        for (const int p : group) {
            const auto& here = frame.pieces()[static_cast<std::size_t>(p)];
            const auto& from = frame.pieces()[static_cast<std::size_t>(
                home[static_cast<std::size_t>(p)])];
            for (std::size_t m = 0; m < from.size(); ++m) {
                if (came_from[static_cast<std::size_t>(here[0])] == from[m]) {
                    flip += static_cast<int>(m);
                }
            }
        }
        if (flip % 2 == 0) continue;

        out.fault = PaintFault::EdgeFlip;
        for (const int p : group) {
            const auto& here = frame.pieces()[static_cast<std::size_t>(p)];
            out.blamed.insert(out.blamed.end(), here.begin(), here.end());
        }
        return out;
    }

    // The one rule that belongs to the three by three alone. Bigger cubes swap
    // a pair of wings with an inner slice while disturbing only centres nobody
    // can tell apart, so there is nothing here to observe -- and asking anyway
    // would refuse half the cubes that really do solve.
    if (size == 3) {
        const auto renumbered = [&](const std::vector<int>& among) {
            std::map<int, int> place;
            for (std::size_t i = 0; i < among.size(); ++i) {
                place[among[i]] = static_cast<int>(i);
            }
            std::vector<int> perm(among.size(), 0);
            for (std::size_t i = 0; i < among.size(); ++i) {
                perm[i] = place.at(home[static_cast<std::size_t>(among[i])]);
            }
            return perm;
        };

        const auto& edges = edges_by_depth.at(1);
        if (parity_of(renumbered(corners)) != parity_of(renumbered(edges))) {
            out.fault = PaintFault::Permutation;
            for (const int p : corners) {
                const auto& here = frame.pieces()[static_cast<std::size_t>(p)];
                out.blamed.insert(out.blamed.end(), here.begin(), here.end());
            }
            return out;
        }
    }

    return out;
}

}  // namespace

std::vector<FaceColor> painting_of(const CubeState& cube)
{
    std::vector<FaceColor> out;
    for (const auto& s : surface_stickers(cube.size())) {
        out.push_back(cube.at(s.x, s.y, s.z).sticker(s.face));
    }
    return out;
}

PaintReading read_painting(int size, const std::vector<FaceColor>& painting)
{
    PaintReading refused;
    refused.fault = PaintFault::ColourCount;
    if (size < 2 || size > 64) return refused;

    const Frame& frame = frame_for(size);
    if (painting.size() != frame.slots().size()) return refused;

    // Every way up, because how somebody held their cube is their business.
    // When none of them answers, the one with the least to complain about is
    // the one to show: a single mis-copied sticker reads as one bad piece in
    // the orientation the painter used and as a ruined cube in the other
    // twenty-three, and the shortest complaint is the useful one.
    PaintReading best;
    bool have = false;
    for (std::size_t r = 0; r < frame.rotations(); ++r) {
        PaintReading reading = read_held(frame, held(frame, painting, r));
        reading.orientation = static_cast<int>(r);
        if (reading.fault == PaintFault::None) return reading;
        if (!have || reading.blamed.size() < best.blamed.size()) {
            best = std::move(reading);
            have = true;
        }
    }
    return best;
}

std::optional<CubeState> assembled(int size,
                                   const std::vector<FaceColor>& painting)
{
    const PaintReading reading = read_painting(size, painting);
    if (reading.fault != PaintFault::None) return std::nullopt;

    const Frame& frame = frame_for(size);
    const auto turned = held(frame, painting,
                             static_cast<std::size_t>(reading.orientation));

    std::vector<Cubie> cubies(
        static_cast<std::size_t>(size) * static_cast<std::size_t>(size) *
            static_cast<std::size_t>(size),
        solved_cubie());
    for (std::size_t i = 0; i < turned.size(); ++i) {
        const auto& s = frame.slots()[i];
        const auto at = static_cast<std::size_t>((s.x * size + s.y) * size +
                                                 s.z);
        cubies[at].stickers[face_index(s.face)] = turned[i];
    }
    return CubeState{size, std::move(cubies)};
}

}  // namespace rubiks::cube
