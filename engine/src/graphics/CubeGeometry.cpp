#include "graphics/CubeGeometry.hpp"

#include <array>
#include <cstddef>

#include "graphics/Palette.hpp"

namespace rubiks::graphics {
namespace {

using cube::Face;
using math::Vec3;

/** Corner offsets in units of half_extent, wound outward-facing. */
using Corners = std::array<Vec3, 4>;

constexpr float kH = 1.0f;

[[nodiscard]] Corners corners_of(Face face) noexcept
{
    switch (face) {
        case Face::Right:
            return {Vec3{kH, -kH, kH}, Vec3{kH, -kH, -kH}, Vec3{kH, kH, -kH},
                    Vec3{kH, kH, kH}};
        case Face::Left:
            return {Vec3{-kH, -kH, -kH}, Vec3{-kH, -kH, kH}, Vec3{-kH, kH, kH},
                    Vec3{-kH, kH, -kH}};
        case Face::Up:
            return {Vec3{-kH, kH, kH}, Vec3{kH, kH, kH}, Vec3{kH, kH, -kH},
                    Vec3{-kH, kH, -kH}};
        case Face::Down:
            return {Vec3{-kH, -kH, -kH}, Vec3{kH, -kH, -kH}, Vec3{kH, -kH, kH},
                    Vec3{-kH, -kH, kH}};
        case Face::Front:
            return {Vec3{-kH, -kH, kH}, Vec3{kH, -kH, kH}, Vec3{kH, kH, kH},
                    Vec3{-kH, kH, kH}};
        case Face::Back:
            break;
    }
    return {Vec3{kH, -kH, -kH}, Vec3{-kH, -kH, -kH}, Vec3{-kH, kH, -kH},
            Vec3{kH, kH, -kH}};
}

/**
 * The layer index a cubie must occupy for `face` to be on the outside.
 *
 * Positive faces sit in the last layer, negative faces in layer 0.
 */
[[nodiscard]] int outer_layer(Face face, int size) noexcept
{
    switch (face) {
        case Face::Right:
        case Face::Up:
        case Face::Front:
            return size - 1;
        case Face::Left:
        case Face::Down:
        case Face::Back:
            break;
    }
    return 0;
}

/** The coordinate that decides whether `face` is exposed. */
[[nodiscard]] int coordinate_for(Face face, int x, int y, int z) noexcept
{
    switch (face) {
        case Face::Right:
        case Face::Left:
            return x;
        case Face::Up:
        case Face::Down:
            return y;
        case Face::Front:
        case Face::Back:
            break;
    }
    return z;
}

}  // namespace

float sticker_half_extent(int size) noexcept
{
    // A cubie spans one N-th of the cube's half extent; the sticker shrinks
    // inside that cell, leaving the rest as seam.
    return kCubeHalfExtent / static_cast<float>(size) * kStickerScale;
}

float cubie_center(int i, int size) noexcept
{
    // Pitch is 2 / N and the first center sits half a pitch in from the face.
    return kCubeHalfExtent * static_cast<float>(2 * i - (size - 1)) /
           static_cast<float>(size);
}

void append_sticker(WorldScene& scene, const Vec3& center, float half_extent,
                    Face face, const Color& color)
{
    const auto offsets = corners_of(face);

    WorldFace quad;
    quad.color = color;
    for (std::size_t i = 0; i < offsets.size(); ++i) {
        quad.points[i] = Vec3{center.x + offsets[i].x * half_extent,
                              center.y + offsets[i].y * half_extent,
                              center.z + offsets[i].z * half_extent};
    }
    scene.faces.push_back(quad);
}

WorldScene build_cube_scene(const cube::CubeState& state)
{
    const int size = state.size();
    const float half_extent = sticker_half_extent(size);

    WorldScene scene;
    // Six faces of an N x N sheet: 54 at N = 3.
    scene.faces.reserve(static_cast<std::size_t>(6 * size * size));

    for (int x = 0; x < size; ++x) {
        for (int y = 0; y < size; ++y) {
            for (int z = 0; z < size; ++z) {
                const Vec3 center{cubie_center(x, size), cubie_center(y, size),
                                  cubie_center(z, size)};
                const auto& cubie = state.at(x, y, z);

                for (std::size_t i = 0; i < cube::kFaceCount; ++i) {
                    const auto face = static_cast<Face>(i);
                    if (coordinate_for(face, x, y, z) !=
                        outer_layer(face, size)) {
                        continue;
                    }
                    append_sticker(scene, center, half_extent, face,
                                   to_color(cubie.sticker(face)));
                }
            }
        }
    }
    return scene;
}

}  // namespace rubiks::graphics
