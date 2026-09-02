#include "graphics/Passes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
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

/** Blinn-Phong specular strength of one lamp at a point of a face, as a byte. */
std::uint8_t specular_alpha(const Light& lamp, const math::Vec3& lamp_pos,
                            const math::Vec3& point,
                            const math::Vec3& normal) noexcept
{
    // The eye is the origin of view space, so the direction to it is -point.
    const math::Vec3 to_light = math::normalize(lamp_pos - point);
    const math::Vec3 to_eye = math::normalize(-point);
    const math::Vec3 half = to_light + to_eye;
    const float half_length = math::length(half);
    if (!(half_length > 0.0f)) return 0;

    const float cosine = std::max(math::dot(normal, half / half_length), 0.0f);
    const float strength = lamp.specular * std::pow(cosine, lamp.shininess);
    return static_cast<std::uint8_t>(
        std::clamp(std::lround(strength * 255.0f), 0L, 255L));
}

/**
 * The glint of one lamp on one plane: its peak on the plane, how far it
 * reaches and how strong it is. Worked out once per plane and lamp and shared
 * by every face on the plane -- the reach is found by bisection, which is not
 * something to do four thousand times a frame on a big cube.
 */
struct PlaneGlint {
    std::size_t lamp;
    math::Vec3 normal;
    float offset;
    std::optional<HighlightOf<math::Vec3>> glint;
    float radius;
};

}  // namespace

ViewScene LightPass::operator()(ViewScene scene) const
{
    std::vector<PlaneGlint> planes;

    // Distance from each lamp to the cube's centre: the distance at which its
    // diffuse term is at full strength.
    std::vector<float> reference;
    for (const auto& lamp_pos : lamps_in_view) {
        reference.push_back(math::length(lamp_pos - anchor_in_view));
    }

    for (auto& face : scene.faces) {
        const auto normal = outward_normal(face.points);
        if (!normal) continue;

        const auto& origin = face.points[0];
        const Color base = face.color;
        const math::Vec3 u = face.points[1] - origin;
        const float uu = math::dot(u, u);
        const math::Vec3 v = face.points[3] - origin;
        const float vv = math::dot(v, v);
        if (!(uu > 0.0f) || !(vv > 0.0f)) continue;

        // Diffuse: the sum over the lamps of Lambert's cosine, each falling
        // off with distance from its lamp, read at a point of the plane.
        const auto brightness = [&](const math::Vec3& point) {
            float total = lighting.ambient;
            for (std::size_t i = 0; i < lamps_in_view.size(); ++i) {
                const auto& lamp = lighting.lamps[i];
                if (!(lamp.diffuse > 0.0f)) continue;
                const math::Vec3 to_lamp = lamps_in_view[i] - point;
                const float distance = math::length(to_lamp);
                if (!(distance > 0.0f)) continue;
                const float cosine = math::dot(*normal, to_lamp) / distance;
                if (!(cosine > 0.0f)) continue;
                const float falloff =
                    lighting.attenuation > 0.0f && reference[i] > 0.0f
                        ? std::pow(reference[i] / distance, lighting.attenuation)
                        : 1.0f;
                total += lamp.diffuse * cosine * falloff;
            }
            return total;
        };

        // The reference shade: where the plane comes nearest the cube's
        // centre, the middle of the cube face for a sticker on the outside.
        const math::Vec3 anchor_foot =
            anchor_in_view - *normal * math::dot(anchor_in_view - origin, *normal);
        face.color = shade(base, brightness(anchor_foot));

        // Shading across the face, along the direction away from the key
        // light's foot on the plane, with the true brightness at both ends.
        // Neighbouring stickers on the plane share the function, so it runs
        // on across the seams.
        if (!lamps_in_view.empty() && lighting.lamps[0].diffuse > 0.0f) {
            const math::Vec3& key = lamps_in_view[0];
            const math::Vec3 key_foot =
                key - *normal * math::dot(key - origin, *normal);
            math::Vec3 centre{0.0f, 0.0f, 0.0f};
            for (const auto& point : face.points) centre += point;
            centre /= static_cast<float>(kCorners);

            const math::Vec3 away = centre - key_foot;
            const float away_length = math::length(away);
            if (away_length > 1e-4f) {
                const math::Vec3 direction = away / away_length;
                float near = 1e9f, far = -1e9f;
                for (const auto& point : face.points) {
                    const float t = math::dot(point - key_foot, direction);
                    near = std::min(near, t);
                    far = std::max(far, t);
                }
                const math::Vec3 from = key_foot + direction * near;
                const math::Vec3 to = key_foot + direction * far;
                const Color from_color = shade(base, brightness(from));
                const Color mid_color = shade(base, brightness((from + to) * 0.5f));
                const Color to_color = shade(base, brightness(to));
                // Flat only when it really is flat.
                if (from_color != to_color || from_color != mid_color) {
                    face.shading = ShadingOf<math::Vec3>{from, to, from_color,
                                                         mid_color, to_color};
                }
            }
        }

        // Specular, lamp by lamp: where the mirror image of the lamp, seen
        // from the eye, lands on this plane. Both have to be on the outer side
        // of the face for there to be anything to see.
        const float eye_height = math::dot(-origin, *normal);
        if (!(eye_height > 0.0f)) continue;
        const float offset = math::dot(origin, *normal);

        for (std::size_t i = 0; i < lamps_in_view.size(); ++i) {
            const auto& lamp = lighting.lamps[i];
            if (!(lamp.specular > 0.0f)) continue;
            const math::Vec3& lamp_pos = lamps_in_view[i];
            const float lamp_height = math::dot(lamp_pos - origin, *normal);
            if (!(lamp_height > 0.0f)) continue;

            PlaneGlint* plane = nullptr;
            for (auto& seen : planes) {
                if (seen.lamp == i && std::abs(seen.offset - offset) < 1e-4f &&
                    math::length(seen.normal - *normal) < 1e-4f) {
                    plane = &seen;
                    break;
                }
            }
            if (!plane) {
                planes.push_back(PlaneGlint{i, *normal, offset, std::nullopt, 0.0f});
                plane = &planes.back();

                const math::Vec3 mirrored = lamp_pos - *normal * (2.0f * lamp_height);
                // The segment from the mirrored lamp to the eye (the origin)
                // crosses the plane at this fraction of its length.
                const float t = lamp_height / (lamp_height + eye_height);
                const math::Vec3 peak = mirrored * (1.0f - t);
                const math::Vec3 along = u / std::sqrt(uu);

                const auto peak_alpha = specular_alpha(lamp, lamp_pos, peak, *normal);
                if (peak_alpha >= kHighlightMinAlpha) {
                    // The glint's radius is where the lobe has fallen to the
                    // threshold, found along the plane by bisection.
                    constexpr float kFarthest = 40.0f;
                    float inside = 0.0f, outside = kFarthest;
                    if (specular_alpha(lamp, lamp_pos, peak + along * kFarthest,
                                       *normal) >= kHighlightMinAlpha) {
                        inside = kFarthest;
                    } else {
                        for (int step = 0; step < 24; ++step) {
                            const float probe = 0.5f * (inside + outside);
                            if (specular_alpha(lamp, lamp_pos, peak + along * probe,
                                               *normal) >= kHighlightMinAlpha) {
                                inside = probe;
                            } else {
                                outside = probe;
                            }
                        }
                    }
                    if (inside > 0.0f) {
                        plane->radius = inside;
                        plane->glint = HighlightOf<math::Vec3>{
                            peak, peak + along * inside, peak_alpha,
                            specular_alpha(lamp, lamp_pos,
                                           peak + along * (0.5f * inside), *normal)};
                    }
                }
            }
            if (!plane->glint) continue;

            // Only stickers the glint reaches carry it: the nearest point of
            // the sticker to the peak is within the radius.
            const math::Vec3& peak = plane->glint->centre;
            const float a = std::clamp(math::dot(peak - origin, u) / uu, 0.0f, 1.0f);
            const float b = std::clamp(math::dot(peak - origin, v) / vv, 0.0f, 1.0f);
            const math::Vec3 nearest = origin + u * a + v * b;
            if (math::length(nearest - peak) >= plane->radius) continue;

            face.highlights.push_back(*plane->glint);
        }
    }
    return scene;
}

