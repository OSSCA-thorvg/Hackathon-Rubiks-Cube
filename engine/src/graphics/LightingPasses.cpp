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

/** Lambert's cosine at in-plane distance `rho` from the foot of a light `height` above the plane. */
float lambert_at(float height, float rho) noexcept
{
    return height / std::sqrt(height * height + rho * rho);
}

/**
 * The glint of one plane: its peak on the plane, how far it reaches and how
 * strong it is. Worked out once per plane and shared by every face on it --
 * the reach is found by bisection, which is not something to do four
 * thousand times a frame on a big cube.
 */
struct PlaneGlint {
    math::Vec3 normal;
    float offset;
    std::optional<HighlightOf<math::Vec3>> glint;  // rim is peak + along * radius
    math::Vec3 along;
    float radius;
};

ViewScene LightPass::operator()(ViewScene scene) const
{
    std::vector<PlaneGlint> planes;

    for (auto& face : scene.faces) {
        const auto normal = outward_normal(face.points);
        if (!normal) continue;

        const auto& origin = face.points[0];
        const float light_height = math::dot(light_in_view - origin, *normal);

        // Diffuse. On a plane, a point light's cosine depends only on how far
        // along the plane a point is from the light's foot -- so it is one
        // function for every sticker on that plane, read here at two points
        // of each sticker and at the plane's point nearest the cube's centre.
        const math::Vec3 anchor_foot =
            anchor_in_view - *normal * math::dot(anchor_in_view - origin, *normal);
        const Color base = face.color;

        if (!(light_height > 0.0f)) {
            // Lit from behind or edge-on: ambient everywhere, flat.
            face.color = shade(base, light.ambient);
            continue;
        }

        const math::Vec3 light_foot = light_in_view - *normal * light_height;
        const auto brightness = [&](float rho) {
            return light.ambient + light.diffuse * lambert_at(light_height, rho);
        };
        face.color = shade(base, brightness(math::length(anchor_foot - light_foot)));

        math::Vec3 centre{0.0f, 0.0f, 0.0f};
        for (const auto& point : face.points) centre += point;
        centre /= static_cast<float>(kCorners);

        const math::Vec3 away = centre - light_foot;
        const float away_length = math::length(away);
        if (away_length > 1e-4f && light.diffuse > 0.0f) {
            const math::Vec3 direction = away / away_length;
            float near = 1e9f, far = -1e9f;
            for (const auto& point : face.points) {
                const float t = math::dot(point - light_foot, direction);
                near = std::min(near, t);
                far = std::max(far, t);
            }
            const Color near_color = shade(base, brightness(std::abs(near)));
            const Color far_color = shade(base, brightness(std::abs(far)));
            // A sticker too small for the function to move a channel by more
            // than one across it is drawn flat: within the contract's
            // tolerance and indistinguishable by eye, for a fraction of the
            // cost -- which matters on a 28x28 with three thousand of them.
            const auto apart = [](std::uint8_t a, std::uint8_t b) {
                return std::abs(int{a} - int{b}) > 1;
            };
            if (apart(near_color.r, far_color.r) || apart(near_color.g, far_color.g) ||
                apart(near_color.b, far_color.b)) {
                face.shading = ShadingOf<math::Vec3>{
                    light_foot + direction * near, light_foot + direction * far,
                    near_color, far_color};
            }
        }

        // Specular: where the mirror image of the light, seen from the eye,
        // lands on this plane. Both have to be on the outer side of the face
        // for there to be anything to see.
        const float eye_height = math::dot(-origin, *normal);
        if (!(eye_height > 0.0f) || !(light.specular > 0.0f)) continue;

        const math::Vec3 u = face.points[1] - origin;
        const float uu = math::dot(u, u);
        const math::Vec3 v = face.points[3] - origin;
        const float vv = math::dot(v, v);
        if (!(uu > 0.0f) || !(vv > 0.0f)) continue;

        // The plane this face lies on, looked up among the ones already seen.
        const float offset = math::dot(origin, *normal);
        PlaneGlint* plane = nullptr;
        for (auto& seen : planes) {
            if (std::abs(seen.offset - offset) < 1e-4f &&
                math::length(seen.normal - *normal) < 1e-4f) {
                plane = &seen;
                break;
            }
        }
        if (!plane) {
            planes.push_back(PlaneGlint{*normal, offset, std::nullopt,
                                        u / std::sqrt(uu), 0.0f});
            plane = &planes.back();

            const math::Vec3 mirrored =
                light_in_view - *normal * (2.0f * light_height);
            // The segment from the mirrored light to the eye (the origin)
            // crosses the plane at this fraction of its length.
            const float t = light_height / (light_height + eye_height);
            const math::Vec3 peak = mirrored * (1.0f - t);

            const auto peak_alpha =
                specular_alpha(light, light_in_view, peak, *normal);
            if (peak_alpha >= kHighlightMinAlpha) {
                // The glint's radius is where the lobe has fallen to the
                // threshold, found along the plane by bisection.
                constexpr float kFarthest = 40.0f;
                float inside = 0.0f, outside = kFarthest;
                if (specular_alpha(light, light_in_view,
                                   peak + plane->along * kFarthest,
                                   *normal) >= kHighlightMinAlpha) {
                    inside = kFarthest;
                } else {
                    for (int i = 0; i < 24; ++i) {
                        const float probe = 0.5f * (inside + outside);
                        if (specular_alpha(light, light_in_view,
                                           peak + plane->along * probe,
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
                        peak, peak + plane->along * inside, peak_alpha,
                        specular_alpha(light, light_in_view,
                                       peak + plane->along * (0.5f * inside),
                                       *normal)};
                }
            }
        }
        if (!plane->glint) continue;

        // Only stickers the glint reaches carry it: the nearest point of the
        // sticker to the peak is within the radius.
        const math::Vec3& peak = plane->glint->centre;
        const float a = std::clamp(math::dot(peak - origin, u) / uu, 0.0f, 1.0f);
        const float b = std::clamp(math::dot(peak - origin, v) / vv, 0.0f, 1.0f);
        const math::Vec3 nearest = origin + u * a + v * b;
        if (math::length(nearest - peak) >= plane->radius) continue;

        face.highlight = plane->glint;
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
    shadow.fade_end = shadow.fade_start;

    // The fade runs from under the cube to the shadow's own tip: the point of
    // the outline furthest along the direction the light throws it. At the tip
    // it is gone, so wherever a clip cuts it there is nothing left to cut.
    if (centre.y < light.position.y) {
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

ShadowPass shadow(const Light& light, const Camera& camera,
                  float ground_y) noexcept
{
    return ShadowPass{light, ground_y, camera.eye()};
}

}  // namespace rubiks::graphics
