#include "graphics/Passes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "graphics/ConvexHull.hpp"

namespace rubiks::graphics {
namespace {

constexpr std::size_t kCorners = 4;
constexpr float kPi = 3.14159265358979323846f;
constexpr int kContactPoints = 16;

math::Vec3 to_vec3(const math::Vec4& v) noexcept
{
    return math::Vec3{v.x, v.y, v.z};
}

/**
 * Outward unit normal of a face, or nullopt for a degenerate one.
 *
 * Faces are wound counter-clockwise seen from outside, which is what the cull
 * pass reads as "front"; by the right-hand rule the cross product of two
 * consecutive edges of such a winding points at the viewer, so this is the
 * same convention read the other way round.
 */
std::optional<math::Vec3> outward_normal(
    const std::array<math::Vec3, kCorners>& points) noexcept
{
    const math::Vec3 normal = math::cross(points[1] - points[0],
                                          points[2] - points[0]);
    const float length = math::length(normal);
    if (!(length > 0.0f)) return std::nullopt;
    return normal / length;
}

/** Blinn-Phong specular strength at a point of a face, as a byte. */
std::uint8_t specular_alpha(const Light& light, const math::Vec3& light_pos,
                            const math::Vec3& point,
                            const math::Vec3& normal) noexcept
{
    // The eye is the origin of view space, so the direction to it is -point.
    const math::Vec3 to_light = math::normalize(light_pos - point);
    const math::Vec3 to_eye = math::normalize(-point);
    const math::Vec3 half = to_light + to_eye;
    const float half_length = math::length(half);
    if (!(half_length > 0.0f)) return 0;

    const float cosine = std::max(math::dot(normal, half / half_length), 0.0f);
    const float strength =
        light.specular * std::pow(cosine, light.shininess);
    return static_cast<std::uint8_t>(
        std::clamp(std::lround(strength * 255.0f), 0L, 255L));
}

}  // namespace

ViewScene LightPass::operator()(ViewScene scene) const
{
    for (auto& face : scene.faces) {
        const auto normal = outward_normal(face.points);
        if (!normal) continue;

        // Diffuse: one value for the whole plane, read where the plane comes
        // nearest the cube's centre -- the middle of the cube face, for a
        // sticker on the outside.
        const math::Vec3 foot =
            anchor_in_view -
            *normal * math::dot(anchor_in_view - face.points[0], *normal);
        const math::Vec3 to_light = math::normalize(light_in_view - foot);
        const float lambert = std::max(math::dot(*normal, to_light), 0.0f);
        face.color = shade(face.color, light.ambient + light.diffuse * lambert);

        // Specular: where the mirror image of the light, seen from the eye,
        // lands on this plane. Both have to be on the outer side of the face
        // for there to be anything to see.
        const auto& origin = face.points[0];
        const float light_height = math::dot(light_in_view - origin, *normal);
        const float eye_height = math::dot(-origin, *normal);
        if (!(light_height > 0.0f) || !(eye_height > 0.0f)) continue;

        const math::Vec3 mirrored = light_in_view - *normal * (2.0f * light_height);
        // The segment from the mirrored light to the eye (the origin) crosses
        // the plane at this fraction of its length.
        const float t = light_height / (light_height + eye_height);
        const math::Vec3 on_plane = mirrored * (1.0f - t);

        // Pulled onto the face if it fell outside, then re-evaluated there,
        // so a glint just off a face's edge shows faintly at that edge and a
        // glint far from it shows nothing.
        const math::Vec3 u = face.points[1] - origin;
        const math::Vec3 v = face.points[3] - origin;
        const float uu = math::dot(u, u);
        const float vv = math::dot(v, v);
        if (!(uu > 0.0f) || !(vv > 0.0f)) continue;

        const float a = std::clamp(math::dot(on_plane - origin, u) / uu, 0.0f, 1.0f);
        const float b = std::clamp(math::dot(on_plane - origin, v) / vv, 0.0f, 1.0f);
        const math::Vec3 on_face = origin + u * a + v * b;

        const auto alpha = specular_alpha(light, light_in_view, on_face, *normal);
        if (alpha < kHighlightMinAlpha) continue;

        face.highlight = HighlightOf<math::Vec3>{
            on_face, on_face + u * (kHighlightRadius / std::sqrt(uu)), alpha};
    }
    return scene;
}

LightPass light(const Light& light, const Camera& camera) noexcept
{
    const auto view = camera.view();
    return LightPass{light, to_vec3(math::apply_point(view, light.position)),
                     to_vec3(math::apply_point(view, kCubeCenter))};
}

WorldScene ShadowPass::operator()(WorldScene scene) const
{
    scene.shadow.reset();
    if (scene.casters.empty()) return scene;

    // Seen from underneath, a floating object's ground shadow is not there;
    // it thins out over the last stretch above the ground rather than
    // vanishing at it.
    const float presence =
        std::clamp((eye.y - ground_y) / kShadowFadeBand, 0.0f, 1.0f);
    if (!(presence > 0.0f)) return scene;

    // Onto the plane y = ground_y, along the ray from the light.
    const auto cast = [this](const math::Vec3& point) {
        const float t = (ground_y - light.position.y) / (point.y - light.position.y);
        return light.position + (point - light.position) * t;
    };

    ShadowGroup<math::Vec3> shadow;
    math::Vec3 centre{0.0f, 0.0f, 0.0f};
    float half_extent = 0.0f;
    std::size_t corner_count = 0;

    for (const auto& caster : scene.casters) {
        // A corner level with the light or above it has no shadow on the
        // ground; the box goes rather than a torn part of it.
        const bool below_light = std::all_of(
            caster.corners.begin(), caster.corners.end(),
            [this](const math::Vec3& corner) {
                return corner.y < light.position.y;
            });
        if (!below_light) continue;

        std::vector<math::Vec2> footprints;
        footprints.reserve(caster.corners.size());
        for (const auto& corner : caster.corners) {
            const math::Vec3 on_ground = cast(corner);
            footprints.push_back(math::Vec2{on_ground.x, on_ground.z});
            centre += corner;
            ++corner_count;
        }

        std::vector<math::Vec3> polygon;
        for (const auto& point : convex_hull(std::move(footprints))) {
            polygon.push_back(math::Vec3{point.x, ground_y, point.y});
        }
        shadow.polygons.push_back(std::move(polygon));
        shadow.occluders.push_back(caster.corners);
    }
    if (shadow.polygons.empty()) return scene;

    centre /= static_cast<float>(corner_count);
    for (const auto& caster : scene.casters) {
        for (const auto& corner : caster.corners) {
            half_extent = std::max(
                {half_extent, std::abs(corner.x - centre.x),
                 std::abs(corner.z - centre.z)});
        }
    }

    shadow.fade_start = math::Vec3{centre.x, ground_y, centre.z};
    shadow.fade_end =
        centre.y < light.position.y ? cast(centre) : shadow.fade_start;

    const float radius = kContactRadiusShare * half_extent;
    shadow.contact.reserve(kContactPoints);
    for (int i = 0; i < kContactPoints; ++i) {
        const float angle = 2.0f * kPi * static_cast<float>(i) / kContactPoints;
        shadow.contact.push_back(
            math::Vec3{shadow.fade_start.x + radius * std::cos(angle), ground_y,
                       shadow.fade_start.z + radius * std::sin(angle)});
    }

    shadow.opacity = static_cast<std::uint8_t>(
        std::lround(static_cast<float>(kShadowOpacity) * presence));
    scene.shadow = std::move(shadow);
    return scene;
}

ShadowPass shadow(const Light& light, const Camera& camera,
                  float ground_y) noexcept
{
    return ShadowPass{light, ground_y, camera.eye()};
}

}  // namespace rubiks::graphics
