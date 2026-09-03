#include "graphics/Passes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <vector>

#include "graphics/ConvexHull.hpp"
#include "graphics/CubeGeometry.hpp"

namespace rubiks::graphics {
namespace {

constexpr std::size_t kCorners = 4;
/** Stops of a plane's shading gradient, the true brightness at each. */
constexpr std::size_t kShadingStops = 5;

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
 * The glint of one lamp on one plane: where its lobe lands, how far it
 * reaches each way and how strong it is. Worked out once per plane and lamp
 * and shared by every face on the plane -- the reach is found by bisection,
 * which is not something to do four thousand times a frame on a big cube.
 *
 * The footprint is an ellipse rather than a circle: a lobe seen at a slant
 * reaches further towards the eye than away from it, so its reach is measured
 * both ways along both of the plane's axes and the ellipse is fitted to the
 * four ends. That is also the shape every sticker is tested against, so a
 * sticker joins or leaves the glint exactly where the drawn ellipse does.
 */
struct PlaneGlint {
    std::size_t lamp;
    math::Vec3 normal;
    float offset;
    std::optional<HighlightOf<math::Vec3>> glint;
    /** The footprint's unit axes and semi-axes on the plane. */
    math::Vec3 axis_u{};
    math::Vec3 axis_v{};
    float semi_u = 0.0f;
    float semi_v = 0.0f;
};

/**
 * How far a lobe stays above the highlight threshold from `peak` along
 * `along`, found by bisection; the whole span when it never falls below.
 */
float lobe_reach(const Light& lamp, const math::Vec3& lamp_pos,
                 const math::Vec3& peak, const math::Vec3& along,
                 const math::Vec3& normal) noexcept
{
    constexpr float kFarthest = 40.0f;
    if (specular_alpha(lamp, lamp_pos, peak + along * kFarthest, normal) >=
        kHighlightMinAlpha) {
        return kFarthest;
    }
    float inside = 0.0f;
    float outside = kFarthest;
    for (int step = 0; step < 24; ++step) {
        const float probe = 0.5f * (inside + outside);
        if (specular_alpha(lamp, lamp_pos, peak + along * probe, normal) >=
            kHighlightMinAlpha) {
            inside = probe;
        } else {
            outside = probe;
        }
    }
    return inside;
}

}  // namespace

/**
 * The brightness function of one plane: an axis from the key light's foot
 * through the plane's point nearest the cube's centre, the span of every face
 * on the plane along it, and the brightness at evenly spaced stops. Worked
 * out once per plane so every sticker on it shades with the same function.
 */
struct PlaneShading {
    math::Vec3 normal;
    float offset;
    math::Vec3 foot;       // the key light's foot on the plane
    math::Vec3 direction;  // in-plane, away from the foot
    float near = 1e9f;
    float far = -1e9f;
    std::array<float, kShadingStops> brightness{};
    bool usable = false;
};

ViewScene LightPass::operator()(ViewScene scene) const
{
    std::vector<PlaneGlint> planes;
    std::vector<PlaneShading> shadings;

    // Distance from each lamp to the cube's centre: the distance at which its
    // diffuse term is at full strength.
    std::vector<float> reference;
    for (const auto& lamp_pos : lamps_in_view) {
        reference.push_back(math::length(lamp_pos - anchor_in_view));
    }

    // The sum over the lamps of Lambert's cosine, each falling off with
    // distance from its lamp, at a point of a plane with the given normal.
    const auto brightness = [&](const math::Vec3& point, const math::Vec3& normal) {
        float total = lighting.ambient;
        for (std::size_t i = 0; i < lamps_in_view.size(); ++i) {
            const auto& lamp = lighting.lamps[i];
            if (!(lamp.diffuse > 0.0f)) continue;
            const math::Vec3 to_lamp = lamps_in_view[i] - point;
            const float distance = math::length(to_lamp);
            if (!(distance > 0.0f)) continue;
            const float cosine = math::dot(normal, to_lamp) / distance;
            if (!(cosine > 0.0f)) continue;
            const float falloff =
                lighting.attenuation > 0.0f && reference[i] > 0.0f
                    ? std::pow(reference[i] / distance, lighting.attenuation)
                    : 1.0f;
            total += lamp.diffuse * cosine * falloff;
        }
        return total;
    };

    const bool shaded = !lamps_in_view.empty() && lighting.lamps[0].diffuse > 0.0f;

    // First, the planes: which faces share one, and how far each plane's
    // faces reach along its shading axis.
    for (const auto& face : scene.faces) {
        if (!shaded) break;
        const auto normal = outward_normal(face.points);
        if (!normal) continue;
        const float offset = math::dot(face.points[0], *normal);

        PlaneShading* plane = nullptr;
        for (auto& seen : shadings) {
            if (std::abs(seen.offset - offset) < 1e-4f &&
                math::length(seen.normal - *normal) < 1e-4f) {
                plane = &seen;
                break;
            }
        }
        if (!plane) {
            PlaneShading fresh;
            fresh.normal = *normal;
            fresh.offset = offset;
            const math::Vec3& key = lamps_in_view[0];
            fresh.foot = key - *normal * math::dot(key - face.points[0], *normal);
            const math::Vec3 anchor_foot =
                anchor_in_view - *normal * math::dot(anchor_in_view - face.points[0], *normal);
            const math::Vec3 away = anchor_foot - fresh.foot;
            const float away_length = math::length(away);
            fresh.usable = away_length > 1e-4f;
            if (fresh.usable) fresh.direction = away / away_length;
            shadings.push_back(fresh);
            plane = &shadings.back();
        }
        if (!plane->usable) continue;
        for (const auto& point : face.points) {
            const float t = math::dot(point - plane->foot, plane->direction);
            plane->near = std::min(plane->near, t);
            plane->far = std::max(plane->far, t);
        }
    }
    for (auto& plane : shadings) {
        if (!plane.usable) continue;
        for (std::size_t k = 0; k < kShadingStops; ++k) {
            const float t = plane.near + (plane.far - plane.near) *
                                             static_cast<float>(k) /
                                             static_cast<float>(kShadingStops - 1);
            plane.brightness[k] = brightness(plane.foot + plane.direction * t, plane.normal);
        }
    }

    for (auto& face : scene.faces) {
        const auto normal = outward_normal(face.points);
        if (!normal) continue;

        const auto& origin = face.points[0];
        // The stickers get their chroma raised before they are lit; the body
        // is plastic and stays as it is.
        const int saturation = static_cast<int>(std::lround(lighting.saturation * 100.0f));
        const Color base = face.color == kBodyColor || saturation == 100
                               ? face.color
                               : saturate(face.color, saturation);
        const math::Vec3 u = face.points[1] - origin;
        const float uu = math::dot(u, u);
        const math::Vec3 v = face.points[3] - origin;
        const float vv = math::dot(v, v);
        if (!(uu > 0.0f) || !(vv > 0.0f)) continue;
        const float offset = math::dot(origin, *normal);

        // The reference shade: where the plane comes nearest the cube's
        // centre, the middle of the cube face for a sticker on the outside.
        const math::Vec3 anchor_foot =
            anchor_in_view - *normal * math::dot(anchor_in_view - origin, *normal);
        face.color = shade(base, brightness(anchor_foot, *normal));

        // Shading: the plane's axis and stops, this sticker's colour.
        if (shaded) {
            for (const auto& plane : shadings) {
                if (!plane.usable || std::abs(plane.offset - offset) >= 1e-4f ||
                    math::length(plane.normal - *normal) >= 1e-4f) {
                    continue;
                }
                // The plane's brightness at a point along its axis, read off
                // the stops the way the gradient itself will read it.
                const auto along_axis = [&](float t) {
                    const float span = plane.far - plane.near;
                    const float f = span > 0.0f ? std::clamp((t - plane.near) / span, 0.0f, 1.0f) : 0.0f;
                    const float scaled = f * static_cast<float>(kShadingStops - 1);
                    const auto low = static_cast<std::size_t>(scaled);
                    const auto high = std::min(low + 1, kShadingStops - 1);
                    const float mix = scaled - static_cast<float>(low);
                    return plane.brightness[low] * (1.0f - mix) + plane.brightness[high] * mix;
                };
                float face_near = 1e9f, face_far = -1e9f;
                for (const auto& point : face.points) {
                    const float t = math::dot(point - plane.foot, plane.direction);
                    face_near = std::min(face_near, t);
                    face_far = std::max(face_far, t);
                }

                // A sticker the function does not move a byte across is drawn
                // flat at its own middle: the same picture as the gradient
                // would make, for a fraction of the cost -- which matters on a
                // 28x28 with three thousand of them.
                const Color at_near = shade(base, along_axis(face_near));
                const Color at_far = shade(base, along_axis(face_far));
                if (at_near == at_far) {
                    face.color = shade(base, along_axis(0.5f * (face_near + face_far)));
                    break;
                }

                ShadingOf<math::Vec3> shading;
                shading.from = plane.foot + plane.direction * plane.near;
                shading.to = plane.foot + plane.direction * plane.far;
                for (const float b : plane.brightness) {
                    shading.colors.push_back(shade(base, b));
                }
                face.shading = std::move(shading);
                break;
            }
        }

        // Specular, lamp by lamp: where the mirror image of the lamp, seen
        // from the eye, lands on this plane. Both have to be on the outer side
        // of the face for there to be anything to see.
        const float eye_height = math::dot(-origin, *normal);
        if (!(eye_height > 0.0f)) continue;

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
                planes.push_back(PlaneGlint{i, *normal, offset, std::nullopt});
                plane = &planes.back();

                const math::Vec3 mirrored = lamp_pos - *normal * (2.0f * lamp_height);
                // The segment from the mirrored lamp to the eye (the origin)
                // crosses the plane at this fraction of its length.
                const float t = lamp_height / (lamp_height + eye_height);
                const math::Vec3 peak = mirrored * (1.0f - t);
                const math::Vec3 u_hat = u / std::sqrt(uu);
                const math::Vec3 v_hat = v / std::sqrt(vv);

                const auto peak_alpha = specular_alpha(lamp, lamp_pos, peak, *normal);
                if (peak_alpha >= kHighlightMinAlpha) {
                    // The footprint: how far the lobe stays above the
                    // threshold each way along each axis, and the ellipse
                    // through those four ends.
                    const float plus_u = lobe_reach(lamp, lamp_pos, peak, u_hat, *normal);
                    const float minus_u = lobe_reach(lamp, lamp_pos, peak, -u_hat, *normal);
                    const float plus_v = lobe_reach(lamp, lamp_pos, peak, v_hat, *normal);
                    const float minus_v = lobe_reach(lamp, lamp_pos, peak, -v_hat, *normal);
                    const float semi_u = 0.5f * (plus_u + minus_u);
                    const float semi_v = 0.5f * (plus_v + minus_v);
                    if (semi_u > 0.0f && semi_v > 0.0f) {
                        const math::Vec3 centre = peak + u_hat * (0.5f * (plus_u - minus_u)) +
                                                  v_hat * (0.5f * (plus_v - minus_v));
                        const auto at = [&](const math::Vec3& point) {
                            return int{specular_alpha(lamp, lamp_pos, point, *normal)};
                        };
                        // The stops are the lobe's own strength at evenly
                        // spaced distances out, taken round the ellipse on
                        // both axes, so the falloff drawn is the lobe's rather
                        // than a guess between the centre and the rim. Kept
                        // non-increasing outwards, and zero at the rim: the rim
                        // is where the lobe fell below the threshold, and a
                        // gradient pads its last stop past the radius.
                        HighlightOf<math::Vec3> glint;
                        glint.centre = centre;
                        glint.rims = {centre + u_hat * semi_u, centre - u_hat * semi_u,
                                      centre + v_hat * semi_v, centre - v_hat * semi_v};
                        int previous = 255;
                        for (std::size_t s = 0; s + 1 < kGlintStops; ++s) {
                            const float t = static_cast<float>(s) /
                                            static_cast<float>(kGlintStops - 1);
                            const int sampled = (at(centre + u_hat * (t * semi_u)) +
                                                 at(centre - u_hat * (t * semi_u)) +
                                                 at(centre + v_hat * (t * semi_v)) +
                                                 at(centre - v_hat * (t * semi_v)) + 2) / 4;
                            previous = std::min(previous, sampled);
                            glint.stops[s] = static_cast<std::uint8_t>(previous);
                        }
                        glint.stops[kGlintStops - 1] = 0;
                        glint.anchor = anchor_foot;
                        glint.axis_u = u_hat;
                        glint.axis_v = v_hat;
                        plane->glint = glint;
                        plane->axis_u = u_hat;
                        plane->axis_v = v_hat;
                        plane->semi_u = semi_u;
                        plane->semi_v = semi_v;
                    }
                }
            }
            if (!plane->glint) continue;

            // Only stickers the footprint reaches carry it. The sticker's box
            // in the footprint's own frame, scaled so the ellipse is the unit
            // circle, has to come within that circle -- with a little to
            // spare, since the drawn lobe is all but gone at the rim and a
            // sticker cut off exactly there would show the cut.
            constexpr float kReachMargin = 1.1f;
            const math::Vec3& centre = plane->glint->centre;
            float min_u = 1e9f, max_u = -1e9f, min_v = 1e9f, max_v = -1e9f;
            for (const auto& corner : face.points) {
                const math::Vec3 d = corner - centre;
                const float cu = math::dot(d, plane->axis_u) / plane->semi_u;
                const float cv = math::dot(d, plane->axis_v) / plane->semi_v;
                min_u = std::min(min_u, cu);
                max_u = std::max(max_u, cu);
                min_v = std::min(min_v, cv);
                max_v = std::max(max_v, cv);
            }
            const float nearest_u = std::clamp(0.0f, min_u, max_u);
            const float nearest_v = std::clamp(0.0f, min_v, max_v);
            if (nearest_u * nearest_u + nearest_v * nearest_v >=
                kReachMargin * kReachMargin) {
                continue;
            }

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
