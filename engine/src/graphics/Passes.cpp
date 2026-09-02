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

    result.occluders.reserve(shadow->occluders.size());
    for (const auto& box : shadow->occluders) {
        std::array<To, 8> mapped{};
        for (std::size_t i = 0; i < box.size(); ++i) mapped[i] = map(box[i]);
        result.occluders.push_back(mapped);
    }

    result.contact.reserve(shadow->contact.size());
    for (const auto& point : shadow->contact) result.contact.push_back(map(point));
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
    for (const auto& box : shadow.occluders) {
        if (!std::all_of(box.begin(), box.end(), in_front)) return false;
    }
    if (!std::all_of(shadow.contact.begin(), shadow.contact.end(), in_front)) {
        return false;
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

        // The highlight lies on the face, so it passed the same guard.
        if (face.highlight) {
            projected.highlight = HighlightOf<math::Vec2>{
                to_ndc(face.highlight->centre), to_ndc(face.highlight->rim),
                face.highlight->alpha};
        }
        result.faces.push_back(projected);
    }

    // The shadow is one thing: a polygon of it reaching the near plane cannot
    // be dropped on its own without tearing the outline, so it goes whole.
    if (scene.shadow && shadow_in_front(*scene.shadow, near_plane)) {
        result.shadow = map_shadow<math::Vec3, math::Vec2>(scene.shadow, to_ndc);
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
        if (face.highlight) {
            mapped.highlight = RenderHighlight{to_pixels(face.highlight->centre),
                                               to_pixels(face.highlight->rim),
                                               face.highlight->alpha};
        }
        result.faces.push_back(mapped);
    }

    if (const auto mapped =
            map_shadow<math::Vec2, math::Vec2>(scene.shadow, to_pixels)) {
        RenderShadow shadow;
        shadow.polygons = mapped->polygons;
        shadow.fade_start = mapped->fade_start;
        shadow.fade_end = mapped->fade_end;
        shadow.contact = mapped->contact;
        shadow.opacity = mapped->opacity;
        shadow.blur_sigma = kShadowBlurShare * rect.width;
        shadow.clip = rect;

        // A convex box's outline on screen is the hull of its corners, which
        // is all the renderer needs to keep the shadow out of the cube.
        shadow.occluders.reserve(mapped->occluders.size());
        for (const auto& box : mapped->occluders) {
            shadow.occluders.push_back(
                convex_hull(std::vector<math::Vec2>(box.begin(), box.end())));
        }
        result.shadow = std::move(shadow);
    }
    return result;
}

ViewportPass viewport(const Rect& rect) noexcept
{
    return ViewportPass{rect};
}

}  // namespace rubiks::graphics
