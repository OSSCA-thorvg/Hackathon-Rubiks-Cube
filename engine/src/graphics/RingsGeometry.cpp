#include "graphics/RingsGeometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "graphics/Palette.hpp"

namespace rubiks::graphics {
namespace {

using cube::Axis;
using cube::SurfaceSticker;
using cube::axis_of;
using cube::coordinate_on;
using cube::outer_layer;
using math::Vec2;

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegreesPerQuarterTurn = 90.0f;

/** The three axes, in the order their centres are placed around the middle. */
constexpr std::array<Axis, 3> kAxes{Axis::X, Axis::Y, Axis::Z};

[[nodiscard]] std::size_t axis_index(Axis axis) noexcept
{
    switch (axis) {
        case Axis::X:
            return 0;
        case Axis::Y:
            return 1;
        case Axis::Z:
            break;
    }
    return 2;
}

[[nodiscard]] Vec2 subtracted(const Vec2& a, const Vec2& b) noexcept
{
    return Vec2{a.x - b.x, a.y - b.y};
}

[[nodiscard]] float length_of(const Vec2& v) noexcept
{
    return std::sqrt(v.x * v.x + v.y * v.y);
}

[[nodiscard]] bool in_layers(cube::LayerMask layers, int index) noexcept
{
    return (layers & cube::layer(index)) != 0;
}

/**
 * Where an axis's circles are centred, in the diagram's own coordinates.
 *
 * A third of a turn apart with the first pointing up, written out rather than
 * computed: they are constants of the layout, and every slot placement asks
 * for three of them.
 */
[[nodiscard]] Vec2 axis_center(Axis axis) noexcept
{
    constexpr float kRoot3Over2 = 0.8660254038f;
    constexpr float kAcross[3] = {0.0f, -kRoot3Over2, kRoot3Over2};
    constexpr float kUp[3] = {1.0f, -0.5f, -0.5f};

    const std::size_t index = axis_index(axis);
    return Vec2{kRingCenterDistance * kAcross[index],
                kRingCenterDistance * kUp[index]};
}

[[nodiscard]] std::optional<Vec2> slot_in_diagram(const SurfaceSticker& sticker,
                                                  int size) noexcept;

/**
 * How close the two nearest slots come on a cube of `size`, in diagram units.
 *
 * The one measure everything in the diagram is sized against -- sticker,
 * stroke and the reach of a press all follow it -- and it is a property of the
 * layout rather than a setting. It was a constant while there was one size to
 * be laid out; with more, a written number would be right for one cube and
 * would have the others drawing over themselves or rattling around.
 *
 * Worked out by placing every slot and finding the closest pair, because the
 * slots are the crossings of circles and are nowhere near evenly spaced: which
 * pair turns out to be the closest is not something a formula would carry
 * across sizes. Once per size, into a table, so the frames only ever read it.
 */
[[nodiscard]] float slot_spacing(int size)
{
    // A cube whose layers no longer fit in a mask cannot be drawn either, so
    // the table covers every size that could reach here.
    constexpr int kSizes = 32;
    static std::array<float, kSizes> spacing{};

    if (size < 2 || size >= kSizes) return 1.0f;
    if (spacing[static_cast<std::size_t>(size)] > 0.0f) {
        return spacing[static_cast<std::size_t>(size)];
    }

    std::vector<Vec2> places;
    for (const auto& sticker : cube::surface_stickers(size)) {
        if (const auto at = slot_in_diagram(sticker, size)) {
            places.push_back(*at);
        }
    }

    float closest = 1.0f;
    for (std::size_t i = 0; i < places.size(); ++i) {
        for (std::size_t j = i + 1; j < places.size(); ++j) {
            closest = std::fmin(closest,
                                length_of(subtracted(places[i], places[j])));
        }
    }

    spacing[static_cast<std::size_t>(size)] = closest;
    return closest;
}

/** How far the whole drawing reaches, in its own units, with y running up. */
struct RingsBounds {
    float left;
    float right;
    float bottom;
    float top;
};

/**
 * The box the nine circles fill.
 *
 * Not a disc about the origin, which is what makes this worth working out: the
 * three centres are a third of a turn apart, so one of them points up and the
 * other two point down and out, and the figure reaches a good deal further one
 * way than the other. Fitting a rectangle to the wrong box leaves the drawing
 * sitting off to one side of it.
 */
[[nodiscard]] RingsBounds rings_bounds(int size)
{
    const float reach = 1.0f + kRingRadiusOffset +
                        0.5f * kRingsStrokeScale * kRingsActiveStrokeScale *
                            slot_spacing(size);

    RingsBounds bounds{0.0f, 0.0f, 0.0f, 0.0f};
    bool started = false;

    for (const Axis axis : kAxes) {
        const Vec2 at = axis_center(axis);
        if (!started) {
            bounds = RingsBounds{at.x - reach, at.x + reach, at.y - reach,
                                 at.y + reach};
            started = true;
            continue;
        }
        bounds.left = std::fmin(bounds.left, at.x - reach);
        bounds.right = std::fmax(bounds.right, at.x + reach);
        bounds.bottom = std::fmin(bounds.bottom, at.y - reach);
        bounds.top = std::fmax(bounds.top, at.y + reach);
    }
    return bounds;
}

/** How the diagram's own units land on the rectangle it is drawn in. */
struct RingsMetrics {
    /** Where the diagram's origin falls, in the drawing's pixels. */
    Vec2 origin;
    /** Pixels per diagram unit, one unit being the centre separation. */
    float scale;
    float sticker;
    float stroke;
};

[[nodiscard]] RingsMetrics rings_metrics(const Rect& rect, int size)
{
    const RingsBounds bounds = rings_bounds(size);
    const float spacing = slot_spacing(size);
    const float wide = bounds.right - bounds.left;
    const float tall = bounds.top - bounds.bottom;
    const float scale = std::fmin(rect.width / wide, rect.height / tall);

    // The middle of the box goes to the middle of the rectangle, so the origin
    // lands wherever that leaves it.
    const Vec2 middle{0.5f * (bounds.left + bounds.right),
                      0.5f * (bounds.bottom + bounds.top)};

    return RingsMetrics{
        Vec2{rect.x + 0.5f * rect.width - middle.x * scale,
             rect.y + 0.5f * rect.height + middle.y * scale},
        scale, spacing * kRingsStickerScale * scale,
        spacing * kRingsStrokeScale * scale};
}

/**
 * A point of the diagram in the pixels of the rectangle it fills.
 *
 * The vertical flip is the whole of the conversion: the layout is worked out
 * with y running up, the way the circles were laid out and checked, and the
 * drawing has it running down.
 */
[[nodiscard]] Vec2 to_screen(const Vec2& point,
                             const RingsMetrics& metrics) noexcept
{
    return Vec2{metrics.origin.x + point.x * metrics.scale,
                metrics.origin.y - point.y * metrics.scale};
}

/** Radius of one layer's circle, the middle layer's being exactly one. */
[[nodiscard]] float layer_radius(int layer, int size) noexcept
{
    if (size < 2) return 1.0f;

    const float along =
        static_cast<float>(layer) / static_cast<float>(size - 1);
    return 1.0f + kRingRadiusOffset * (2.0f * along - 1.0f);
}

/** Which side of the line from `a` to `b` a point falls on. */
[[nodiscard]] float side_of(const Vec2& point, const Vec2& a,
                            const Vec2& b) noexcept
{
    return (b.x - a.x) * (point.y - a.y) - (b.y - a.y) * (point.x - a.x);
}

/** The two points where two circles cross, or nothing when they do not. */
[[nodiscard]] std::optional<std::array<Vec2, 2>> crossings(
    const Vec2& first, float first_radius, const Vec2& second,
    float second_radius) noexcept
{
    const Vec2 between = subtracted(second, first);
    const float span = length_of(between);
    if (span <= 0.0f) return std::nullopt;

    const float along = (first_radius * first_radius -
                         second_radius * second_radius + span * span) /
                        (2.0f * span);
    const float square = first_radius * first_radius - along * along;
    if (square < 0.0f) return std::nullopt;

    const float across = std::sqrt(square);
    const Vec2 foot{first.x + along * between.x / span,
                    first.y + along * between.y / span};
    const Vec2 aside{-between.y / span, between.x / span};

    return std::array<Vec2, 2>{
        Vec2{foot.x + across * aside.x, foot.y + across * aside.y},
        Vec2{foot.x - across * aside.x, foot.y - across * aside.y}};
}

/**
 * Where a sticker rests, in the diagram's own coordinates.
 *
 * Two circles cross at two points, and the two stickers sharing that crossing
 * are the two faces of the axis neither circle belongs to. Which takes which is
 * settled by that third axis: its positive face takes the crossing on the far
 * side of the line joining the two centres from where the third centre is. That
 * one rule places all fifty-four, and it is what leaves every ring's slots in
 * the order the domain cycles them.
 */
[[nodiscard]] std::optional<Vec2> slot_in_diagram(const SurfaceSticker& sticker,
                                                  int size) noexcept
{
    const Axis free = axis_of(sticker.face);

    Axis first = Axis::X;
    Axis second = Axis::X;
    bool found_first = false;
    for (const Axis axis : kAxes) {
        if (axis == free) continue;
        if (!found_first) {
            first = axis;
            found_first = true;
            continue;
        }
        second = axis;
    }

    const Vec2 first_center = axis_center(first);
    const Vec2 second_center = axis_center(second);
    const auto met =
        crossings(first_center, layer_radius(coordinate_on(first, sticker), size),
                  second_center,
                  layer_radius(coordinate_on(second, sticker), size));
    if (!met) return std::nullopt;

    const float toward =
        side_of(axis_center(free), first_center, second_center);
    const bool positive_face = outer_layer(sticker.face, size) == size - 1;
    const float wanted = positive_face ? -toward : toward;

    return side_of((*met)[0], first_center, second_center) * wanted > 0.0f
               ? (*met)[0]
               : (*met)[1];
}

/** The square a sticker fills, centred on a point and square to the screen. */
[[nodiscard]] RenderFace sticker_quad(const Vec2& center, float side) noexcept
{
    const float half = 0.5f * side;

    RenderFace quad;
    quad.points = {Vec2{center.x - half, center.y - half},
                   Vec2{center.x + half, center.y - half},
                   Vec2{center.x + half, center.y + half},
                   Vec2{center.x - half, center.y + half}};
    return quad;
}

/**
 * A sticker as an index, for looking its place up rather than working it out.
 *
 * Sparse -- only 6N^2 of the 6N^3 slots are ever a sticker -- which costs a
 * hundred and eight unused entries on a 3x3x3 and saves searching for one.
 */
[[nodiscard]] std::size_t sticker_slots(int size) noexcept
{
    const auto span = static_cast<std::size_t>(size);
    return cube::kFaceCount * span * span * span;
}

[[nodiscard]] std::size_t sticker_key(const SurfaceSticker& sticker,
                                      int size) noexcept
{
    const auto span = static_cast<std::size_t>(size);
    return ((cube::face_index(sticker.face) * span +
             static_cast<std::size_t>(sticker.x)) *
                span +
            static_cast<std::size_t>(sticker.y)) *
               span +
           static_cast<std::size_t>(sticker.z);
}

/** Where a sticker ends up after `times` positive quarter turns. */
[[nodiscard]] SurfaceSticker turned_times(SurfaceSticker sticker, Axis axis,
                                          int size, int times) noexcept
{
    const int steps = ((times % 4) + 4) % 4;
    for (int step = 0; step < steps; ++step) {
        sticker = cube::turned_sticker(sticker, axis, size);
    }
    return sticker;
}

/**
 * One ring's slots as the drawing places them, with the way round they run.
 *
 * Both the loop and the reverse lookup need exactly this, and they have to
 * agree about the sense or a drag would read backwards from what it moves.
 */
struct ScreenRing {
    std::vector<cube::SurfaceSticker> slots;
    std::vector<Vec2> points;
    Vec2 center{};
    /** +1 when the domain's slot order runs the way angles increase. */
    float sense = 1.0f;
};

[[nodiscard]] std::optional<ScreenRing> screen_ring(Axis axis, int layer,
                                                    const Rect& rect, int size)
{
    ScreenRing ring;
    ring.slots = cube::ring_slots(axis, layer, size);
    if (ring.slots.size() < 3) return std::nullopt;

    const RingsMetrics metrics = rings_metrics(rect, size);
    ring.center = to_screen(axis_center(axis), metrics);

    ring.points.reserve(ring.slots.size());
    for (const auto& slot : ring.slots) {
        const auto placed = slot_in_diagram(slot, size);
        if (!placed) return std::nullopt;
        ring.points.push_back(to_screen(*placed, metrics));
    }

    // Read from the whole loop rather than from one pair, which a wide step
    // could turn the wrong way round.
    float swept = 0.0f;
    for (std::size_t i = 0; i < ring.points.size(); ++i) {
        const Vec2 here = subtracted(ring.points[i], ring.center);
        const Vec2 next =
            subtracted(ring.points[(i + 1) % ring.points.size()], ring.center);
        swept += std::atan2(here.x * next.y - here.y * next.x,
                            here.x * next.x + here.y * next.y);
    }
    ring.sense = swept >= 0.0f ? 1.0f : -1.0f;
    return ring;
}

/** How far it is from one angle to another, travelling in `sense`. */
[[nodiscard]] float forward_angle(float from, float to, float sense) noexcept
{
    float step = (to - from) * sense;
    while (step < 0.0f) step += 2.0f * kPi;
    while (step >= 2.0f * kPi) step -= 2.0f * kPi;
    return step;
}

/** The shorter way round from one angle to another, in radians. */
[[nodiscard]] float toward_angle(float from, float to) noexcept
{
    float turn = to - from;
    while (turn <= -kPi) turn += 2.0f * kPi;
    while (turn > kPi) turn -= 2.0f * kPi;
    return turn;
}

}  // namespace

std::optional<math::Vec2> rings_slot_position(const SurfaceSticker& sticker,
                                              const Rect& rect, int size)
{
    const auto placed = slot_in_diagram(sticker, size);
    if (!placed) return std::nullopt;
    return to_screen(*placed, rings_metrics(rect, size));
}

math::Vec2 rings_axis_center(Axis axis, const Rect& rect, int size)
{
    return to_screen(axis_center(axis), rings_metrics(rect, size));
}

float rings_slot_spacing(const Rect& rect, int size)
{
    return slot_spacing(size) * rings_metrics(rect, size).scale;
}

std::optional<math::Vec2> rings_slot_tangent(Axis axis, int layer,
                                             const Rect& rect, int size,
                                             const SurfaceSticker& sticker)
{
    const auto ring = screen_ring(axis, layer, rect, size);
    if (!ring) return std::nullopt;

    for (std::size_t slot = 0; slot < ring->slots.size(); ++slot) {
        if (!(ring->slots[slot] == sticker)) continue;

        // A circle's tangent is its radius turned a right angle, the way the
        // slots run.
        const Vec2 out = subtracted(ring->points[slot], ring->center);
        const float reach = length_of(out);
        if (reach <= 0.0f) return std::nullopt;

        return Vec2{-out.y * ring->sense / reach, out.x * ring->sense / reach};
    }
    return std::nullopt;
}

SlotRing rings_ring(Axis axis, int layer, const Rect& rect, int size)
{
    auto ring = screen_ring(axis, layer, rect, size);
    if (!ring) return SlotRing{};

    const auto& points = ring->points;
    const Vec2 center = ring->center;
    const float sense = ring->sense;

    // A knot on every slot, and more between them where the gap is wide.
    //
    // The slots are nowhere near evenly spaced -- fourteen degrees apart at the
    // tightest and better than a hundred and sixty at the loosest -- and a
    // single cubic cannot hold half a circle to its shape. Splitting the wide
    // gaps keeps every piece a shallow arc, which is where the one-cubic-per-arc
    // approximation is exact to a fraction of a pixel.
    std::vector<SlotRing::Knot> knots;
    std::vector<std::size_t> slot_knots(points.size());
    knots.reserve(points.size() * 3);

    const auto angle_at = [&](const Vec2& point) {
        return std::atan2(point.y - center.y, point.x - center.x);
    };
    const auto on_circle = [&](float angle, float reach) {
        const Vec2 at{center.x + reach * std::cos(angle),
                      center.y + reach * std::sin(angle)};
        // A circle's tangent is its radius turned a right angle, the way the
        // slots run. The curve into a point and out of it is the same one, so
        // there is no corner anywhere on this loop.
        const Vec2 tangent{-std::sin(angle) * sense, std::cos(angle) * sense};
        return SlotRing::Knot{at, tangent, tangent};
    };

    for (std::size_t i = 0; i < points.size(); ++i) {
        const float here = angle_at(points[i]);
        const float reach = length_of(subtracted(points[i], center));

        slot_knots[i] = knots.size();
        knots.push_back(on_circle(here, reach));

        const std::size_t after = (i + 1) % points.size();
        const float step =
            sense * forward_angle(here, angle_at(points[after]), sense);

        const auto pieces = static_cast<int>(
            std::ceil(std::abs(step) / (0.5f * kPi) - 1e-3f));
        for (int piece = 1; piece < pieces; ++piece) {
            knots.push_back(on_circle(
                here + step * static_cast<float>(piece) /
                           static_cast<float>(pieces),
                reach));
        }
    }

    return SlotRing{std::move(knots), std::move(slot_knots),
                    std::move(ring->slots)};
}

std::optional<float> rings_slot_at(Axis axis, int layer, const Rect& rect,
                                   int size, const math::Vec2& point)
{
    const auto ring = screen_ring(axis, layer, rect, size);
    if (!ring) return std::nullopt;

    const auto angle_at = [&](const Vec2& at) {
        return std::atan2(at.y - ring->center.y, at.x - ring->center.x);
    };
    const float wanted = angle_at(point);

    // Which pair of slots the point lies between, and how far along it is.
    // The spans add up to a whole revolution, so exactly one of them holds it.
    for (std::size_t i = 0; i < ring->points.size(); ++i) {
        const float here = angle_at(ring->points[i]);
        const float span = forward_angle(
            here, angle_at(ring->points[(i + 1) % ring->points.size()]),
            ring->sense);
        const float offset = forward_angle(here, wanted, ring->sense);

        if (offset < span) {
            return static_cast<float>(i) + (span > 0.0f ? offset / span : 0.0f);
        }
    }
    return 0.0f;
}

RenderScene build_rings_scene(const cube::CubeState& state, const Rect& rect,
                              Palette palette)
{
    return build_rings_scene(state, rect, std::nullopt, {}, palette);
}

RenderScene build_rings_scene(const cube::CubeState& state, const Rect& rect,
                              const std::optional<ActiveRotation>& active,
                              const std::vector<RingsGuide>& guides,
                              Palette palette)
{
    const int size = state.size();
    const RingsMetrics metrics = rings_metrics(rect, size);
    const auto stickers = cube::surface_stickers(size);

    RenderScene scene;
    scene.underlays.reserve(static_cast<std::size_t>(3 * size));
    scene.faces.reserve(stickers.size());

    // Where every sticker rests, worked out once. These are the same fifty-four
    // answers a resting sticker, a turning face's middle and that face's
    // destination all ask for, so asking the layout each time meant computing
    // each of them two or three times a frame.
    std::vector<Vec2> resting(sticker_slots(size));
    std::vector<char> placed(resting.size(), 0);
    for (const auto& sticker : stickers) {
        const auto at = slot_in_diagram(sticker, size);
        if (!at) continue;

        const std::size_t key = sticker_key(sticker, size);
        resting[key] = to_screen(*at, metrics);
        placed[key] = 1;
    }

    const auto place = [&](const SurfaceSticker& sticker) -> const Vec2* {
        const std::size_t key = sticker_key(sticker, size);
        return placed[key] != 0 ? &resting[key] : nullptr;
    };

    // The nine loops, built once. They are what the diagram is drawn on and
    // what a band rides, and building them twice for that was the single
    // largest thing a frame did.
    std::vector<SlotRing> rings;
    rings.reserve(static_cast<std::size_t>(3 * size));
    for (const Axis axis : kAxes) {
        for (int layer = 0; layer < size; ++layer) {
            rings.push_back(rings_ring(axis, layer, rect, size));
        }
    }
    const auto ring_of = [&](Axis axis, int layer) -> const SlotRing& {
        return rings[axis_index(axis) * static_cast<std::size_t>(size) +
                     static_cast<std::size_t>(layer)];
    };

    for (const Axis axis : kAxes) {
        for (int layer = 0; layer < size; ++layer) {
            const SlotRing& ring = ring_of(axis, layer);
            if (ring.empty()) continue;

            // Heavier for a ring a turn is running along, and for one a press
            // is holding out as somewhere it could go.
            const bool carrying =
                (active && active->axis == axis &&
                 in_layers(active->layers, layer)) ||
                std::any_of(guides.begin(), guides.end(),
                            [&](const RingsGuide& guide) {
                                return guide.axis == axis &&
                                       guide.layer == layer;
                            });
            scene.underlays.push_back(ring.stroke(
                metrics.stroke * (carrying ? kRingsActiveStrokeScale : 1.0f),
                guide_color(axis)));
        }
    }

    const auto emit = [&](const SurfaceSticker& sticker, const Vec2& at,
                          std::vector<RenderFace>& into) {
        RenderFace quad = sticker_quad(at, metrics.sticker);
        quad.color = to_color(
            state.at(sticker.x, sticker.y, sticker.z).sticker(sticker.face),
            palette);
        into.push_back(quad);
    };

    if (!active) {
        for (const auto& sticker : stickers) {
            if (const Vec2* at = place(sticker)) emit(sticker, *at, scene.faces);
        }
        return scene;
    }

    const float progress = active->angle_degrees / kDegreesPerQuarterTurn;

    // A turn past a whole quarter is that many finished turns and a part of
    // one. Taking the finished ones off first is what keeps a sticker landing
    // exactly on a slot at every right angle, however far the drag was carried.
    const auto whole = static_cast<int>(std::floor(progress));
    const float part = progress - static_cast<float>(whole);

    // What a turning face's stickers turn about: the middle of its own nine as
    // they rest, which does not move while they do. Only the faces the turn
    // actually spins have one, which for a middle layer is none of them.
    std::array<Vec2, cube::kFaceCount> middles{};
    for (const cube::Face face : cube::faces()) {
        if (axis_of(face) != active->axis) continue;
        if (!in_layers(active->layers, outer_layer(face, size))) continue;

        Vec2 total{0.0f, 0.0f};
        int counted = 0;
        for (const auto& sticker : stickers) {
            if (sticker.face != face) continue;
            const Vec2* at = place(sticker);
            if (!at) continue;

            total = Vec2{total.x + at->x, total.y + at->y};
            ++counted;
        }
        if (counted == 0) continue;

        const auto share = static_cast<float>(counted);
        middles[cube::face_index(face)] =
            Vec2{total.x / share, total.y / share};
    }

    // Pieces the turn is carrying wait here until every resting sticker has
    // been drawn, so a moving one passes over the diagram rather than under it.
    std::vector<RenderFace> moving;

    for (const auto& sticker : stickers) {
        const Vec2* at_rest = place(sticker);
        if (!at_rest) continue;

        // The band is every sticker the turn carries onto another face: those
        // off the turning axis whose layer the turn takes.
        const bool band = axis_of(sticker.face) != active->axis &&
                          in_layers(active->layers,
                                    coordinate_on(active->axis, sticker));

        if (band) {
            const SlotRing& ring =
                ring_of(active->axis, coordinate_on(active->axis, sticker));
            const auto slot = ring.slot_of(sticker);
            if (!slot) {
                emit(sticker, *at_rest, scene.faces);
                continue;
            }

            // A quarter turn is a quarter of the way round, which for a ring of
            // 4N slots is N of them.
            const float along = static_cast<float>(*slot) +
                                progress * static_cast<float>(size);
            emit(sticker, ring.at(along).position, moving);
            continue;
        }

        const bool turning_face =
            axis_of(sticker.face) == active->axis &&
            in_layers(active->layers, outer_layer(sticker.face, size));

        if (!turning_face) {
            emit(sticker, *at_rest, scene.faces);
            continue;
        }

        // The face on the axis turns about the middle of its own nine. It
        // cannot do so rigidly: the nine sit where three circles cross three
        // others, a curved grid with a mirror symmetry and no four-fold one, so
        // a rigid quarter turn misses the slots they are due by nearly half the
        // cluster. Carrying the angle and the reach separately, from the slot
        // it leaves to the slot it takes, reads as that same rotation and
        // arrives where the cube says it should.
        const Vec2* from = place(turned_times(sticker, active->axis, size,
                                              whole));
        const Vec2* to =
            place(turned_times(sticker, active->axis, size, whole + 1));
        if (!from || !to) {
            emit(sticker, *at_rest, scene.faces);
            continue;
        }

        const Vec2 middle = middles[cube::face_index(sticker.face)];
        const Vec2 out = subtracted(*from, middle);
        const Vec2 in = subtracted(*to, middle);

        const float angle = std::atan2(out.y, out.x);
        const float swept = toward_angle(angle, std::atan2(in.y, in.x)) * part;
        const float reach =
            length_of(out) + (length_of(in) - length_of(out)) * part;

        emit(sticker,
             Vec2{middle.x + reach * std::cos(angle + swept),
                  middle.y + reach * std::sin(angle + swept)},
             moving);
    }

    scene.faces.insert(scene.faces.end(), moving.begin(), moving.end());
    return scene;
}

}  // namespace rubiks::graphics
