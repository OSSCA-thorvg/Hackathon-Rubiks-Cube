#include "cube/Surface.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <map>

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

std::size_t painting_index(const SurfaceSticker& sticker, int size) noexcept
{
    assert(is_surface_sticker(sticker, size));
    const std::array<Axis, 2> across_down = other_axes(axis_of(sticker.face));
    const int col = coordinate_on(across_down[0], sticker);
    const int row = coordinate_on(across_down[1], sticker);
    return face_index(sticker.face) * static_cast<std::size_t>(size * size) +
           static_cast<std::size_t>(row * size + col);
}

SurfaceSticker SurfaceCells::Iterator::operator*() const noexcept
{
    const std::size_t area = static_cast<std::size_t>(size_ * size_);
    const Face face = static_cast<Face>(index_ / area);
    const int within = static_cast<int>(index_ % area);
    return face_sticker(face, within / size_, within % size_, size_);
}

LayerCells::Iterator::Iterator(Axis axis, LayerMask layers, int size, bool end) noexcept
    : axis_(axis), layers_(layers), size_(size), face_(end ? kFaceCount : 0)
{
    if (size < kMinCubeSize || size >= kLayerMaskBits || layers == 0) face_ = kFaceCount;
    seek();
}

bool LayerCells::Iterator::row_along_axis() const noexcept
{
    return detail::kFaceBases[face_].row == axis_;
}

void LayerCells::Iterator::seek() noexcept
{
    while (face_ < kFaceCount) {
        const Face face = static_cast<Face>(face_);
        if (axis_of(face) == axis_) {
            if (offset_ < size_ * size_ && in_layers(layers_, outer_layer(face, size_))) return;
        } else {
            while (line_ < size_) {
                const SurfaceSticker first = row_along_axis()
                    ? face_sticker(face, line_, 0, size_)
                    : face_sticker(face, 0, line_, size_);
                if (in_layers(layers_, coordinate_on(axis_, first))) return;
                ++line_;
            }
        }
        ++face_;
        line_ = 0;
        offset_ = 0;
    }
}

SurfaceSticker LayerCells::Iterator::operator*() const noexcept
{
    const Face face = static_cast<Face>(face_);
    if (axis_of(face) == axis_) return face_sticker(face, offset_ / size_, offset_ % size_, size_);
    return row_along_axis() ? face_sticker(face, line_, offset_, size_)
                            : face_sticker(face, offset_, line_, size_);
}

LayerCells::Iterator& LayerCells::Iterator::operator++() noexcept
{
    ++offset_;
    if (axis_of(static_cast<Face>(face_)) != axis_ && offset_ == size_) {
        offset_ = 0;
        ++line_;
    }
    seek();
    return *this;
}

bool LayerCells::Iterator::operator==(const Iterator& other) const noexcept
{
    return axis_ == other.axis_ && layers_ == other.layers_ && size_ == other.size_ &&
           face_ == other.face_ && line_ == other.line_ && offset_ == other.offset_;
}

const std::vector<SurfaceTurn>& surface_turns(Axis axis, int slice, int size)
{
    static const std::vector<SurfaceTurn> empty;
    if (axis_index(axis) >= kAxisCount || size < kMinCubeSize ||
        size >= kLayerMaskBits || slice < 0 || slice >= size) return empty;
    using LayerTurns = std::vector<std::vector<SurfaceTurn>>;
    using AxisTurns = std::array<LayerTurns, kAxisCount>;
    static thread_local std::map<int, AxisTurns> known;
    std::map<int, AxisTurns>::iterator found = known.find(size);
    if (found == known.end()) {
        AxisTurns permutations;
        for (const Axis along : axes()) {
            LayerTurns& layers = permutations[axis_index(along)];
            layers.resize(static_cast<std::size_t>(size));
            for (int index = 0; index < size; ++index) {
                std::vector<SurfaceTurn>& turns = layers[static_cast<std::size_t>(index)];
                for (const SurfaceSticker from : layer_cells(along, layer(index), size)) {
                    turns.push_back(SurfaceTurn{surface_index(from, size),
                        surface_index(turned_sticker(from, along, size), size)});
                }
            }
        }
        found = known.emplace(size, std::move(permutations)).first;
    }
    return found->second[axis_index(axis)][static_cast<std::size_t>(slice)];
}

LayerMask depth_layers(Face face, int first_depth, int last_depth,
                       int size) noexcept
{
    if (size <= 0 || size >= kLayerMaskBits) return 0;
    if (first_depth < 1 || last_depth < first_depth || last_depth > size) {
        return 0;
    }

    // Counting inwards from a face at the far end of its axis is counting
    // down through the indices, so the run's two ends swap over with it.
    if (is_positive_face(face)) {
        return layers_through(size - last_depth, size - first_depth);
    }
    return layers_through(first_depth - 1, last_depth - 1);
}

CubeMove face_move(Face face, int first_depth, int last_depth, int turns,
                   int size) noexcept
{
    return CubeMove{axis_of(face),
                    depth_layers(face, first_depth, last_depth, size),
                    is_positive_face(face) ? turns : -turns};
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

Face turned_face(Axis axis, Face face, int quarter_turns) noexcept
{
    for (int turn = 0, count = normalized_turns(quarter_turns); turn < count; ++turn) {
        face = turned_face(axis, face);
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
        const auto [across_axis, down_axis] = other_axes(normal);
        for (int down = 0; down < size; ++down) {
            for (int across = 0; across < size; ++across) {
                CubiePosition position{0, 0, 0};
                set_coordinate(position, normal, outer_layer(face, size));
                set_coordinate(position, across_axis, across);
                set_coordinate(position, down_axis, down);
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
    if (size < kMinCubeSize || layer < 0 || layer >= size) return slots;

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
