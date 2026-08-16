#include "cube/Surface.hpp"

#include <array>
#include <cstddef>

namespace rubiks::cube {
namespace {

/** An integer direction; only the six axis directions ever appear. */
struct Vector3 {
    int x;
    int y;
    int z;
};

[[nodiscard]] Vector3 axis_vector(Axis axis) noexcept
{
    switch (axis) {
        case Axis::X:
            return Vector3{1, 0, 0};
        case Axis::Y:
            return Vector3{0, 1, 0};
        case Axis::Z:
            break;
    }
    return Vector3{0, 0, 1};
}

/** Whether a face is the one at the far end of its axis. */
[[nodiscard]] bool is_positive_face(Face face) noexcept
{
    switch (face) {
        case Face::Right:
        case Face::Up:
        case Face::Front:
            return true;
        case Face::Left:
        case Face::Down:
        case Face::Back:
            break;
    }
    return false;
}

[[nodiscard]] Vector3 face_normal(Face face) noexcept
{
    const auto along = axis_vector(axis_of(face));
    const int sign = is_positive_face(face) ? 1 : -1;
    return Vector3{along.x * sign, along.y * sign, along.z * sign};
}

[[nodiscard]] Vector3 cross_product(const Vector3& a, const Vector3& b) noexcept
{
    return Vector3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                   a.x * b.y - a.y * b.x};
}

/**
 * The faces a positive quarter turn cycles, each entry moving to the next.
 *
 * The two faces perpendicular to the axis keep their color, which is why they
 * are absent here.
 */
[[nodiscard]] std::array<Face, 4> sticker_cycle(Axis axis) noexcept
{
    switch (axis) {
        case Axis::X:
            return {Face::Up, Face::Back, Face::Down, Face::Front};
        case Axis::Y:
            return {Face::Front, Face::Left, Face::Back, Face::Right};
        case Axis::Z:
            break;
    }
    return {Face::Up, Face::Right, Face::Down, Face::Left};
}

void set_coordinate(CubiePosition& p, Axis axis, int value) noexcept
{
    switch (axis) {
        case Axis::X:
            p.x = value;
            return;
        case Axis::Y:
            p.y = value;
            return;
        case Axis::Z:
            break;
    }
    p.z = value;
}

/** The axis a direction lies along, together with which way it points. */
struct SignedAxis {
    Axis axis;
    int sign;
};

[[nodiscard]] SignedAxis signed_axis(const Vector3& v) noexcept
{
    if (v.x != 0) return SignedAxis{Axis::X, v.x > 0 ? 1 : -1};
    if (v.y != 0) return SignedAxis{Axis::Y, v.y > 0 ? 1 : -1};
    return SignedAxis{Axis::Z, v.z > 0 ? 1 : -1};
}

}  // namespace

Axis axis_of(Face face) noexcept
{
    switch (face) {
        case Face::Right:
        case Face::Left:
            return Axis::X;
        case Face::Up:
        case Face::Down:
            return Axis::Y;
        case Face::Front:
        case Face::Back:
            break;
    }
    return Axis::Z;
}

int outer_layer(Face face, int size) noexcept
{
    return is_positive_face(face) ? size - 1 : 0;
}

LayerMask depth_layers(Face face, int first_depth, int last_depth,
                       int size) noexcept
{
    if (size <= 0 || size >= 32) return 0;
    if (first_depth < 1 || last_depth < first_depth || last_depth > size) {
        return 0;
    }

    // Counting inwards from a face at the far end of its axis is counting
    // down through the indices, so the run's two ends swap over with it.
    if (outer_layer(face, size) == size - 1) {
        return layers_through(size - last_depth, size - first_depth);
    }
    return layers_through(first_depth - 1, last_depth - 1);
}

int coordinate_on(Axis axis, const CubiePosition& p) noexcept
{
    switch (axis) {
        case Axis::X:
            return p.x;
        case Axis::Y:
            return p.y;
        case Axis::Z:
            break;
    }
    return p.z;
}

