#include "graphics/CubeGeometry.hpp"

namespace rubiks::graphics {
namespace {

using math::Vec3;

/** Corner offsets in units of half_extent, indexed as (x, y, z) signs. */
constexpr float kHalf = 1.0f;

Vec3 corner(const Vec3& center, float half_extent, float x, float y, float z)
{
    return Vec3{center.x + x * half_extent, center.y + y * half_extent,
                center.z + z * half_extent};
}

}  // namespace

CubeFaceColors standard_cube_colors() noexcept
{
    return CubeFaceColors{
        Color{183, 18, 52, 255},    // +X red
        Color{255, 88, 0, 255},     // -X orange
        Color{255, 255, 255, 255},  // +Y white
        Color{255, 213, 0, 255},    // -Y yellow
        Color{0, 155, 72, 255},     // +Z green
        Color{0, 70, 173, 255},     // -Z blue
    };
}

void append_cube(WorldScene& scene, const Vec3& center, float half_extent,
                 const CubeFaceColors& colors)
{
    const auto at = [&](float x, float y, float z) {
        return corner(center, half_extent, x, y, z);
    };
    const auto h = kHalf;

    scene.faces.push_back(
        WorldFace{{at(h, -h, h), at(h, -h, -h), at(h, h, -h), at(h, h, h)},
                  colors.right});
    scene.faces.push_back(
        WorldFace{{at(-h, -h, -h), at(-h, -h, h), at(-h, h, h), at(-h, h, -h)},
                  colors.left});
    scene.faces.push_back(
        WorldFace{{at(-h, h, h), at(h, h, h), at(h, h, -h), at(-h, h, -h)},
                  colors.up});
    scene.faces.push_back(
        WorldFace{{at(-h, -h, -h), at(h, -h, -h), at(h, -h, h), at(-h, -h, h)},
                  colors.down});
    scene.faces.push_back(
        WorldFace{{at(-h, -h, h), at(h, -h, h), at(h, h, h), at(-h, h, h)},
                  colors.front});
    scene.faces.push_back(
        WorldFace{{at(h, -h, -h), at(-h, -h, -h), at(-h, h, -h), at(h, h, -h)},
                  colors.back});
}

WorldScene build_scene()
{
    WorldScene scene;
    scene.faces.reserve(6);
    append_cube(scene, Vec3{0.0f, 0.0f, 0.0f}, 1.0f, standard_cube_colors());
    return scene;
}

}  // namespace rubiks::graphics
