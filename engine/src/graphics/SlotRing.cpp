#include "graphics/SlotRing.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace rubiks::graphics {
namespace {

using cube::SurfaceSticker;
using math::Vec2;

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

[[nodiscard]] Vec2 unit(const Vec2& v) noexcept
{
    const float magnitude = length_of(v);
    if (magnitude <= 0.0f) return Vec2{1.0f, 0.0f};
    return scaled(v, 1.0f / magnitude);
}

}  // namespace

float arc_arm(float chord, float turn) noexcept
{
    if (turn < 1e-4f) return chord / 3.0f;
    return (4.0f / 3.0f) * std::tan(0.25f * turn) * chord /
           (2.0f * std::sin(0.5f * turn));
}

SlotRing::SlotRing(std::vector<Knot> knots, std::vector<std::size_t> slot_knots,
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

std::optional<std::size_t> SlotRing::slot_of(
    const SurfaceSticker& sticker) const noexcept
{
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (slots_[i] == sticker) return i;
    }
    return std::nullopt;
}

RingPoint SlotRing::at(float slot) const noexcept
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

RenderStroke SlotRing::stroke(float width, const Color& color) const
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

}  // namespace rubiks::graphics
