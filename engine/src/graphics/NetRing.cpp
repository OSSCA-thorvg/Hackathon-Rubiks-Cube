#include "graphics/NetRing.hpp"

#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "cube/Surface.hpp"
#include "graphics/NetGeometry.hpp"

namespace rubiks::graphics {
namespace {

using cube::Axis;
using cube::SurfaceSticker;
using math::Vec2;

/** Points closer together than this are the same point. */
constexpr float kTouching = 1e-3f;

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


/** The loop through a given cycle of slots, laid over the net in `rect`. */
[[nodiscard]] SlotRing ring_through(const std::vector<SurfaceSticker>& slots,
                                   const Rect& rect, int size)
{
    const std::size_t count = slots.size();
    if (count < 2) return SlotRing{};

    std::vector<Vec2> centers;
    centers.reserve(count);
    for (const auto& slot : slots) {
        const auto placed = net_position(slot, size);
        if (!placed) return SlotRing{};
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
                         ? math::unit(centers[after] - centers[i])
                         : math::unit(centers[i] - centers[before]);
        arriving[i] = same_face(before, i)
                          ? math::unit(centers[i] - centers[before])
                          : math::unit(centers[after] - centers[i]);
    }

    const float reach = kNetBreakReachCells * net_cell_side(rect, size);

    // The polyline: every slot, and a corner wherever the loop has to change
    // direction between two of them.
    std::vector<PathPoint> path;
    path.reserve(count + 8);

    for (std::size_t i = 0; i < count; ++i) {
        path.push_back(PathPoint{centers[i], false, i});

        const std::size_t after = next_of(i);
        const Vec2 step = centers[after] - centers[i];
        const Vec2 out = leaving[i];
        const Vec2 in = arriving[after];

        // Already running the right way and already pointing at the next slot:
        // one straight piece, nothing to round.
        if (std::abs(math::cross(out, in)) < kTouching &&
            math::dot(out, in) > 0.0f &&
            std::abs(math::cross(step, out)) < kTouching &&
            math::dot(step, out) > 0.0f) {
            continue;
        }

        // A quarter turn between two faces the cross draws side by side: the
        // corner is where the two runs would meet if they carried on.
        if (std::abs(math::dot(out, in)) < kTouching) {
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
            beyond_edge(centers[after], in * -1.0f, rect, reach);

        path.push_back(PathPoint{away, true, 0});

        // Where it leaves and re-enters going the same way -- a row that wraps
        // -- the way straight back would lie along the band itself. It is
        // lifted clear of the drawing instead, over the empty margin rather
        // than across the faces stacked beside the row.
        if (math::dot(back - away, out) < 0.0f) {
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
            math::distance(before.point, path[i].point) *
                (before.corner ? 0.5f : 1.0f),
            math::distance(path[i].point, after.point) *
                (after.corner ? 0.5f : 1.0f));

        radius = bounded ? std::fmin(radius, room) : room;
        bounded = true;
    }

    // Where the room reaches exactly to a slot -- which is what the cross's own
    // geometry gives every ring around the front block -- the arc begins on
    // that slot and the run between them vanishes.
    std::vector<SlotRing::Knot> knots;
    std::vector<std::size_t> slot_knots(count);
    knots.reserve(path.size() * 2);

    const auto add_knot = [&](const Vec2& point, const Vec2& into,
                              const Vec2& out_of, bool is_slot,
                              std::size_t slot) {
        if (!knots.empty() &&
            math::distance(point, knots.back().point) < kTouching) {
            knots.back().out_direction = out_of;
        } else {
            knots.push_back(SlotRing::Knot{point, into, out_of});
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

        const Vec2 into = math::unit(here.point - before.point);
        const Vec2 out_of = math::unit(after.point - here.point);

        add_knot(here.point - into * radius, into, into, false, 0);
        add_knot(here.point + out_of * radius, out_of, out_of, false, 0);
    }

    // The loop's last knot may be the first one over again, where the walk
    // began part way along a run that a corner then reached back to.
    if (knots.size() > 1 &&
        math::distance(knots.back().point, knots.front().point) < kTouching) {
        knots.front().in_direction = knots.back().in_direction;
        const std::size_t dropped = knots.size() - 1;
        for (auto& knot : slot_knots) {
            if (knot == dropped) knot = 0;
        }
        knots.pop_back();
    }

    return SlotRing{std::move(knots), std::move(slot_knots), slots};
}

}  // namespace

SlotRing net_ring(Axis axis, int layer, const Rect& rect, int size)
{
    return ring_through(cube::ring_slots(axis, layer, size), rect, size);
}

}  // namespace rubiks::graphics
