#include "graphics/NetRing.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "graphics/NetGeometry.hpp"

namespace rubiks::graphics {
namespace {

using cube::Axis;
using cube::SurfaceSticker;
using math::Vec2;

/** Points closer together than this are the same point. */
constexpr float kTouching = 1e-3f;

[[nodiscard]] float length_of(const Vec2& v) noexcept
{
    return std::sqrt(v.x * v.x + v.y * v.y);
}

[[nodiscard]] Vec2 scaled(const Vec2& v, float factor) noexcept
{
    return Vec2{v.x * factor, v.y * factor};
}

[[nodiscard]] Vec2 added(const Vec2& a, const Vec2& b) noexcept
{
    return Vec2{a.x + b.x, a.y + b.y};
}

[[nodiscard]] Vec2 subtracted(const Vec2& a, const Vec2& b) noexcept
{
    return Vec2{a.x - b.x, a.y - b.y};
}

[[nodiscard]] float dot_product(const Vec2& a, const Vec2& b) noexcept
{
    return a.x * b.x + a.y * b.y;
}

[[nodiscard]] float cross_product(const Vec2& a, const Vec2& b) noexcept
{
    return a.x * b.y - a.y * b.x;
}

[[nodiscard]] Vec2 unit(const Vec2& v) noexcept
{
    const float magnitude = length_of(v);
    if (magnitude <= 0.0f) return Vec2{1.0f, 0.0f};
    return scaled(v, 1.0f / magnitude);
}

/** Where the loop leaves the drawing, given a direction of travel. */
[[nodiscard]] Vec2 beyond_edge(const Vec2& point, const Vec2& direction,
                               const Rect& rect, float reach) noexcept
{
    Vec2 out = point;
    if (direction.x < 0.0f) out.x = rect.x - reach;
    if (direction.x > 0.0f) out.x = rect.x + rect.width + reach;
    if (direction.y < 0.0f) out.y = rect.y - reach;
    if (direction.y > 0.0f) out.y = rect.y + rect.height + reach;
    return out;
}

/**
 * A point of the polyline the loop is built from.
 *
 * Slots are passed through; corners are cut away by an arc and never end up on
 * the finished curve at all.
 */
struct PathPoint {
    Vec2 point;
    bool corner;
    std::size_t slot;
};

}  // namespace

float arc_arm(float chord, float turn) noexcept
{
    if (turn < 1e-4f) return chord / 3.0f;
    return (4.0f / 3.0f) * std::tan(0.25f * turn) * chord /
           (2.0f * std::sin(0.5f * turn));
}

NetRing::NetRing(std::vector<Knot> knots, std::vector<std::size_t> slot_knots,
                 std::vector<SurfaceSticker> slots)
    : knots_(std::move(knots)),
      slot_knots_(std::move(slot_knots)),
      slots_(std::move(slots))
{
    segments_.reserve(knots_.size());

    for (std::size_t knot = 0; knot < knots_.size(); ++knot) {
        const auto& from = knots_[knot];
        const auto& to = knots_[(knot + 1) % knots_.size()];

        const float chord = length_of(subtracted(to.point, from.point));
        const float turn = std::acos(std::clamp(
            dot_product(from.out_direction, to.in_direction), -1.0f, 1.0f));
        const float arm = arc_arm(chord, turn);

        segments_.push_back(RenderSegment{
            added(from.point, scaled(from.out_direction, arm)),
            subtracted(to.point, scaled(to.in_direction, arm)), to.point});
    }
}

std::optional<std::size_t> NetRing::slot_of(
    const SurfaceSticker& sticker) const noexcept
{
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (slots_[i] == sticker) return i;
    }
    return std::nullopt;
}

