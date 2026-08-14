#include "render/ThorVGSoftwareRenderer.hpp"

#include <cstddef>
#include <limits>
#include <new>

namespace rubiks::render {
namespace {

constexpr std::uint32_t kBytesPerPixel = 4;

// Clear color behind the scene; native and browser tests assert it as part
// of the rendered scene contract. Face colors belong to the scene, but the
// background is a property of the target rather than of the geometry.
constexpr std::uint8_t kBackgroundColor[4] = {32, 32, 32, 255};

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

    // -fno-exceptions: allocation failure must surface as nullptr.
    buffer_ = new (std::nothrow)
        std::uint32_t[static_cast<std::size_t>(width) * height];
    if (!buffer_) return false;

    if (!set_target(buffer_, width, height)) return false;
    width_ = width;
    height_ = height;

    usable_ = true;
    return true;
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

    auto* next = new (std::nothrow)
        std::uint32_t[static_cast<std::size_t>(width) * height];
    if (!next) return false;  // ThorVG untouched; current target preserved.

    if (!set_target(next, width, height)) {
        // The failed call may have partially updated the surface, and the
        // only caller treats any resize failure as fatal, so this renderer
        // is done rather than rolled back.
        delete[] next;
        delete[] buffer_;
        buffer_ = nullptr;
        width_ = 0;
        height_ = 0;
        usable_ = false;
        return false;
    }

    delete[] buffer_;
    buffer_ = next;
    width_ = width;
    height_ = height;
    return true;
}

bool ThorVGSoftwareRenderer::render(
    const graphics::RenderScene& scene) noexcept
{
    if (!usable_) return false;

    if (!rebuild_canvas(scene)) return false;

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

bool ThorVGSoftwareRenderer::rebuild_canvas(
    const graphics::RenderScene& scene) noexcept
{
    // Face count and geometry change every frame, so the canvas is rebuilt
    // rather than diffed. At this scene size the cost is not measurable, and
    // reusing shapes would be an optimization with no correctness benefit.
    if (canvas_->remove() != tvg::Result::Success) return false;

    auto* background = add_shape(*canvas_);
    if (!background) return false;
    if (background->appendRect(0.0f, 0.0f, static_cast<float>(width_),
                               static_cast<float>(height_)) !=
        tvg::Result::Success) {
        return false;
    }
    if (background->fill(kBackgroundColor[0], kBackgroundColor[1],
                         kBackgroundColor[2], kBackgroundColor[3]) !=
        tvg::Result::Success) {
        return false;
    }

    for (const auto& face : scene.faces) {
        auto* shape = add_shape(*canvas_);
        if (!shape) return false;

        if (shape->moveTo(face.points[0].x, face.points[0].y) !=
            tvg::Result::Success) {
            return false;
        }
        for (std::size_t i = 1; i < face.points.size(); ++i) {
            if (shape->lineTo(face.points[i].x, face.points[i].y) !=
                tvg::Result::Success) {
                return false;
            }
        }
        if (shape->close() != tvg::Result::Success) return false;

        if (shape->fill(face.color.r, face.color.g, face.color.b,
                        face.color.a) != tvg::Result::Success) {
            return false;
        }
    }

    // Strokes last, so a guide line reads on top of the stickers it crosses.
    for (const auto& path : scene.strokes) {
        auto* shape = add_shape(*canvas_);
        if (!shape) return false;

        if (shape->moveTo(path.start.x, path.start.y) !=
            tvg::Result::Success) {
            return false;
        }
        for (const auto& segment : path.segments) {
            if (shape->cubicTo(segment.control_a.x, segment.control_a.y,
                               segment.control_b.x, segment.control_b.y,
                               segment.to.x,
                               segment.to.y) != tvg::Result::Success) {
                return false;
            }
        }
        if (path.closed && shape->close() != tvg::Result::Success) {
            return false;
        }

        // No fill() call: a shape starts fully transparent, so the curve is
        // the line and nothing else.
        if (shape->strokeWidth(path.width) != tvg::Result::Success) {
            return false;
        }
        if (shape->strokeFill(path.color.r, path.color.g, path.color.b,
                              path.color.a) != tvg::Result::Success) {
            return false;
        }
        if (shape->strokeCap(tvg::StrokeCap::Round) != tvg::Result::Success) {
            return false;
        }
        if (shape->strokeJoin(tvg::StrokeJoin::Round) !=
            tvg::Result::Success) {
            return false;
        }
    }

    return true;
}

}  // namespace rubiks::render