int coordinate_on(Axis axis, const SurfaceSticker& s) noexcept
{
    return coordinate_on(axis, CubiePosition{s.x, s.y, s.z});
}

CubiePosition turned_position(Axis axis, const CubiePosition& p,
                              int size) noexcept
{
    // These three mappings, together with the face cycle above, are the whole
    // move engine. They are verified against a physical cube by the
    // known-answer test: on a solved 3x3x3, R sends the UFR cubie to the UBR
    // slot showing green on top and white at the back.
    const int last = size - 1;

    switch (axis) {
        case Axis::X:
            return CubiePosition{p.x, p.z, last - p.y};
        case Axis::Y:
            return CubiePosition{last - p.z, p.y, p.x};
        case Axis::Z:
            break;
    }
    return CubiePosition{p.y, last - p.x, p.z};
}

Face turned_face(Axis axis, Face face) noexcept
{
    if (axis_of(face) == axis) return face;

    const auto cycle = sticker_cycle(axis);
    for (std::size_t i = 0; i < cycle.size(); ++i) {
        if (cycle[i] == face) return cycle[(i + 1) % cycle.size()];
    }
    return face;
}

SurfaceSticker turned_sticker(const SurfaceSticker& sticker, Axis axis,
                              int size) noexcept
{
    const auto to =
        turned_position(axis, CubiePosition{sticker.x, sticker.y, sticker.z},
                        size);
    return SurfaceSticker{to.x, to.y, to.z, turned_face(axis, sticker.face)};
}

std::vector<SurfaceSticker> surface_stickers(int size)
{
    std::vector<SurfaceSticker> stickers;
    if (size < 1) return stickers;

    stickers.reserve(static_cast<std::size_t>(6 * size * size));

    for (const Face face : faces()) {
        const Axis normal = axis_of(face);

        // The face pins its own axis; the other two run over the whole face.
        for (int down = 0; down < size; ++down) {
            for (int across = 0; across < size; ++across) {
                CubiePosition position{0, 0, 0};
                set_coordinate(position, normal, outer_layer(face, size));

                bool first = true;
                for (const Axis axis : {Axis::X, Axis::Y, Axis::Z}) {
                    if (axis == normal) continue;
                    set_coordinate(position, axis, first ? across : down);
                    first = false;
                }
                stickers.push_back(SurfaceSticker{position.x, position.y,
                                                  position.z, face});
            }
        }
    }
    return stickers;
}

std::vector<SurfaceSticker> ring_slots(Axis axis, int layer, int size)
{
    std::vector<SurfaceSticker> slots;
    if (size < 2 || layer < 0 || layer >= size) return slots;

    const auto cycle = sticker_cycle(axis);
    const Face first = cycle.front();

    // A sticker crosses its face along `normal x axis`. That is the part of
    // the rotation's velocity that stays on the surface, and it points this
    // way rather than the other because the domain's positive turn is
    // clockwise seen from the positive end of the axis, so the angular
    // velocity runs the opposite way along it.
    const auto travel =
        cross_product(face_normal(first), axis_vector(axis));
    const auto along = signed_axis(travel);

    slots.reserve(static_cast<std::size_t>(4 * size));

    // One face's worth in travel order. The rest of the ring is this line
    // carried round by the turn, which is what puts slot i + N exactly one
    // quarter turn ahead of slot i.
    for (int step = 0; step < size; ++step) {
        CubiePosition position{0, 0, 0};
        set_coordinate(position, axis, layer);
        set_coordinate(position, axis_of(first), outer_layer(first, size));
        set_coordinate(position, along.axis,
                       along.sign > 0 ? step : size - 1 - step);
        slots.push_back(
            SurfaceSticker{position.x, position.y, position.z, first});
    }

    for (std::size_t slot = static_cast<std::size_t>(size);
         slot < static_cast<std::size_t>(4 * size); ++slot) {
        slots.push_back(turned_sticker(slots[slot - static_cast<std::size_t>(size)],
                                       axis, size));
    }
    return slots;
}

}  // namespace rubiks::cube
