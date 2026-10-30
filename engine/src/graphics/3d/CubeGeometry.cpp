#include "graphics/3d/CubeGeometry.hpp"

#include <array>
#include <cmath>
#include <cstddef>

#include "graphics/Palette.hpp"
#include "cube/Surface.hpp"
#include "math/Angle.hpp"

namespace rubiks::graphics {
namespace {

using cube::Axis;
using cube::Face;
using cube::in_layers;
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

/** The cubie at one cell of a layer's cut sheet. */
[[nodiscard]] cube::CubiePosition cap_position(
    Axis axis, int slice, int across, int down) noexcept
{
    const std::array<Axis, 2> sheet = cube::other_axes(axis);
    cube::CubiePosition position{0, 0, 0};
    cube::set_coordinate(position, axis, slice);
    cube::set_coordinate(position, sheet[0], across);
    cube::set_coordinate(position, sheet[1], down);
    return position;
}

/**
 * The box spanning [low, high] along `axis` and the whole cube across it.
 *
 * At rest that is the cube itself. During a turn it is one run of layers, and
 * a run is all the shadow needs: the union of the runs is the cube, so the
 * union of their shadows is the cube's shadow, whatever N is.
 */
[[nodiscard]] Caster slab(Axis axis, float low, float high) noexcept
{
    Caster caster;
    std::size_t index = 0;
    for (const float sx : {-1.0f, 1.0f}) {
        for (const float sy : {-1.0f, 1.0f}) {
            for (const float sz : {-1.0f, 1.0f}) {
                Vec3 corner{sx * kCubeHalfExtent, sy * kCubeHalfExtent,
                            sz * kCubeHalfExtent};
                switch (axis) {
                    case Axis::X:
                        corner.x = sx < 0.0f ? low : high;
                        break;
                    case Axis::Y:
                        corner.y = sy < 0.0f ? low : high;
                        break;
                    case Axis::Z:
                        corner.z = sz < 0.0f ? low : high;
                        break;
                }
                caster.corners[index++] = corner;
            }
        }
    }
    return caster;
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

Vec3 axis_direction(Axis axis) noexcept
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

math::Quaternion layer_rotation(Axis axis, float degrees) noexcept
{
    // The single place the domain's clockwise-from-the-positive-end convention
    // becomes a right-handed rotation.
    return math::quaternion_from_axis_angle(axis_direction(axis),
                                            math::to_radians(-degrees));
}

WorldScene build_cube_scene(cube::CubeView state, Palette palette)
{
    const int size = state.size();
    const float half_extent = sticker_half_extent(size);

    WorldScene scene;
    // Six faces of an N x N sheet: 54 at N = 3.
    scene.faces.reserve(static_cast<std::size_t>(6 * size * size));
    // At rest the whole cube is one box, and one box is one shadow.
    scene.casters.push_back(slab(Axis::X, -kCubeHalfExtent, kCubeHalfExtent));

    for (const cube::SurfaceSticker sticker : cube::surface_cells(size)) {
        const Vec3 center{cubie_center(sticker.x, size),
                          cubie_center(sticker.y, size),
                          cubie_center(sticker.z, size)};
        append_sticker(scene, center, half_extent, sticker.face,
                       to_lit_color(state.sticker(sticker), palette));
    }
    return scene;
}

WorldScene build_cube_scene(cube::CubeView state,
                            const std::optional<ActiveRotation>& active,
                            Palette palette)
{
    if (!active) return build_cube_scene(state, palette);

    const int size = state.size();
    const float half_extent = sticker_half_extent(size);
    const math::Quaternion turn =
        layer_rotation(active->axis, active->angle_degrees);
    // The cut sheets face along the turning axis: the lower layer's towards
    // its positive end, the upper layer's towards the end opposite.
    const std::array<Face, 2> body_faces{cube::end_face(active->axis, true),
                                         cube::end_face(active->axis, false)};

    WorldScene scene;
    // Reserve for surface stickers and the two sheets of a single interface.
    scene.faces.reserve(static_cast<std::size_t>(8 * size * size));

    /** Appends one quad, carrying it with its layer when that layer moves. */
    const auto emit = [&](const Vec3& center, Face face, const Color& color,
                          bool turning) -> void {
        append_sticker(scene, center, half_extent, face, color);
        if (!turning) return;

        for (auto& point : scene.faces.back().points) {
            point = math::quaternion_rotate(turn, point);
        }
    };

    for (const cube::SurfaceSticker sticker : cube::surface_cells(size)) {
        const Vec3 center{cubie_center(sticker.x, size),
                          cubie_center(sticker.y, size),
                          cubie_center(sticker.z, size)};
        const bool turning =
            in_layers(active->layers,
                      cube::coordinate_on(active->axis, sticker));
        emit(center, sticker.face,
             to_lit_color(state.sticker(sticker), palette), turning);
    }

    // Only an interface between moving and still layers exposes a cut sheet.
    // Both sides of each interface are body geometry, independent of colours.
    for (int slice = 0; slice + 1 < size; ++slice) {
        const bool lower_turning = in_layers(active->layers, slice);
        const bool upper_turning = in_layers(active->layers, slice + 1);
        if (lower_turning == upper_turning) continue;

        for (int down = 0; down < size; ++down) {
            for (int across = 0; across < size; ++across) {
                const cube::CubiePosition lower =
                    cap_position(active->axis, slice, across, down);
                const cube::CubiePosition upper =
                    cap_position(active->axis, slice + 1, across, down);
                emit(Vec3{cubie_center(lower.x, size),
                          cubie_center(lower.y, size),
                          cubie_center(lower.z, size)},
                     body_faces[0], kBodyColor, lower_turning);
                emit(Vec3{cubie_center(upper.x, size),
                          cubie_center(upper.y, size),
                          cubie_center(upper.z, size)},
                     body_faces[1], kBodyColor, upper_turning);
            }
        }
    }

    // One caster per unbroken run of layers along the turning axis, the
    // turning runs carried round with their stickers. Only this axis is cut:
    // the other two do not move in this turn, so there is nothing to split.
    // A run mask makes three at most -- still, turning, still -- and a mask
    // with gaps in it, which nothing here produces, makes one more per gap.
    const float half_pitch = kCubeHalfExtent / static_cast<float>(size);
    int run_start = 0;
    for (int index = 1; index <= size; ++index) {
        const bool run_turning = in_layers(active->layers, run_start);
        if (index < size && in_layers(active->layers, index) == run_turning) {
            continue;
        }

        Caster caster =
            slab(active->axis, cubie_center(run_start, size) - half_pitch,
                 cubie_center(index - 1, size) + half_pitch);
        if (run_turning) {
            for (auto& corner : caster.corners) {
                corner = math::quaternion_rotate(turn, corner);
            }
        }
        scene.casters.push_back(caster);
        run_start = index;
    }
    return scene;
}

}  // namespace rubiks::graphics
