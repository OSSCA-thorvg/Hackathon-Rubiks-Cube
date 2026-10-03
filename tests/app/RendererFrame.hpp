#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "EngineLifecycle.hpp"
#include "graphics/CanvasTheme.hpp"
#include "graphics/Color.hpp"
#include "graphics/RenderScene.hpp"
#include "math/Types.hpp"
#include "render/ThorVGSoftwareRenderer.hpp"

/**
 * A renderer of a test's own, and the frame it leaves.
 *
 * For the tests of how the renderer composes a scene out of ThorVG's parts.
 * They build a RenderScene by hand, draw it with a renderer made beside the
 * application's rather than through the application, and read the frame back
 * pixel by pixel, so the composition is asserted rather than eyeballed.
 *
 * Beside the application's tests: draw() asserts, and this is the only
 * directory whose tests link the renderer.
 */
namespace rubiks::test {

/**
 * The two grounds a scene is drawn over.
 *
 * Asked of the engine rather than written out again. In these tests a ground
 * is what the renderer is handed, not what it is expected to produce, so
 * nothing is checked against itself; what the two grounds are, byte for byte,
 * is pinned by the render contract.
 */
inline const graphics::Color kDark =
    graphics::canvas_background(graphics::CanvasTheme::Dark);
inline const graphics::Color kLight =
    graphics::canvas_background(graphics::CanvasTheme::Light);

/** A square frame and the renderer that holds its pixels. */
struct Frame {
    std::unique_ptr<render::ThorVGSoftwareRenderer> renderer;
    std::uint32_t side = 0;

    [[nodiscard]] Rgba at(std::uint32_t x, std::uint32_t y) const
    {
        const auto* bytes =
            reinterpret_cast<const std::uint8_t*>(renderer->pixel_buffer());
        const std::size_t offset = (static_cast<std::size_t>(y) * side + x) * 4;
        return Rgba{bytes[offset], bytes[offset + 1], bytes[offset + 2],
                    bytes[offset + 3]};
    }

    [[nodiscard]] int luminance(std::uint32_t x, std::uint32_t y) const
    {
        const Rgba pixel = at(x, y);
        return int{pixel[0]} + int{pixel[1]} + int{pixel[2]};
    }

    [[nodiscard]] bool same_as(const Frame& other) const
    {
        if (side != other.side) return false;

        const auto* a =
            reinterpret_cast<const std::uint8_t*>(renderer->pixel_buffer());
        const auto* b =
            reinterpret_cast<const std::uint8_t*>(other.renderer->pixel_buffer());
        return std::equal(a, a + static_cast<std::size_t>(side) * side * 4, b);
    }
};

/**
 * Renders a scene on a square canvas, over a ground colour.
 *
 * The engine has to be up for ThorVG, so a case holds an EngineLifecycle
 * before it calls this.
 */
[[nodiscard]] inline Frame draw(const graphics::RenderScene& scene,
                                std::uint32_t side,
                                graphics::Color ground = kDark)
{
    Frame frame{render::ThorVGSoftwareRenderer::create(side, side), side};
    REQUIRE(frame.renderer);
    frame.renderer->set_background(ground);
    REQUIRE(frame.renderer->render(scene));
    return frame;
}

/** An axis-aligned rectangle of one colour, as a face of a scene. */
[[nodiscard]] inline graphics::RenderFace quad(float left, float top,
                                               float right, float bottom,
                                               graphics::Color color)
{
    graphics::RenderFace face;
    face.points = {math::Vec2{left, top}, math::Vec2{right, top},
                   math::Vec2{right, bottom}, math::Vec2{left, bottom}};
    face.color = color;
    return face;
}

/** Whether two pixels are one colour, whatever their alpha. */
[[nodiscard]] inline bool same_rgb(const Rgba& a, const Rgba& b)
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

/** Whether a pixel is the ground, with nothing drawn or shaded over it. */
[[nodiscard]] inline bool is_ground(const Rgba& pixel, graphics::Color ground)
{
    return pixel[0] == ground.r && pixel[1] == ground.g && pixel[2] == ground.b;
}

}  // namespace rubiks::test
