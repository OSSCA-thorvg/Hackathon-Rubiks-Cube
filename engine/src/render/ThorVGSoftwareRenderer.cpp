#include "render/ThorVGSoftwareRenderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <new>
#include <vector>

#include "graphics/CubeGeometry.hpp"
#include "graphics/Light.hpp"

namespace rubiks::render {
namespace {

constexpr std::uint32_t kBytesPerPixel = 4;

using rubiks::math::Vec2;

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

/** A shape added to a scene, which owns it from then on. */
tvg::Shape* add_shape(tvg::Scene& scene) noexcept
{
    auto* shape = tvg::Shape::gen();
    if (!shape) return nullptr;

    if (scene.add(shape) != tvg::Result::Success) {
        static_cast<void>(shape->unref());
        return nullptr;
    }
    return shape;
}

/** A scene added to a canvas, which owns it from then on. */
tvg::Scene* add_scene(tvg::SwCanvas& canvas) noexcept
{
    auto* scene = tvg::Scene::gen();
    if (!scene) return nullptr;

    if (canvas.add(scene) != tvg::Result::Success) {
        static_cast<void>(scene->unref());
        return nullptr;
    }
    return scene;
}

/** Appends a closed polygon to a shape. */
bool append_polygon(tvg::Shape& shape, const std::vector<Vec2>& points) noexcept
{
    if (points.size() < 3) return true;  // nothing to draw, nothing wrong

    if (shape.moveTo(points[0].x, points[0].y) != tvg::Result::Success) {
        return false;
    }
    for (std::size_t i = 1; i < points.size(); ++i) {
        if (shape.lineTo(points[i].x, points[i].y) != tvg::Result::Success) {
            return false;
        }
    }
    return shape.close() == tvg::Result::Success;
}

template <std::size_t N>
bool append_polygon(tvg::Shape& shape, const std::array<Vec2, N>& points) noexcept
{
    return append_polygon(shape, std::vector<Vec2>(points.begin(), points.end()));
}

/**
 * The one place the variadic effect call is made.
 *
 * SceneEffect::add reads its arguments by the types the header documents --
 * a double and three ints for a Gaussian blur -- and a float or a bool where
 * one of those was expected is undefined behaviour rather than a compile
 * error. Pinning the types here keeps that from being written twice.
 */
bool add_gaussian_blur(tvg::Scene& scene, float sigma, int quality) noexcept
{
    const double sigma_arg = std::max(static_cast<double>(sigma), 0.01);
    const int direction = 0;  // both axes
    const int border = 0;     // duplicate the edge
    return scene.add(tvg::SceneEffect::GaussianBlur, sigma_arg, direction,
                     border, quality) == tvg::Result::Success;
}

/** White at `alpha` in the middle, `mid` half way out, nothing at the edge. */
/**
 * A glow over an ellipse: the unit radial falloff carried onto the screen by
 * an affine map whose columns are the ellipse's semi-axes and whose
 * translation is its centre, so a lobe seen at a slant is drawn at the slant.
 */
bool fill_glow(tvg::Shape& shape, const Vec2& centre, const Vec2& axis_a,
               const Vec2& axis_b, const graphics::Color& color,
               const std::array<std::uint8_t, graphics::kGlintStops>& alphas) noexcept
{
    auto* gradient = tvg::RadialGradient::gen();
    if (!gradient) return false;

    if (gradient->radial(0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f) !=
            tvg::Result::Success ||
        gradient->transform(tvg::Matrix{axis_a.x, axis_b.x, centre.x, axis_a.y,
                                        axis_b.y, centre.y, 0.0f, 0.0f, 1.0f}) !=
            tvg::Result::Success) {
        delete gradient;
        return false;
    }
    std::array<tvg::Fill::ColorStop, graphics::kGlintStops> stops{};
    for (std::size_t i = 0; i < stops.size(); ++i) {
        stops[i] = {static_cast<float>(i) / static_cast<float>(stops.size() - 1),
                    color.r, color.g, color.b, alphas[i]};
    }
    if (gradient->colorStops(stops.data(), static_cast<uint32_t>(stops.size())) !=
        tvg::Result::Success) {
        delete gradient;
        return false;
    }
    return shape.fill(gradient) == tvg::Result::Success;
}

/** A face's colour running from one point to another, or flat without shading. */
bool fill_face(tvg::Shape& shape, const graphics::RenderFace& face) noexcept
{
    if (!face.shading) {
        return shape.fill(face.color.r, face.color.g, face.color.b,
                          face.color.a) == tvg::Result::Success;
    }

    const auto& shading = *face.shading;
    auto* gradient = tvg::LinearGradient::gen();
    if (!gradient) return false;
    if (gradient->linear(shading.from.x, shading.from.y, shading.to.x,
                         shading.to.y) != tvg::Result::Success) {
        delete gradient;
        return false;
    }
    // The plane's stops with this sticker's colour at each: the same axis and
    // spacing as every sticker beside it, so the shading runs on across the
    // seams, following the brightness curve stop by stop. The spacing is the
    // projected one when the pipeline supplied it -- perspective does not keep
    // the plane's even spacing -- and even otherwise.
    std::vector<tvg::Fill::ColorStop> stops;
    stops.reserve(shading.colors.size());
    const bool placed = shading.offsets.size() == shading.colors.size();
    for (std::size_t i = 0; i < shading.colors.size(); ++i) {
        const auto& color = shading.colors[i];
        const float even = shading.colors.size() > 1
                               ? static_cast<float>(i) /
                                     static_cast<float>(shading.colors.size() - 1)
                               : 0.0f;
        const float offset = placed ? shading.offsets[i] : even;
        stops.push_back({offset, color.r, color.g, color.b, color.a});
    }
    if (stops.empty() ||
        gradient->colorStops(stops.data(), static_cast<uint32_t>(stops.size())) !=
            tvg::Result::Success ||
        gradient->spread(tvg::FillSpread::Pad) != tvg::Result::Success) {
        delete gradient;
        return false;
    }
    return shape.fill(gradient) == tvg::Result::Success;
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

void ThorVGSoftwareRenderer::set_background(graphics::Color color) noexcept
{
    // No usable_ guard and no failure path: this writes a field the next
    // rebuild reads. A renderer that cannot draw is already refusing to
    // draw, and remembering a color for it costs nothing.
    background_ = color;
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
    // Face colors belong to the scene; the ground behind them is a property
    // of the target, so it is read from the renderer rather than arriving
    // with the geometry.
    if (background->fill(background_.r, background_.g, background_.b,
                         background_.a) != tvg::Result::Success) {
        return false;
    }

    // One shape per path; both stroke layers are drawn the same way, only at
    // different points in the order below.
    const auto stroke_paths =
        [this](const std::vector<graphics::RenderStroke>& paths) {
            for (const auto& path : paths) {
                auto* shape = add_shape(*canvas_);
                if (!shape) return false;

                if (shape->moveTo(path.start.x, path.start.y) !=
                    tvg::Result::Success) {
                    return false;
                }
                for (const auto& segment : path.segments) {
                    if (shape->cubicTo(segment.control_a.x, segment.control_a.y,
                                       segment.control_b.x, segment.control_b.y,
                                       segment.to.x, segment.to.y) !=
                        tvg::Result::Success) {
                        return false;
                    }
                }
                if (path.closed && shape->close() != tvg::Result::Success) {
                    return false;
                }

                // No fill() call: a shape starts fully transparent, so the
                // curve is the line and nothing else.
                if (shape->strokeWidth(path.width) != tvg::Result::Success) {
                    return false;
                }
                if (shape->strokeFill(path.color.r, path.color.g, path.color.b,
                                      path.color.a) != tvg::Result::Success) {
                    return false;
                }
                if (shape->strokeCap(tvg::StrokeCap::Round) !=
                    tvg::Result::Success) {
                    return false;
                }
                if (shape->strokeJoin(tvg::StrokeJoin::Round) !=
                    tvg::Result::Success) {
                    return false;
                }
            }
            return true;
        };

    // Underlays first, so the ring diagram's loops read as the thread its
    // stickers are strung on rather than as lines ruled across them.
    if (!stroke_paths(scene.underlays)) return false;

    // The shadow lies on the ground, so it goes down before anything that
    // stands on it.
    if (scene.shadow && !draw_shadow(*scene.shadow)) return false;

    // The cubies' bodies: each slab's silhouette in the body colour, over the
    // shadow and under the stickers. That is what shows through the seams
    // between stickers -- the same plastic as a cut face -- and what keeps
    // the shadow out of the cube's own outline, at the price of a few
    // polygons rather than a mask. Drawn whether or not there is a shadow.
    for (const auto& hull : scene.bodies) {
        auto* shape = add_shape(*canvas_);
        if (!shape) return false;
        if (!append_polygon(*shape, hull)) return false;
        if (shape->fill(graphics::kBodyColor.r, graphics::kBodyColor.g,
                        graphics::kBodyColor.b, graphics::kBodyColor.a) !=
            tvg::Result::Success) {
            return false;
        }
    }

    for (const auto& face : scene.faces) {
        auto* shape = add_shape(*canvas_);
        if (!shape) return false;

        if (!append_polygon(*shape, face.points)) return false;
        if (!fill_face(*shape, face)) return false;

        // The glints straight after their own face, so that whatever is drawn
        // over this face later is drawn over them too.
        for (const auto& glint : face.highlights) {
            if (!draw_highlight(face, glint)) return false;
        }
    }

    // Strokes last, so a guide line reads on top of the stickers it crosses.
    if (!stroke_paths(scene.strokes)) return false;

    return true;
}

bool ThorVGSoftwareRenderer::draw_highlight(
    const graphics::RenderFace& face,
    const graphics::RenderHighlight& highlight) noexcept
{
    auto* shape = add_shape(*canvas_);
    if (!shape) return false;

    // The same outline as the face, so the glow is cut to the sticker.
    if (!append_polygon(*shape, face.points)) return false;

    // The ellipse through the four projected rim points: its centre is their
    // mean and its semi-axes half the distance between each opposite pair.
    // Under perspective that is where the footprint's edge actually falls,
    // which the projected centre alone would not say.
    const auto& rims = highlight.rims;
    const Vec2 centre{(rims[0].x + rims[1].x + rims[2].x + rims[3].x) * 0.25f,
                      (rims[0].y + rims[1].y + rims[2].y + rims[3].y) * 0.25f};
    const Vec2 axis_a{(rims[0].x - rims[1].x) * 0.5f, (rims[0].y - rims[1].y) * 0.5f};
    const Vec2 axis_b{(rims[2].x - rims[3].x) * 0.5f, (rims[2].y - rims[3].y) * 0.5f};
    // Too thin to hold a pixel: nothing to draw, and nothing to invert.
    if (std::abs(axis_a.x * axis_b.y - axis_a.y * axis_b.x) < 0.25f) return true;
    // White laid over at the glint's alpha. This is the same arithmetic as a
    // screen blend of white -- c + a(255 - c) either way -- so it brightens a
    // sticker towards white without hiding which colour it was, and without
    // the composition layer a blend mode would ask for on every glint.
    return fill_glow(*shape, centre, axis_a, axis_b,
                     graphics::Color{255, 255, 255, 255}, highlight.stops);
}

bool ThorVGSoftwareRenderer::draw_shadow(
    const graphics::RenderShadow& shadow) noexcept
{
    if (shadow.opacity == 0 || shadow.polygons.empty()) return true;

    // The shadow colour multiplied into the ground, worked out here once
    // rather than composed with a multiply blend: the ground is one flat
    // colour, so the two are the same picture, and the blend was measured at
    // more than a millisecond a frame for a layer it had to composite.
    const graphics::Color tint{
        static_cast<std::uint8_t>(background_.r * graphics::kShadowColor.r / 255),
        static_cast<std::uint8_t>(background_.g * graphics::kShadowColor.g / 255),
        static_cast<std::uint8_t>(background_.b * graphics::kShadowColor.b / 255),
        255};

    // Every composition layer is a pass over the shadow's whole footprint, so
    // there is exactly one: the scene the blur needs. Everything else -- the
    // flattening, the fade, the clip -- is done on the shape or on that one
    // scene, and the cube's silhouette is painted rather than masked.
    const auto clipper_for = [](tvg::Paint& paint, const graphics::Rect& clip) {
        auto* clipper = tvg::Shape::gen();
        if (!clipper) return false;
        if (clipper->appendRect(clip.x, clip.y, clip.width, clip.height) !=
                tvg::Result::Success ||
            paint.clip(clipper) != tvg::Result::Success) {
            static_cast<void>(clipper->unref());
            return false;
        }
        return true;
    };

    // The cast shadow is one shape: every caster's polygon as a subpath of
    // it, filled non-zero, so where two casters overlap is inside the outline
    // once and no darker than where one is. That is the flattening, and it
    // costs no composition layer. The fade is the fill itself -- a gradient
    // from the anchor under the cube out to the far anchor -- so it costs
    // none either.
    auto* body = add_scene(*canvas_);
    if (!body) return false;

    auto* cast = add_shape(*body);
    if (!cast) return false;
    for (const auto& polygon : shadow.polygons) {
        if (!append_polygon(*cast, polygon)) return false;
    }
    if (cast->fillRule(tvg::FillRule::NonZero) != tvg::Result::Success) {
        return false;
    }

    {
        // Strongest under the cube, most of the way gone half way to the tip,
        // gone at it: a shadow thins faster than a straight line as it leaves
        // the thing that casts it.
        const auto floor = static_cast<std::uint8_t>(std::lround(
            static_cast<float>(shadow.opacity) * graphics::kShadowFadeFloor));
        const auto midway = static_cast<std::uint8_t>(std::lround(
            static_cast<float>(shadow.opacity) * graphics::kShadowFadeMidway));
        const tvg::Fill::ColorStop stops[]{
            {0.0f, tint.r, tint.g, tint.b, shadow.opacity},
            {0.5f, tint.r, tint.g, tint.b, midway},
            {1.0f, tint.r, tint.g, tint.b, floor},
        };

        tvg::Fill* gradient = nullptr;
        const float run = std::hypot(shadow.fade_end.x - shadow.fade_start.x,
                                     shadow.fade_end.y - shadow.fade_start.y);
        if (run < graphics::kShadowFadeMinLength) {
            // Light straight overhead: no direction to fade along, so it thins
            // evenly outwards from under the cube instead.
            float reach = 1.0f;
            for (const auto& polygon : shadow.polygons) {
                for (const auto& point : polygon) {
                    reach = std::max(
                        reach, std::hypot(point.x - shadow.fade_start.x,
                                          point.y - shadow.fade_start.y));
                }
            }
            auto* radial = tvg::RadialGradient::gen();
            if (radial &&
                radial->radial(shadow.fade_start.x, shadow.fade_start.y, reach,
                               shadow.fade_start.x, shadow.fade_start.y,
                               0.0f) != tvg::Result::Success) {
                delete radial;
                radial = nullptr;
            }
            gradient = radial;
        } else {
            auto* linear = tvg::LinearGradient::gen();
            if (linear &&
                linear->linear(shadow.fade_start.x, shadow.fade_start.y,
                               shadow.fade_end.x, shadow.fade_end.y) !=
                    tvg::Result::Success) {
                delete linear;
                linear = nullptr;
            }
            gradient = linear;
        }
        if (!gradient) return false;
        if (gradient->colorStops(stops, 3) != tvg::Result::Success ||
            gradient->spread(tvg::FillSpread::Pad) != tvg::Result::Success) {
            delete gradient;
            return false;
        }
        if (cast->fill(gradient) != tvg::Result::Success) return false;
    }

    // The one layer: the blur has to see the whole outline at once.
    if (!add_gaussian_blur(*body, shadow.blur_sigma,
                           graphics::kShadowBlurQuality)) {
        return false;
    }
    // Cut to the cube's own region, so a long shadow cannot reach the drawing
    // beside it.
    if (!clipper_for(*body, shadow.clip)) return false;

    return true;
}

}  // namespace rubiks::render
