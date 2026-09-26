#include "graphics/AxisGizmo.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

#include "cube/CubeMove.hpp"
#include "graphics/Palette.hpp"
#include "math/Types.hpp"

namespace rubiks::graphics {
namespace {

using cube::Axis;
using math::Vec2;
using math::Vec3;

/** One arm, kept with its view-space depth so the near ones can go last. */
struct Arm {
    Axis axis;
    Vec2 tip;
    float depth;
};

}  // namespace

namespace {

/** The three arms out of one point, as long and as thick as they are asked. */
RenderScene draw_arms(const Camera& camera, Vec2 origin, float arm_length,
                      float line_width)
{
    RenderScene scene;
    const math::Mat4 view = camera.view();

    constexpr std::array<std::pair<Axis, Vec3>, 3> kAxes{
        {{Axis::X, Vec3{1.0f, 0.0f, 0.0f}},
         {Axis::Y, Vec3{0.0f, 1.0f, 0.0f}},
         {Axis::Z, Vec3{0.0f, 0.0f, 1.0f}}}};

    std::array<Arm, 3> arms{};
    for (std::size_t i = 0; i < kAxes.size(); ++i) {
        // A direction, so w is zero and the eye's position drops out: what is
        // left is the viewpoint's rotation, which is all a gizmo reports.
        const auto turned =
            math::apply(view, math::Vec4{kAxes[i].second, 0.0f});

        // Screen Y runs down, so the view's up becomes a step up the canvas.
        arms[i] = Arm{kAxes[i].first,
                      Vec2{origin.x + turned.x * arm_length,
                           origin.y - turned.y * arm_length},
                      turned.z};
    }

    // Furthest first: the camera looks down its own -Z, so a larger z is nearer
    // and an arm pointing away goes under the ones pointing toward.
    std::stable_sort(arms.begin(), arms.end(),
                     [](const Arm& a, const Arm& b) { return a.depth < b.depth; });

    scene.strokes.reserve(arms.size());
    for (const auto& arm : arms) {
        RenderStroke line;
        line.start = origin;
        line.closed = false;
        line.width = line_width;
        line.color = guide_color(arm.axis);

        // A straight cubic: the controls a third and two thirds along, which
        // is the same segment the renderer draws every other path with.
        line.segments.push_back(RenderSegment{
            Vec2{origin.x + (arm.tip.x - origin.x) / 3.0f,
                 origin.y + (arm.tip.y - origin.y) / 3.0f},
            Vec2{origin.x + 2.0f * (arm.tip.x - origin.x) / 3.0f,
                 origin.y + 2.0f * (arm.tip.y - origin.y) / 3.0f},
            arm.tip});
        scene.strokes.push_back(std::move(line));
    }
    return scene;
}

}  // namespace

RenderScene build_axis_gizmo(const Camera& camera, const Rect& region)
{
    if (region.width <= 0.0f || region.height <= 0.0f) return RenderScene{};

    const float side = std::min(region.width, region.height);
    const float inset = kAxisGizmoInsetShare * side;
    const Vec2 origin{region.x + region.width - inset, region.y + inset};
    return draw_arms(camera, origin, kAxisGizmoArmShare * side,
                     kAxisGizmoWidthShare * side);
}

RenderScene build_axis_badge(const Camera& camera, const Rect& surface)
{
    if (surface.width <= 0.0f || surface.height <= 0.0f) return RenderScene{};

    const float side = std::min(surface.width, surface.height);
    const Vec2 origin{surface.x + surface.width * 0.5f,
                      surface.y + surface.height * 0.5f};
    return draw_arms(camera, origin, kAxisBadgeArmShare * side,
                     kAxisBadgeWidthShare * side);
}

}  // namespace rubiks::graphics