RingPoint NetRing::at(float slot) const noexcept
{
    if (slots_.empty()) return RingPoint{};

    const auto slot_count = static_cast<float>(slots_.size());
    float position = std::fmod(slot, slot_count);
    if (position < 0.0f) position += slot_count;

    const auto index = static_cast<std::size_t>(position);
    const float fraction = position - static_cast<float>(index);

    const std::size_t knot_count = knots_.size();
    const std::size_t from = slot_knots_[index];
    const std::size_t to = slot_knots_[(index + 1) % slots_.size()];
    std::size_t span = (to + knot_count - from) % knot_count;
    if (span == 0) span = knot_count;

    // A step between slots may run over more than one knot where the loop
    // rounds a corner. Sharing the step out by how long each part is keeps a
    // cell moving at one speed through the bend.
    const auto chord = [&](std::size_t knot) {
        const auto& start = knots_[knot % knot_count].point;
        const auto& end = knots_[(knot + 1) % knot_count].point;
        return length_of(subtracted(end, start));
    };

    float total = 0.0f;
    for (std::size_t step = 0; step < span; ++step) total += chord(from + step);

    std::size_t knot = from;
    float local = fraction;
    if (total > 0.0f) {
        float remaining = fraction * total;
        for (std::size_t step = 0; step < span; ++step) {
            const float part = chord(from + step);
            if (remaining <= part || step + 1 == span) {
                knot = from + step;
                local = part > 0.0f ? remaining / part : 0.0f;
                break;
            }
            remaining -= part;
        }
    }
    knot %= knot_count;
    local = std::fmin(std::fmax(local, 0.0f), 1.0f);

    const Vec2 p0 = knots_[knot].point;
    const Vec2 p1 = segments_[knot].control_a;
    const Vec2 p2 = segments_[knot].control_b;
    const Vec2 p3 = segments_[knot].to;

    const float rest = 1.0f - local;
    const float a = rest * rest * rest;
    const float b = 3.0f * rest * rest * local;
    const float c = 3.0f * rest * local * local;
    const float d = local * local * local;

    const Vec2 point{a * p0.x + b * p1.x + c * p2.x + d * p3.x,
                     a * p0.y + b * p1.y + c * p2.y + d * p3.y};

    const float da = 3.0f * rest * rest;
    const float db = 6.0f * rest * local;
    const float dc = 3.0f * local * local;
    const Vec2 slope{da * (p1.x - p0.x) + db * (p2.x - p1.x) +
                         dc * (p3.x - p2.x),
                     da * (p1.y - p0.y) + db * (p2.y - p1.y) +
                         dc * (p3.y - p2.y)};

    // Where a corner's arc reaches all the way to its slots the run between
    // them has no length left, so there is no slope to read and the knot's own
    // direction is the answer.
    if (length_of(slope) <= 0.0f) {
        return RingPoint{point, knots_[knot].out_direction};
    }
    return RingPoint{point, unit(slope)};
}

RenderStroke NetRing::stroke(float width, const Color& color) const
{
    RenderStroke path;
    if (knots_.empty()) return path;

    path.start = knots_.front().point;
    path.closed = true;
    path.width = width;
    path.color = color;
    path.segments = segments_;
    return path;
}

