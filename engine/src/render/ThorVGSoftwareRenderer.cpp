#include "render/ThorVGSoftwareRenderer.hpp"

#include <cstddef>
#include <limits>
#include <new>

namespace rubiks::render {
namespace {

constexpr std::uint32_t kBytesPerPixel = 4;

// Rendered scene contract colors; native and browser tests assert them.
constexpr std::uint8_t kBackgroundColor[4] = {32, 32, 32, 255};
constexpr std::uint8_t kRectangleColor[4] = {230, 57, 70, 255};

tvg::Shape* add_shape(tvg::SwCanvas& canvas) noexcept
{
    auto* shape = tvg::Shape::gen();
    if (!shape) return nullptr;

    if (canvas.add(shape) != tvg::Result::Success) {
        // Ownership was not transferred to the canvas.
        static_cast<void>(shape->unref());
        return nullptr;
    }

    return shape;
}

}  // namespace

bool ThorVGSoftwareRenderer::valid_dimensions(std::uint32_t width,
                                              std::uint32_t height) noexcept
{
    if (width == 0 || height == 0) return false;
    if (width > kMaxDimension || height > kMaxDimension) return false;

    const auto bytes =
        static_cast<std::uint64_t>(width) * height * kBytesPerPixel;
    return bytes <= std::numeric_limits<std::uint32_t>::max();
}

std::unique_ptr<ThorVGSoftwareRenderer> ThorVGSoftwareRenderer::create(
    std::uint32_t width, std::uint32_t height) noexcept
{
    std::unique_ptr<ThorVGSoftwareRenderer> renderer(
        new (std::nothrow) ThorVGSoftwareRenderer());
    if (!renderer) return nullptr;

    if (!renderer->init(width, height)) return nullptr;
    return renderer;
}

bool ThorVGSoftwareRenderer::init(std::uint32_t width,
                                  std::uint32_t height) noexcept
{
    if (!valid_dimensions(width, height)) return false;

    canvas_ = tvg::SwCanvas::gen();
    if (!canvas_) return false;

    buffer_ = allocate_pixels(width, height);
    if (!buffer_) return false;

    if (!set_target(buffer_, width, height)) return false;
    width_ = width;
    height_ = height;

    background_ = add_shape(*canvas_);
    if (!background_) return false;

    rectangle_ = add_shape(*canvas_);
    if (!rectangle_) return false;

    if (!layout_scene()) return false;

    usable_ = true;
    return true;
}

std::uint32_t* ThorVGSoftwareRenderer::allocate_pixels(
    std::uint32_t width, std::uint32_t height) noexcept
{
    // -fno-exceptions: allocation failure must surface as nullptr.
    return new (std::nothrow)
        std::uint32_t[static_cast<std::size_t>(width) * height];
}

ThorVGSoftwareRenderer::~ThorVGSoftwareRenderer()
{
    delete canvas_;
    delete[] buffer_;
}

bool ThorVGSoftwareRenderer::resize(std::uint32_t width,
                                    std::uint32_t height) noexcept
{
    if (!usable_) return false;
    if (!valid_dimensions(width, height)) return false;
    if (width == width_ && height == height_) return true;

    auto* next = allocate_pixels(width, height);
    if (!next) return false;  // ThorVG untouched; current target preserved.

    if (!set_target(next, width, height)) {
        delete[] next;

        // The failed call may have partially updated the surface, so the
        // previous target must be restored explicitly.
        if (!set_target(buffer_, width_, height_)) {
            delete[] buffer_;
            buffer_ = nullptr;
            width_ = 0;
            height_ = 0;
            usable_ = false;
        }
        return false;
    }

    delete[] buffer_;
    buffer_ = next;
    width_ = width;
    height_ = height;

    if (!layout_scene()) {
        usable_ = false;
        return false;
    }
    return true;
}

bool ThorVGSoftwareRenderer::render() noexcept
{
    if (!usable_) return false;

    if (canvas_->update() != tvg::Result::Success) return false;
    if (canvas_->draw(true) != tvg::Result::Success) return false;
    return canvas_->sync() == tvg::Result::Success;
}

std::uintptr_t ThorVGSoftwareRenderer::pixel_buffer() const noexcept
{
    if (!usable_) return 0;
    return reinterpret_cast<std::uintptr_t>(buffer_);
}

std::uint32_t ThorVGSoftwareRenderer::pixel_byte_length() const noexcept
{
    if (!usable_) return 0;
    return width_ * height_ * kBytesPerPixel;
}

bool ThorVGSoftwareRenderer::set_target(std::uint32_t* buffer,
                                        std::uint32_t width,
                                        std::uint32_t height) noexcept
{
    // The stride is a pixel count per row, not a byte count.
    return canvas_->target(buffer, width, width, height,
                           tvg::ColorSpace::ABGR8888S) == tvg::Result::Success;
}

bool ThorVGSoftwareRenderer::layout_scene() noexcept
{
    const auto width = static_cast<float>(width_);
    const auto height = static_cast<float>(height_);

    if (background_->reset() != tvg::Result::Success) return false;
    if (background_->appendRect(0.0f, 0.0f, width, height) !=
        tvg::Result::Success) {
        return false;
    }
    if (background_->fill(kBackgroundColor[0], kBackgroundColor[1],
                          kBackgroundColor[2], kBackgroundColor[3]) !=
        tvg::Result::Success) {
        return false;
    }

    // Centered rectangle whose sides are 50% of the drawing buffer.
    if (rectangle_->reset() != tvg::Result::Success) return false;
    if (rectangle_->appendRect(width * 0.25f, height * 0.25f, width * 0.5f,
                               height * 0.5f) != tvg::Result::Success) {
        return false;
    }
    if (rectangle_->fill(kRectangleColor[0], kRectangleColor[1],
                         kRectangleColor[2], kRectangleColor[3]) !=
        tvg::Result::Success) {
        return false;
    }

    return true;
}

}  // namespace rubiks::render