LightPass light(const Lighting& lighting, const Camera& camera) noexcept
{
    const auto view = camera.view();
    LightPass pass{lighting, {}, to_vec3(math::apply_point(view, kCubeCenter))};
    for (const auto& lamp : lighting.lamps) {
        pass.lamps_in_view.push_back(
            to_vec3(math::apply_point(view, lamp.position)));
    }
    return pass;
}

WorldScene ShadowPass::operator()(WorldScene scene) const
{
    scene.shadow.reset();
    if (!key || scene.casters.empty()) return scene;
    const math::Vec3& light_pos = key->position;

    // Seen from underneath, a floating object's ground shadow is not there;
    // it thins out over the last stretch above the ground rather than
    // vanishing at it.
    const float presence =
        std::clamp((eye.y - ground_y) / kShadowFadeBand, 0.0f, 1.0f);
    if (!(presence > 0.0f)) return scene;

    // Onto the plane y = ground_y, along the ray from the light.
    const auto cast = [&](const math::Vec3& point) {
        const float t = (ground_y - light_pos.y) / (point.y - light_pos.y);
        return light_pos + (point - light_pos) * t;
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
            [&](const math::Vec3& corner) { return corner.y < light_pos.y; });
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
    shadow.fade_end = shadow.fade_start;

    // The fade runs from under the cube to the shadow's own tip: the point of
    // the outline furthest along the direction the light throws it. At the tip
    // it is gone, so wherever a clip cuts it there is nothing left to cut.
    if (centre.y < light_pos.y) {
        const math::Vec3 thrown = cast(centre) - shadow.fade_start;
        const float thrown_length = math::length(thrown);
        if (thrown_length > 1e-4f) {
            const math::Vec3 direction = thrown / thrown_length;
            float reach = 0.0f;
            for (const auto& polygon : shadow.polygons) {
                for (const auto& point : polygon) {
                    reach = std::max(
                        reach, math::dot(point - shadow.fade_start, direction));
                }
            }
            shadow.fade_end = shadow.fade_start + direction * reach;
        }
    }

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

ShadowPass shadow(const Lighting& lighting, const Camera& camera,
                  float ground_y) noexcept
{
    std::optional<Light> key;
    if (!lighting.lamps.empty()) key = lighting.lamps.front();
    return ShadowPass{key, ground_y, camera.eye()};
}

}  // namespace rubiks::graphics
