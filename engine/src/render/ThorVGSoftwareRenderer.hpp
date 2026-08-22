#pragma once

#include <cstdint>
#include <memory>

#include <thorvg.h>

#include "graphics/Color.hpp"
#include "graphics/RenderScene.hpp"
#include "render/Renderer.hpp"

namespace rubiks::render {

/**
 * Software renderer that owns a ThorVG SwCanvas and its pixel buffer.
 *
 * The pixel buffer lives in engine memory, uses tvg::ColorSpace::ABGR8888S,
 * and stays valid from a successful create() or resize() until the next
 * resize() or destruction. Callers borrow it read-only through
 * pixel_buffer() and pixel_byte_length().
 *
 * The canvas contents are rebuilt from the RenderScene on every frame, so
 * the renderer holds no scene state between frames.
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
     * The previous target survives validation and allocation failures, which
     * happen before anything is touched. A failed target swap leaves the
     * renderer unusable until it is destroyed and recreated; the only caller
     * treats any resize failure as fatal, so nothing rolls back. A successful
     * resize does not touch scene state, because the scene arrives with each
     * frame.
     */
    [[nodiscard]] bool resize(std::uint32_t width,
                              std::uint32_t height) noexcept override;

    [[nodiscard]] bool render(
        const graphics::RenderScene& scene) noexcept override;

    /**
     * Sets the opaque color every frame after this one is cleared to.
     *
     * Takes effect on the next render rather than immediately: the canvas
     * holds nothing between frames, so there is no painted background here
     * to go back and change.
     */
    void set_background(graphics::Color color) noexcept override;

    [[nodiscard]] std::uintptr_t pixel_buffer() const noexcept override;

    [[nodiscard]] std::uint32_t pixel_byte_length() const noexcept override;

private:
    ThorVGSoftwareRenderer() = default;

    /** Builds the canvas, buffer, and target for the initial size. */
    [[nodiscard]] bool init(std::uint32_t width, std::uint32_t height) noexcept;

    /** Points the canvas at a buffer; false when ThorVG rejects it. */
    [[nodiscard]] bool set_target(std::uint32_t* buffer, std::uint32_t width,
                                  std::uint32_t height) noexcept;

    /** Replaces the canvas contents with the background and the scene faces. */
    [[nodiscard]] bool rebuild_canvas(
        const graphics::RenderScene& scene) noexcept;

    // Dark by default, which is the ground the renderer has always had and
    // the one the native build with no UI in front of it still wants.
    graphics::Color background_{32, 32, 32, 255};

    tvg::SwCanvas* canvas_ = nullptr;
    std::uint32_t* buffer_ = nullptr;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    bool usable_ = false;
};

}  // namespace rubiks::render
