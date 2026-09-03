#include "graphics/Passes.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include "graphics/ConvexHull.hpp"

namespace rubiks::graphics {
namespace {

constexpr std::size_t kCorners = 4;

math::Vec3 to_vec3(const math::Vec4& v) noexcept
{
    return math::Vec3{v.x, v.y, v.z};
}

/** Twice the signed area of a quad; positive for counter-clockwise winding. */
float signed_area_doubled(const std::array<math::Vec2, kCorners>& points) noexcept
{
    float sum = 0.0f;
    for (std::size_t i = 0; i < kCorners; ++i) {
        const auto& current = points[i];
        const auto& next = points[(i + 1) % kCorners];
        sum += current.x * next.y - next.x * current.y;
    }
    return sum;
}

/** The same map applied to every point of a shadow group. */
template <typename From, typename To, typename Map>
std::optional<ShadowGroup<To>> map_shadow(
    const std::optional<ShadowGroup<From>>& shadow, Map&& map)
{
    if (!shadow) return std::nullopt;

    ShadowGroup<To> result;
    result.opacity = shadow->opacity;
    result.fade_start = map(shadow->fade_start);
    result.fade_end = map(shadow->fade_end);

    result.polygons.reserve(shadow->polygons.size());
    for (const auto& polygon : shadow->polygons) {
        std::vector<To> mapped;
        mapped.reserve(polygon.size());
        for (const auto& point : polygon) mapped.push_back(map(point));
        result.polygons.push_back(std::move(mapped));
    }

    return result;
}

/** Whether every point of a view-space shadow group is in front of the near plane. */
bool shadow_in_front(const ShadowGroup<math::Vec3>& shadow, float near_plane)
{
    const auto in_front = [near_plane](const math::Vec3& point) {
        return point.z < -near_plane;
    };
    for (const auto& polygon : shadow.polygons) {
        if (!std::all_of(polygon.begin(), polygon.end(), in_front)) return false;
    }
    return in_front(shadow.fade_start) && in_front(shadow.fade_end);
}

}  // namespace

WorldScene TransformPass::operator()(WorldScene scene) const
{
    for (auto& face : scene.faces) {
        for (auto& point : face.points) {
            point = to_vec3(math::apply_point(model_matrix, point));
        }
    }
    // The casters are the cube too, and a shadow of a cube that has moved is
    // the shadow of where it moved to.
    for (auto& caster : scene.casters) {
        for (auto& corner : caster.corners) {
            corner = to_vec3(math::apply_point(model_matrix, corner));
        }
    }
    return scene;
}

TransformPass transform(const math::Transform& model) noexcept
{
    return TransformPass{model.to_matrix()};
}

ViewScene ViewPass::operator()(const WorldScene& scene) const
{
    ViewScene result;
    result.faces.reserve(scene.faces.size());

    const auto to_view = [this](const math::Vec3& point) {
        return to_vec3(math::apply_point(view_matrix, point));
    };

    for (const auto& face : scene.faces) {
        ViewFace transformed;
        transformed.color = face.color;
        for (std::size_t i = 0; i < kCorners; ++i) {
            transformed.points[i] = to_view(face.points[i]);
        }
        result.faces.push_back(transformed);
    }

    result.shadow = map_shadow<math::Vec3, math::Vec3>(scene.shadow, to_view);

    // The casters come along to become the bodies drawn under the stickers.
    result.casters.reserve(scene.casters.size());
    for (const auto& caster : scene.casters) {
        std::array<math::Vec3, 8> corners{};
        for (std::size_t i = 0; i < corners.size(); ++i) {
            corners[i] = to_view(caster.corners[i]);
        }
        result.casters.push_back(corners);
    }
    return result;
}

ViewPass view(const Camera& camera) noexcept
{
    return ViewPass{camera.view()};
}

ClipScene ProjectPass::operator()(const ViewScene& scene) const
{
    ClipScene result;
    result.faces.reserve(scene.faces.size());

    // w is the positive view-space depth, so this division is safe for every
    // point the near-plane guards below have let through.
    const auto to_ndc = [this](const math::Vec3& point) {
        const auto clip = math::apply_point(projection_matrix, point);
        return math::Vec2{clip.x / clip.w, clip.y / clip.w};
    };

    for (const auto& face : scene.faces) {
        ClipFace projected;
        projected.color = face.color;

        float depth_sum = 0.0f;
        bool in_front = true;
        for (std::size_t i = 0; i < kCorners; ++i) {
            const auto& point = face.points[i];
            if (point.z >= -near_plane) {
                in_front = false;
                break;
            }
            projected.ndc[i] = to_ndc(point);
            depth_sum += point.z;
        }
        if (!in_front) continue;

        projected.depth = depth_sum / static_cast<float>(kCorners);

        if (face.shading) {
            projected.shading = ShadingOf<math::Vec2>{
                to_ndc(face.shading->from), to_ndc(face.shading->to),
                face.shading->colors};
        }
        // A highlight lies on the face's plane, and its footprint can be far
        // wider than the face. Under perspective a footprint like that is not
        // an ellipse on screen, and an ellipse fitted through its projected
        // rims would put the far side of the lobe, compressed, onto the
        // stickers. So the footprint is carried through the tangent of the
        // projection at the glint's anchor on the face: exact where the glint
        // is seen, and never asking a rim behind the eye to project. Only the
        // anchor and its two tangent steps are projected.
        for (const auto& glint : face.highlights) {
            constexpr float kTangentStep = 0.25f;
            const math::Vec3& anchor = glint.anchor;
            const math::Vec3 step_u = anchor + glint.axis_u * kTangentStep;
            const math::Vec3 step_v = anchor + glint.axis_v * kTangentStep;
            if (anchor.z >= -near_plane || step_u.z >= -near_plane ||
                step_v.z >= -near_plane) {
                continue;
            }
            const math::Vec2 origin = to_ndc(anchor);
            const math::Vec2 along_u = to_ndc(step_u);
            const math::Vec2 along_v = to_ndc(step_v);
            const auto flatten = [&](const math::Vec3& point) {
                const math::Vec3 d = point - anchor;
                const float cu = math::dot(d, glint.axis_u) / kTangentStep;
                const float cv = math::dot(d, glint.axis_v) / kTangentStep;
                return math::Vec2{
                    origin.x + cu * (along_u.x - origin.x) + cv * (along_v.x - origin.x),
                    origin.y + cu * (along_u.y - origin.y) + cv * (along_v.y - origin.y)};
            };
            HighlightOf<math::Vec2> flat;
            flat.centre = flatten(glint.centre);
            for (std::size_t i = 0; i < glint.rims.size(); ++i) {
                flat.rims[i] = flatten(glint.rims[i]);
            }
            flat.stops = glint.stops;
            projected.highlights.push_back(flat);
        }
        result.faces.push_back(projected);
    }

    // The shadow is one thing: a polygon of it reaching the near plane cannot
    // be dropped on its own without tearing the outline, so it goes whole.
    if (scene.shadow && shadow_in_front(*scene.shadow, near_plane)) {
        result.shadow = map_shadow<math::Vec3, math::Vec2>(scene.shadow, to_ndc);
    }

    // A caster with a corner at the near plane goes whole, like a face.
    const float limit = -near_plane;
    for (const auto& caster : scene.casters) {
        const bool in_front = std::all_of(
            caster.begin(), caster.end(),
            [limit](const math::Vec3& corner) { return corner.z < limit; });
        if (!in_front) continue;
        std::array<math::Vec2, 8> corners{};
        for (std::size_t i = 0; i < corners.size(); ++i) corners[i] = to_ndc(caster[i]);
        result.casters.push_back(corners);
    }
    return result;
}

ProjectPass project(const Camera& camera) noexcept
{
    return ProjectPass{camera.projection(), camera.near_plane()};
}

ClipScene CullPass::operator()(ClipScene scene) const
{
    auto& faces = scene.faces;
    faces.erase(std::remove_if(faces.begin(), faces.end(),
                               [](const ClipFace& face) {
                                   return signed_area_doubled(face.ndc) <= 0.0f;
                               }),
                faces.end());
    return scene;
}

CullPass cull() noexcept
{
    return CullPass{};
}

ClipScene DepthSortPass::operator()(ClipScene scene) const
{
    // Ascending view-space z is back to front: z is negative in front of the
    // camera and decreases with distance.
    std::stable_sort(
        scene.faces.begin(), scene.faces.end(),
        [](const ClipFace& a, const ClipFace& b) { return a.depth < b.depth; });
    return scene;
}

DepthSortPass depth_sort() noexcept
{
    return DepthSortPass{};
}

RenderScene ViewportPass::operator()(const ClipScene& scene) const
{
    RenderScene result;
    result.faces.reserve(scene.faces.size());

    const auto to_pixels = [this](const math::Vec2& ndc) {
        return math::Vec2{rect.x + (ndc.x + 1.0f) * 0.5f * rect.width,
                          rect.y + (1.0f - ndc.y) * 0.5f * rect.height};
    };

    for (const auto& face : scene.faces) {
        RenderFace mapped;
        mapped.color = face.color;
        for (std::size_t i = 0; i < kCorners; ++i) {
            mapped.points[i] = to_pixels(face.ndc[i]);
        }
        if (face.shading) {
            mapped.shading = RenderShading{to_pixels(face.shading->from),
                                           to_pixels(face.shading->to),
                                           face.shading->colors};
        }
        for (const auto& glint : face.highlights) {
            RenderHighlight pixels;
            pixels.centre = to_pixels(glint.centre);
            for (std::size_t i = 0; i < glint.rims.size(); ++i) {
                pixels.rims[i] = to_pixels(glint.rims[i]);
            }
            pixels.stops = glint.stops;
            mapped.highlights.push_back(pixels);
        }
        result.faces.push_back(mapped);
    }

    if (const auto mapped =
            map_shadow<math::Vec2, math::Vec2>(scene.shadow, to_pixels)) {
        RenderShadow shadow;
        shadow.polygons = mapped->polygons;
        shadow.fade_start = mapped->fade_start;
        shadow.fade_end = mapped->fade_end;
        shadow.opacity = mapped->opacity;
        shadow.blur_sigma = kShadowBlurShare * rect.width;
        shadow.clip = clip;
        result.shadow = std::move(shadow);
    }

    // The bodies: a convex box's outline on screen is the hull of its corners.
    result.bodies.reserve(scene.casters.size());
    for (const auto& caster : scene.casters) {
        std::vector<math::Vec2> corners;
        corners.reserve(caster.size());
        for (const auto& corner : caster) corners.push_back(to_pixels(corner));
        result.bodies.push_back(convex_hull(std::move(corners)));
    }
    return result;
}

ViewportPass viewport(const Rect& rect) noexcept
{
    return ViewportPass{rect, rect};
}

ViewportPass viewport(const Rect& rect, const Rect& stage) noexcept
{
    return ViewportPass{rect, stage};
}

}  // namespace rubiks::graphics
