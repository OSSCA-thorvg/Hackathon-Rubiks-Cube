#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"
#include "cube/Surface.hpp"
#include "graphics/Layout.hpp"
#include "graphics/Palette.hpp"
#include "graphics/RingsGeometry.hpp"

namespace {

using Catch::Approx;
using namespace rubiks::graphics;
using rubiks::cube::Axis;
using rubiks::cube::Face;
using rubiks::cube::SurfaceSticker;
using rubiks::math::Vec2;

constexpr int kSize = 3;
constexpr float kPi = 3.14159265358979323846f;

const Rect kRect{0.0f, 0.0f, 600.0f, 600.0f};

constexpr std::array<Axis, 3> kAxes{Axis::X, Axis::Y, Axis::Z};

/** A sticker as something orderable, for putting into sets and maps. */
[[nodiscard]] std::tuple<int, int, int, int> key_of(const SurfaceSticker& s)
{
    return {s.x, s.y, s.z, static_cast<int>(s.face)};
}

[[nodiscard]] float distance(const Vec2& a, const Vec2& b)
{
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

/** How far a ring's slots sit from the centre the layout gives it. */
[[nodiscard]] float ring_radius(Axis axis, int layer)
{
    const auto ring = rings_ring(axis, layer, kRect, kSize);
    return distance(ring.at(0.0f).position, rings_axis_center(axis, kRect));
}

/** The middle of a quad, which is where its sticker sits. */
[[nodiscard]] Vec2 middle_of(const RenderFace& quad)
{
    return Vec2{0.25f * (quad.points[0].x + quad.points[1].x +
                         quad.points[2].x + quad.points[3].x),
                0.25f * (quad.points[0].y + quad.points[1].y +
                         quad.points[2].y + quad.points[3].y)};
}

/** Every quad of a scene as its rounded corners and color, order dropped. */
[[nodiscard]] std::multiset<std::vector<int>> corner_set(
    const RenderScene& scene)
{
    std::multiset<std::vector<int>> quads;
    for (const auto& quad : scene.faces) {
        std::vector<int> shape;
        for (const auto& point : quad.points) {
            shape.push_back(static_cast<int>(std::lround(point.x * 20.0f)));
            shape.push_back(static_cast<int>(std::lround(point.y * 20.0f)));
        }
        shape.push_back(static_cast<int>(quad.color.r));
        shape.push_back(static_cast<int>(quad.color.g));
        shape.push_back(static_cast<int>(quad.color.b));
        quads.insert(shape);
    }
    return quads;
}

}  // namespace

TEST_CASE("every sticker sits on exactly two rings, of the two axes that are "
          "not its own")
{
    std::map<std::tuple<int, int, int, int>, std::vector<Axis>> memberships;

    std::size_t total = 0;
    for (const Axis axis : kAxes) {
        for (int layer = 0; layer < kSize; ++layer) {
            const auto slots = rubiks::cube::ring_slots(axis, layer, kSize);
            REQUIRE(slots.size() == 4 * kSize);
            total += slots.size();

            for (const auto& slot : slots) {
                memberships[key_of(slot)].push_back(axis);
            }
        }
    }

    // Nine rings of twelve, over fifty-four stickers, two apiece.
    REQUIRE(total == 108);
    REQUIRE(memberships.size() == 54);

    for (const auto& sticker : rubiks::cube::surface_stickers(kSize)) {
        const auto& axes = memberships.at(key_of(sticker));
        REQUIRE(axes.size() == 2);
        REQUIRE(axes[0] != axes[1]);

        // Not the axis its own face points along: that turn spins this sticker
        // where it is, or leaves it alone.
        const Axis own = rubiks::cube::axis_of(sticker.face);
        REQUIRE(axes[0] != own);
        REQUIRE(axes[1] != own);
    }
}

TEST_CASE("two rings of different axes share exactly two stickers")
{
    std::size_t pairs = 0;

    for (std::size_t first = 0; first < 9; ++first) {
        for (std::size_t second = first + 1; second < 9; ++second) {
            const Axis first_axis = kAxes[first / 3];
            const Axis second_axis = kAxes[second / 3];
            if (first_axis == second_axis) continue;

            const auto here = rubiks::cube::ring_slots(
                first_axis, static_cast<int>(first % 3), kSize);
            const auto there = rubiks::cube::ring_slots(
                second_axis, static_cast<int>(second % 3), kSize);

            std::set<std::tuple<int, int, int, int>> mine;
            for (const auto& slot : here) mine.insert(key_of(slot));

            std::size_t shared = 0;
            for (const auto& slot : there) {
                if (mine.count(key_of(slot)) != 0) ++shared;
            }
            REQUIRE(shared == 2);
            ++pairs;
        }
    }

    // Three pairs of axes, nine circle pairs each, two stickers apiece: 54.
    REQUIRE(pairs == 27);
}

TEST_CASE("the layout places fifty-four distinct slots, twelve to a ring")
{
    std::set<std::pair<int, int>> placed;

    for (const auto& sticker : rubiks::cube::surface_stickers(kSize)) {
        const auto at = rings_slot_position(sticker, kRect, kSize);
        REQUIRE(at.has_value());
        placed.insert({static_cast<int>(std::lround(at->x * 100.0f)),
                       static_cast<int>(std::lround(at->y * 100.0f))});
    }
    REQUIRE(placed.size() == 54);

    for (const Axis axis : kAxes) {
        for (int layer = 0; layer < kSize; ++layer) {
            const auto ring = rings_ring(axis, layer, kRect, kSize);
            REQUIRE(ring.slot_count() == 4 * kSize);
        }
    }
}

TEST_CASE("each ring's slots come round its own centre in the domain's order")
{
    // This is the condition the radii were chosen for. Get it wrong and a ring
    // still holds the right stickers, but the loop drawn through them doubles
    // back and crosses itself.
    for (const Axis axis : kAxes) {
        for (int layer = 0; layer < kSize; ++layer) {
            const auto ring = rings_ring(axis, layer, kRect, kSize);
            REQUIRE_FALSE(ring.empty());

            const Vec2 middle = rings_axis_center(axis, kRect);

            // Every step round the loop turns the same way about the centre,
            // and the whole way round is exactly one revolution.
            float swept = 0.0f;
            for (std::size_t slot = 0; slot < ring.slot_count(); ++slot) {
                const auto here = ring.at(static_cast<float>(slot)).position;
                const auto next =
                    ring.at(static_cast<float>((slot + 1) % ring.slot_count()))
                        .position;

                const Vec2 from{here.x - middle.x, here.y - middle.y};
                const Vec2 to{next.x - middle.x, next.y - middle.y};
                const float step =
                    std::atan2(from.x * to.y - from.y * to.x,
                               from.x * to.x + from.y * to.y);

                REQUIRE(std::abs(step) > 1e-3f);
                if (slot > 0) REQUIRE(step * swept > 0.0f);
                swept += step;
            }
            REQUIRE(std::abs(std::abs(swept) - 2.0f * kPi) < 1e-3f);
        }
    }
}

TEST_CASE("a ring's loop is the circle its slots lie on")
{
    for (const Axis axis : kAxes) {
        for (int layer = 0; layer < kSize; ++layer) {
            const auto ring = rings_ring(axis, layer, kRect, kSize);
            REQUIRE_FALSE(ring.empty());

            const Vec2 middle = rings_axis_center(axis, kRect);

            const float radius = distance(ring.at(0.0f).position, middle);

            // Between slots as well as on them: one knot per slot carrying that
            // circle's tangent makes every cubic an exact arc of it.
            for (int step = 0; step < 240; ++step) {
                const float along = static_cast<float>(step) / 20.0f;
                const float reach =
                    distance(ring.at(along).position, middle) - radius;
                REQUIRE(std::abs(reach) < 0.02f * radius);
            }
        }
    }
}

TEST_CASE("three slots along the ring is where the domain sends the sticker")
{
    for (const Axis axis : kAxes) {
        for (int layer = 0; layer < kSize; ++layer) {
            const auto slots = rubiks::cube::ring_slots(axis, layer, kSize);
            const auto ring = rings_ring(axis, layer, kRect, kSize);

            for (std::size_t slot = 0; slot < slots.size(); ++slot) {
                const auto turned =
                    rubiks::cube::turned_sticker(slots[slot], axis, kSize);
                REQUIRE(slots[(slot + kSize) % slots.size()] == turned);

                // And the drawing agrees: a whole quarter turn along the loop
                // lands on the very point that sticker rests at.
                const auto arrived =
                    ring.at(static_cast<float>(slot) + kSize).position;
                const auto resting =
                    rings_slot_position(turned, kRect, kSize);
                REQUIRE(resting.has_value());
                REQUIRE(distance(arrived, *resting) < 0.05f);
            }
        }
    }
}

TEST_CASE("a resting diagram draws every sticker and every ring")
{
    const rubiks::cube::CubeState state(kSize);
    const auto scene = build_rings_scene(state, kRect);

    REQUIRE(scene.faces.size() == 54);
    REQUIRE(scene.underlays.size() == 9);
    REQUIRE(scene.strokes.empty());

    // Nine faces of each color, and the rings under them rather than over.
    for (const Face face : rubiks::cube::faces()) {
        const auto color = to_color(rubiks::cube::solved_color(face));
        const auto shown = static_cast<std::size_t>(std::count_if(
            scene.faces.begin(), scene.faces.end(),
            [&](const RenderFace& quad) { return quad.color == color; }));
        REQUIRE(shown == 9);
    }
}

TEST_CASE("a turn drawn at nothing and at a quarter matches the cube itself")
{
    rubiks::cube::CubeState state(kSize);
    state.apply(rubiks::cube::CubeMove{Axis::Y, rubiks::cube::layer(1), 1});

    for (const Axis axis : kAxes) {
        for (int layer = 0; layer < kSize; ++layer) {
            const auto mask = rubiks::cube::layer(layer);

            const auto settled = build_rings_scene(state, kRect);
            const auto opening =
                build_rings_scene(state, kRect,
                                  ActiveRotation{axis, mask, 0.0f, 1.0f});
            REQUIRE(settled.faces.size() == opening.faces.size());

            // A turn at zero degrees is the resting drawing, corner for corner:
            // the middles alone would pass with a sticker left tilted. The
            // pieces a turn carries are drawn after the still ones, so the two
            // scenes hold the same quads in a different order.
            REQUIRE(corner_set(settled) == corner_set(opening));

            // And a whole quarter turn is the cube after that move, drawn at
            // rest: every sticker on the very slot the domain gives it.
            rubiks::cube::CubeState turned = state;
            turned.apply(rubiks::cube::CubeMove{axis, mask, 1});

            const auto landed =
                build_rings_scene(state, kRect,
                                  ActiveRotation{axis, mask, 90.0f, 1.0f});
            const auto after = build_rings_scene(turned, kRect);

            std::map<std::pair<int, int>, Color> shown;
            for (const auto& quad : landed.faces) {
                const auto at = middle_of(quad);
                shown[{static_cast<int>(std::lround(at.x)),
                       static_cast<int>(std::lround(at.y))}] = quad.color;
            }
            REQUIRE(shown.size() == 54);

            for (const auto& quad : after.faces) {
                const auto at = middle_of(quad);
                const auto found =
                    shown.find({static_cast<int>(std::lround(at.x)),
                                static_cast<int>(std::lround(at.y))});
                REQUIRE(found != shown.end());
                REQUIRE(found->second == quad.color);
            }
        }
    }
}

TEST_CASE("a band part way through a turn is on the ring it is riding")
{
    const rubiks::cube::CubeState state(kSize);

    for (const Axis axis : kAxes) {
        for (int layer = 0; layer < kSize; ++layer) {
            const auto ring = rings_ring(axis, layer, kRect, kSize);

            const Vec2 middle = rings_axis_center(axis, kRect);
            const float radius = ring_radius(axis, layer);

            const auto scene = build_rings_scene(
                state, kRect,
                ActiveRotation{axis, rubiks::cube::layer(layer), 37.0f, 1.0f});

            // The twelve this ring carries are the only stickers anywhere on
            // its circle, resting or moving: the other layers of this axis are
            // on circles of another radius, and the face it turns is on none of
            // this axis's at all.
            std::size_t on_the_ring = 0;
            for (const auto& quad : scene.faces) {
                if (std::abs(distance(middle_of(quad), middle) - radius) <
                    0.02f * radius) {
                    ++on_the_ring;
                }
            }
            REQUIRE(on_the_ring == 4 * kSize);
        }
    }
}

TEST_CASE("the rings a turn runs along are drawn heavier than the rest")
{
    const rubiks::cube::CubeState state(kSize);
    const auto scene =
        build_rings_scene(state, kRect,
                          ActiveRotation{Axis::Z, rubiks::cube::layer(2), 45.0f,
                                         1.0f});

    REQUIRE(scene.underlays.size() == 9);

    const auto heavier = static_cast<std::size_t>(std::count_if(
        scene.underlays.begin(), scene.underlays.end(),
        [&](const RenderStroke& path) {
            return path.width > scene.underlays.front().width * 1.5f;
        }));
    REQUIRE(heavier == 1);
}

TEST_CASE("the diagram stays inside the rectangle it is given")
{
    const rubiks::cube::CubeState state(kSize);

    for (const Rect rect : {Rect{0.0f, 0.0f, 600.0f, 600.0f},
                            Rect{40.0f, 90.0f, 500.0f, 320.0f},
                            Rect{10.0f, 10.0f, 220.0f, 700.0f}}) {
        const auto scene = build_rings_scene(
            state, rect,
            ActiveRotation{Axis::X, rubiks::cube::layer(0), 52.0f, 1.0f});

        for (const auto& quad : scene.faces) {
            for (const auto& point : quad.points) {
                REQUIRE(point.x >= rect.x - 0.5f);
                REQUIRE(point.x <= rect.x + rect.width + 0.5f);
                REQUIRE(point.y >= rect.y - 0.5f);
                REQUIRE(point.y <= rect.y + rect.height + 0.5f);
            }
        }
    }
}

TEST_CASE("a stack of flat views keeps them apart and inside the canvas")
{
    using rubiks::graphics::FlatStyle;
    using rubiks::graphics::ViewMode;

    const auto overlaps = [](const Rect& a, const Rect& b) {
        return a.x < b.x + b.width && b.x < a.x + a.width &&
               a.y < b.y + b.height && b.y < a.y + a.height;
    };
    const auto inside = [](const Rect& part, float width, float height) {
        return part.x >= 0.0f && part.y >= 0.0f &&
               part.x + part.width <= width + 0.5f &&
               part.y + part.height <= height + 0.5f;
    };

    for (const auto shape : {std::pair{1000U, 1000U}, std::pair{1400U, 700U},
                             std::pair{600U, 1200U}}) {
        const auto [width, height] = shape;

        // The net over the rings, and neither of them over the other.
        const auto flat = layout(width, height, ViewMode::Flat, FlatStyle::Both);
        REQUIRE(flat.net.height > 0.0f);
        REQUIRE(flat.rings.height > 0.0f);
        REQUIRE(flat.net.y + flat.net.height <= flat.rings.y);
        REQUIRE_FALSE(overlaps(flat.net, flat.rings));
        REQUIRE(inside(flat.net, static_cast<float>(width),
                       static_cast<float>(height)));
        REQUIRE(inside(flat.rings, static_cast<float>(width),
                       static_cast<float>(height)));

        // And with the cube as well, all three in that order.
        const auto all = layout(width, height, ViewMode::Both, FlatStyle::Both);
        REQUIRE(all.cube.height > 0.0f);
        REQUIRE(all.cube.y + all.cube.height <= all.net.y);
        REQUIRE(all.net.y + all.net.height <= all.rings.y);
        REQUIRE_FALSE(overlaps(all.cube, all.net));
        REQUIRE_FALSE(overlaps(all.net, all.rings));
        REQUIRE(inside(all.cube, static_cast<float>(width),
                       static_cast<float>(height)));
        REQUIRE(inside(all.rings, static_cast<float>(width),
                       static_cast<float>(height)));

        // The 3D region stays square, which is what pins the camera aspect.
        REQUIRE(all.cube.width == Approx(all.cube.height));
    }
}
