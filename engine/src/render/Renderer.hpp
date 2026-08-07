#pragma once

#include <cstdint>

namespace rubiks::render {

/**
 * Rendering boundary owned by the application.
 *
 * Implementations own their render target. The application never observes
 * target allocation, pointers, or pixel formats through this interface.
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
     * @return true when the frame fully reached the target.
     */
    [[nodiscard]] virtual bool render() noexcept = 0;

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