namespace {

/** The loop through a given cycle of slots, laid over the net in `rect`. */
[[nodiscard]] NetRing ring_through(const std::vector<SurfaceSticker>& slots,
                                   const Rect& rect, int size)
{
    const std::size_t count = slots.size();
    if (count < 2) return NetRing{};

    std::vector<Vec2> centers;
    centers.reserve(count);
    for (const auto& slot : slots) {
        const auto placed = net_position(slot, size);
        if (!placed) return NetRing{};
        centers.push_back(net_cell_center(placed->face, placed->col,
                                          placed->row, rect, size));
    }

    const auto next_of = [&](std::size_t i) { return (i + 1) % count; };
    const auto previous_of = [&](std::size_t i) {
        return (i + count - 1) % count;
    };
    const auto same_face = [&](std::size_t a, std::size_t b) {
        return slots[a].face == slots[b].face;
    };

    // The way the band runs at each slot, taken from the neighbour on its own
    // face so it is always one of the drawn directions. The two disagree only
    // where a face's own run turns on a sticker, which is what a turning
    // face's ring does and a band's never does.
    std::vector<Vec2> leaving(count);
    std::vector<Vec2> arriving(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t after = next_of(i);
        const std::size_t before = previous_of(i);

        leaving[i] = same_face(i, after)
                         ? unit(subtracted(centers[after], centers[i]))
                         : unit(subtracted(centers[i], centers[before]));
        arriving[i] = same_face(before, i)
                          ? unit(subtracted(centers[i], centers[before]))
                          : unit(subtracted(centers[after], centers[i]));
    }

    const float reach = kNetBreakReachCells * net_cell_side(rect, size);

    // The polyline: every slot, and a corner wherever the loop has to change
    // direction between two of them.
    std::vector<PathPoint> path;
    path.reserve(count + 8);

    for (std::size_t i = 0; i < count; ++i) {
        path.push_back(PathPoint{centers[i], false, i});

        const std::size_t after = next_of(i);
        const Vec2 step = subtracted(centers[after], centers[i]);
        const Vec2 out = leaving[i];
        const Vec2 in = arriving[after];

        // Already running the right way and already pointing at the next slot:
        // one straight piece, nothing to round.
        if (std::abs(cross_product(out, in)) < kTouching &&
            dot_product(out, in) > 0.0f &&
            std::abs(cross_product(step, out)) < kTouching &&
            dot_product(step, out) > 0.0f) {
            continue;
        }

        // A quarter turn between two faces the cross draws side by side: the
        // corner is where the two runs would meet if they carried on.
        if (std::abs(dot_product(out, in)) < kTouching) {
            const Vec2 corner = std::abs(out.x) > std::abs(out.y)
                                    ? Vec2{centers[after].x, centers[i].y}
                                    : Vec2{centers[i].x, centers[after].y};
            path.push_back(PathPoint{corner, true, 0});
            continue;
        }

        // Otherwise the cross has cut the band here, so the loop leaves the
        // drawing and comes back where the next face is.
        const Vec2 away = beyond_edge(centers[i], out, rect, reach);
        const Vec2 back =
            beyond_edge(centers[after], scaled(in, -1.0f), rect, reach);

        path.push_back(PathPoint{away, true, 0});

        // Where it leaves and re-enters going the same way -- a row that wraps
        // -- the way straight back would lie along the band itself. It is
        // lifted clear of the drawing instead, over the empty margin rather
        // than across the faces stacked beside the row.
        if (dot_product(subtracted(back, away), out) < 0.0f) {
            const Vec2 aside{std::abs(out.y), -std::abs(out.x)};
            path.push_back(
                PathPoint{beyond_edge(away, aside, rect, reach), true, 0});
            path.push_back(
                PathPoint{beyond_edge(back, aside, rect, reach), true, 0});
        }

        path.push_back(PathPoint{back, true, 0});
    }

    // One radius for the whole loop, as wide as its tightest corner allows.
    //
    // Cutting every corner as wide as its own neighbours leave room for gives
    // the widest rounding the drawing allows, but not the same rounding: where
    // a band is cut the loop has whole faces of empty margin to bend through,
    // while the corner that drops it back onto a slot has one cell. A loop
    // rounded three cells at one end and half a cell at the other does not
    // read as one shape. So the tightest corner sets them all.
    //
    // The rings the cross draws around the front block are unchanged by this:
    // their corners are equally cramped already, which is what gives the front
    // layer's loop a cell of rounding and the back layer's three.
    float radius = 0.0f;
    bool bounded = false;

    for (std::size_t i = 0; i < path.size(); ++i) {
        if (!path[i].corner) continue;

        const auto& before = path[(i + path.size() - 1) % path.size()];
        const auto& after = path[(i + 1) % path.size()];

        // A corner shares its room with a neighbouring corner, and takes all
        // of it from a slot, which the loop has to reach.
        const float room = std::fmin(
            length_of(subtracted(path[i].point, before.point)) *
                (before.corner ? 0.5f : 1.0f),
            length_of(subtracted(after.point, path[i].point)) *
                (after.corner ? 0.5f : 1.0f));

        radius = bounded ? std::fmin(radius, room) : room;
        bounded = true;
    }

    // Where the room reaches exactly to a slot -- which is what the cross's own
    // geometry gives every ring around the front block -- the arc begins on
    // that slot and the run between them vanishes.
    std::vector<NetRing::Knot> knots;
    std::vector<std::size_t> slot_knots(count);
    knots.reserve(path.size() * 2);

    const auto add_knot = [&](const Vec2& point, const Vec2& into,
                              const Vec2& out_of, bool is_slot,
                              std::size_t slot) {
        if (!knots.empty() &&
            length_of(subtracted(knots.back().point, point)) < kTouching) {
            knots.back().out_direction = out_of;
        } else {
            knots.push_back(NetRing::Knot{point, into, out_of});
        }
        if (is_slot) slot_knots[slot] = knots.size() - 1;
    };

    for (std::size_t i = 0; i < path.size(); ++i) {
        const auto& here = path[i];

        if (!here.corner) {
            add_knot(here.point, arriving[here.slot], leaving[here.slot], true,
                     here.slot);
            continue;
        }

        const auto& before = path[(i + path.size() - 1) % path.size()];
        const auto& after = path[(i + 1) % path.size()];

        const Vec2 into = unit(subtracted(here.point, before.point));
        const Vec2 out_of = unit(subtracted(after.point, here.point));

        add_knot(subtracted(here.point, scaled(into, radius)), into, into,
                 false, 0);
        add_knot(added(here.point, scaled(out_of, radius)), out_of, out_of,
                 false, 0);
    }

    // The loop's last knot may be the first one over again, where the walk
    // began part way along a run that a corner then reached back to.
    if (knots.size() > 1 &&
        length_of(subtracted(knots.front().point, knots.back().point)) <
            kTouching) {
        knots.front().in_direction = knots.back().in_direction;
        const std::size_t dropped = knots.size() - 1;
        for (auto& knot : slot_knots) {
            if (knot == dropped) knot = 0;
        }
        knots.pop_back();
    }

    return NetRing{std::move(knots), std::move(slot_knots), slots};
}

}  // namespace

NetRing net_ring(Axis axis, int layer, const Rect& rect, int size)
{
    return ring_through(cube::ring_slots(axis, layer, size), rect, size);
}

}  // namespace rubiks::graphics
