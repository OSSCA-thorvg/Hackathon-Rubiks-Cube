#pragma once

#include <cstdint>
#include <memory>

#include <thorvg.h>

#include "render/Renderer.hpp"

namespace rubiks::render {

/**
 * Software renderer that owns a ThorVG SwCanvas and its pixel buffer.
 *
 * The pixel buffer lives in engine memory, uses tvg::ColorSpace::ABGR8888S,
 * and stays valid from a successful create() or resize() until the next
 * resize() or destruction. Callers borrow it read-only through
 * pixel_buffer() and pixel_byte_length().
 */
class ThorVGSoftwareRenderer : public Renderer {
public:
    /** Largest accepted width or height in pixels. */
    static constexpr std::uint32_t kMaxDimension = 8192;

    /**
     * Validates dimensions against zero, kMaxDimension, and byte-size
     * overflow. Shared by create(), resize(), and the application boundary.
     */
    [[nodiscard]] static bool valid_dimensions(std::uint32_t width,
                                               std::uint32_t height) noexcept;

    /**
     * Creates a renderer with a valid target and the fixed Phase 1 scene.
     *
     * @return nullptr when validation, allocation, or ThorVG setup fails.
     */
    [[nodiscard]] static std::unique_ptr<ThorVGSoftwareRenderer> create(
        std::uint32_t width, std::uint32_t height) noexcept;

    ~ThorVGSoftwareRenderer() override;

    ThorVGSoftwareRenderer(const ThorVGSoftwareRenderer&) = delete;
    ThorVGSoftwareRenderer& operator=(const ThorVGSoftwareRenderer&) = delete;

    /**
     * Replaces the pixel buffer and canvas target with a new size.
     *
     * Follows the resize failure semantics of the Phase 1 contract: the
     * previous target survives validation and allocation failures, a failed
     * target swap is rolled back, and a failed rollback leaves the renderer
     * unusable until it is destroyed and recreated.
     */
    [[nodiscard]] bool resize(std::uint32_t width,
                              std::uint32_t height) noexcept override;

    [[nodiscard]] bool render() noexcept override;

    [[nodiscard]] std::uintptr_t pixel_buffer() const noexcept override;

    [[nodiscard]] std::uint32_t pixel_byte_length() const noexcept override;

protected:
    ThorVGSoftwareRenderer() = default;

    /**
     * Builds the canvas, buffer, target, and scene for the initial size.
     *
     * Called by create(); test subclasses call it directly after
     * constructing themselves with fault-injecting overrides in place.
     */
    [[nodiscard]] bool init(std::uint32_t width, std::uint32_t height) noexcept;

    // Fault-injection seams: the only operations that can fail after
    // validation. Tests override these to reach the resize rollback
    // branches deterministically; both are called only during init() and
    // resize(), never on the per-frame render path.

    /** Allocates a pixel buffer, nullptr on failure (-fno-exceptions). */
    [[nodiscard]] virtual std::uint32_t* allocate_pixels(
        std::uint32_t width, std::uint32_t height) noexcept;

    /** Points the canvas at a buffer; false when ThorVG rejects it. */
    [[nodiscard]] virtual bool set_target(std::uint32_t* buffer,
                                          std::uint32_t width,
                                          std::uint32_t height) noexcept;

private:
    [[nodiscard]] bool layout_scene() noexcept;

    tvg::SwCanvas* canvas_ = nullptr;
    tvg::Shape* background_ = nullptr;  // owned by canvas_
    tvg::Shape* rectangle_ = nullptr;   // owned by canvas_
    std::uint32_t* buffer_ = nullptr;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    bool usable_ = false;
};

}  // namespace rubiks::render
