#pragma once

#include <cstdint>

#include "graphics/Color.hpp"
#include "graphics/RenderScene.hpp"

namespace rubiks::render {

/**
 * Rendering boundary owned by the application.
 *
 * Implementations own their render target. The application never observes
 * target allocation, pointers, or pixel formats through this interface, and
 * hands over a finished 2D RenderScene rather than anything 3D.
 */
class Renderer {
public:
    virtual ~Renderer() = default;

    /**
     * Resizes the render target.
     *
     * @return true when the target uses the new size afterwards.
     */
    [[nodiscard]] virtual bool resize(std::uint32_t width,
                                      std::uint32_t height) noexcept = 0;

    /**
     * Renders one frame into the render target.
     *
     * Faces are drawn in the order given, which the pipeline has already
     * sorted back to front.
     *
     * @return true when the frame fully reached the target.
     */
    [[nodiscard]] virtual bool render(
        const graphics::RenderScene& scene) noexcept = 0;

    /**
     * Sets the opaque background the next frames are cleared to.
     *
     * On the boundary rather than on the software renderer alone, because
     * the application owns its renderer through this interface and could
     * not reach a method the concrete class kept to itself. A backend with
     * no background of its own is not obliged to have one, so the default
     * accepts the color and does nothing with it -- the same shape the
     * pixel-buffer accessors below already take.
     *
     * The renderer is told a color rather than a theme: a theme is the
     * language of presentation, and a renderer is the tool that paints a
     * target.
     */
    virtual void set_background(graphics::Color color) noexcept
    {
        static_cast<void>(color);
    }

    /**
     * Returns the address of the CPU-visible pixel buffer.
     *
     * @return zero for backends without a CPU-visible pixel buffer.
     */
    [[nodiscard]] virtual std::uintptr_t pixel_buffer() const noexcept
    {
        return 0;
    }

    /**
     * Returns the byte length of the CPU-visible pixel buffer.
     *
     * @return zero for backends without a CPU-visible pixel buffer.
     */
    [[nodiscard]] virtual std::uint32_t pixel_byte_length() const noexcept
    {
        return 0;
    }
};

}  // namespace rubiks::render
