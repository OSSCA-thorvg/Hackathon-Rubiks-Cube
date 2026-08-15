#include "graphics/CubeGeometry.hpp"

#include <array>
#include <cmath>
#include <cstddef>

#include "graphics/Palette.hpp"

namespace rubiks::graphics {
namespace {

using cube::Axis;
using cube::Face;
using math::Vec3;

constexpr float kPi = 3.14159265358979323846f;

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

/** Unit vector along the positive direction of an axis. */
[[nodiscard]] Vec3 axis_direction(Axis axis) noexcept
{
    switch (axis) {
        case Axis::X:
            return Vec3{1.0f, 0.0f, 0.0f};
        case Axis::Y:
            return Vec3{0.0f, 1.0f, 0.0f};
        case Axis::Z:
            break;
    }
    return Vec3{0.0f, 0.0f, 1.0f};
}

/** The coordinate of a cubie along a move's axis. */
[[nodiscard]] int coordinate_on(Axis axis, int x, int y, int z) noexcept
{
    switch (axis) {
        case Axis::X:
            return x;
        case Axis::Y:
            return y;
        case Axis::Z:
            break;
    }
    return z;
}

/** The two faces pointing along an axis: positive direction first. */
[[nodiscard]] std::array<Face, 2> faces_along(Axis axis) noexcept
{
    switch (axis) {
        case Axis::X:
            return {Face::Right, Face::Left};
        case Axis::Y:
            return {Face::Up, Face::Down};
        case Axis::Z:
            break;
    }
    return {Face::Front, Face::Back};
}

[[nodiscard]] bool in_layers(cube::LayerMask layers, int index) noexcept
{
    return (layers & cube::layer(index)) != 0;
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

math::Quaternion layer_rotation(Axis axis, float degrees) noexcept
{
    // The single place the domain's clockwise-from-the-positive-end convention
    // becomes a right-handed rotation.
    return math::quaternion_from_axis_angle(axis_direction(axis),
                                            -degrees * kPi / 180.0f);
}

WorldScene build_cube_scene(const cube::CubeState& state, Palette palette)
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
                                   to_color(cubie.sticker(face), palette));
                }
            }
        }
    }
    return scene;
}

WorldScene build_cube_scene(const cube::CubeState& state,
                            const std::optional<ActiveRotation>& active,
                            Palette palette)
{
    if (!active) return build_cube_scene(state, palette);

    const int size = state.size();
    const float half_extent = sticker_half_extent(size);
    const auto turn = layer_rotation(active->axis, active->angle_degrees);
    const auto body_faces = faces_along(active->axis);

    WorldScene scene;
    // The stickers, plus at most two cut sheets of N x N body faces.
    scene.faces.reserve(static_cast<std::size_t>(8 * size * size));

    // Emits one quad, turned with its layer when that layer is the moving one.
    const auto emit = [&](const Vec3& center, Face face, const Color& color,
                          bool turning) {
        append_sticker(scene, center, half_extent, face, color);
        if (!turning) return;

        for (auto& point : scene.faces.back().points) {
            point = math::quaternion_rotate(turn, point);
        }
    };

    for (int x = 0; x < size; ++x) {
        for (int y = 0; y < size; ++y) {
            for (int z = 0; z < size; ++z) {
                const Vec3 center{cubie_center(x, size), cubie_center(y, size),
                                  cubie_center(z, size)};
                const auto& cubie = state.at(x, y, z);

                const int slice = coordinate_on(active->axis, x, y, z);
                const bool turning = in_layers(active->layers, slice);

                for (std::size_t i = 0; i < cube::kFaceCount; ++i) {
                    const auto face = static_cast<Face>(i);
                    if (coordinate_for(face, x, y, z) !=
                        outer_layer(face, size)) {
                        continue;
                    }
                    emit(center, face, to_color(cubie.sticker(face), palette),
                         turning);
                }

                // A cut surface appears wherever a turning layer meets a still
                // one; the cubies on both sides of that gap show their body.
                // Beyond the outermost layer there is no neighbour and no cut,
                // which is why those cases read as "same membership".
                const bool after =
                    slice + 1 < size ? in_layers(active->layers, slice + 1)
                                     : turning;
                const bool before =
                    slice > 0 ? in_layers(active->layers, slice - 1) : turning;

                if (after != turning) {
                    emit(center, body_faces[0], kBodyColor, turning);
                }
                if (before != turning) {
                    emit(center, body_faces[1], kBodyColor, turning);
                }
            }
        }
    }
    return scene;
}

}  // namespace rubiks::graphics
