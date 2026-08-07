#include "graphics/Passes.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

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

}  // namespace

WorldScene TransformPass::operator()(WorldScene scene) const
{
    for (auto& face : scene.faces) {
        for (auto& point : face.points) {
            point = to_vec3(math::apply_point(model_matrix, point));
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

    for (const auto& face : scene.faces) {
        ViewFace transformed;
        transformed.color = face.color;
        for (std::size_t i = 0; i < kCorners; ++i) {
            transformed.points[i] =
                to_vec3(math::apply_point(view_matrix, face.points[i]));
        }
        result.faces.push_back(transformed);
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

            // w is the positive view-space depth, so this division is safe
            // for every vertex that passed the guard above.
            const auto clip = math::apply_point(projection_matrix, point);
            projected.ndc[i] = math::Vec2{clip.x / clip.w, clip.y / clip.w};
            depth_sum += point.z;
        }
        if (!in_front) continue;

        projected.depth = depth_sum / static_cast<float>(kCorners);
        result.faces.push_back(projected);
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

    for (const auto& face : scene.faces) {
        RenderFace mapped;
        mapped.color = face.color;
        for (std::size_t i = 0; i < kCorners; ++i) {
            mapped.points[i] =
                math::Vec2{(face.ndc[i].x + 1.0f) * 0.5f * width,
                           (1.0f - face.ndc[i].y) * 0.5f * height};
        }
        result.faces.push_back(mapped);
    }
    return result;
}

ViewportPass viewport(std::uint32_t width, std::uint32_t height) noexcept
{
    return ViewportPass{static_cast<float>(width), static_cast<float>(height)};
}

}  // namespace rubiks::graphics
