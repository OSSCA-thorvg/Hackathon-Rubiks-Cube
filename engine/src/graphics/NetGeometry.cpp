#include "graphics/NetGeometry.hpp"

#include <cstddef>

#include "graphics/Layout.hpp"
#include "graphics/Palette.hpp"
#include "math/Types.hpp"

namespace rubiks::graphics {
namespace {

using cube::Face;

}  // namespace

std::array<Face, 6> net_faces() noexcept
{
    return {Face::Up,    Face::Left, Face::Front,
            Face::Right, Face::Back, Face::Down};
}

NetBlock net_block(Face face) noexcept
{
    // Up on top, the four side faces in a row, Down below:
    //     U
    //   L F R B
    //     D
    switch (face) {
        case Face::Up:
            return NetBlock{1, 0};
        case Face::Left:
            return NetBlock{0, 1};
        case Face::Front:
            return NetBlock{1, 1};
        case Face::Right:
            return NetBlock{2, 1};
        case Face::Back:
            return NetBlock{3, 1};
        case Face::Down:
            break;
    }
    return NetBlock{1, 2};
}

NetCell net_cell(Face face, int col, int row, int size) noexcept
{
    const int last = size - 1;

    switch (face) {
        case Face::Up:
            return NetCell{col, last, row, Face::Up};
        case Face::Left:
            return NetCell{0, last - row, col, Face::Left};
        case Face::Front:
            return NetCell{col, last - row, last, Face::Front};
        case Face::Right:
            return NetCell{last, last - row, last - col, Face::Right};
        case Face::Back:
            return NetCell{last - col, last - row, 0, Face::Back};
        case Face::Down:
            break;
    }
    return NetCell{col, 0, last - row, Face::Down};
}

RenderScene build_net_scene(const cube::CubeState& state, const Rect& rect)
{
    const int size = state.size();
    const float face_side = rect.width / static_cast<float>(kNetColumns);
    const float cell = face_side / static_cast<float>(size);
    const float inset = cell * (1.0f - kNetStickerScale) * 0.5f;
    const float sticker = cell * kNetStickerScale;

    RenderScene scene;
    scene.faces.reserve(static_cast<std::size_t>(6 * size * size));

    for (const auto face : net_faces()) {
        const auto block = net_block(face);
        const float origin_x =
            rect.x + static_cast<float>(block.column) * face_side;
        const float origin_y =
            rect.y + static_cast<float>(block.row) * face_side;

        for (int row = 0; row < size; ++row) {
            for (int col = 0; col < size; ++col) {
                const auto source = net_cell(face, col, row, size);
                const auto& cubie = state.at(source.x, source.y, source.z);

                const float left =
                    origin_x + static_cast<float>(col) * cell + inset;
                const float top =
                    origin_y + static_cast<float>(row) * cell + inset;

                RenderFace quad;
                quad.color = to_color(cubie.sticker(source.face));
                quad.points = {math::Vec2{left, top},
                               math::Vec2{left + sticker, top},
                               math::Vec2{left + sticker, top + sticker},
                               math::Vec2{left, top + sticker}};
                scene.faces.push_back(quad);
            }
        }
    }
    return scene;
}

}  // namespace rubiks::graphics
