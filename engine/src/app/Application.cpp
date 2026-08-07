#include "app/Application.hpp"

#include <memory>

#include <thorvg.h>

#include "graphics/Camera.hpp"
#include "graphics/CubeGeometry.hpp"
#include "graphics/Pipeline.hpp"
#include "graphics/RenderScene.hpp"
#include "math/Transform.hpp"
#include "render/Renderer.hpp"
#include "render/ThorVGSoftwareRenderer.hpp"

namespace rubiks::app {
namespace {

bool initialized = false;
std::unique_ptr<render::Renderer> renderer;

// Mirrors the renderer target size; only updated after a successful
// initialize() or resize(), so it cannot drift from the actual target.
std::uint32_t surface_width = 0;
std::uint32_t surface_height = 0;

graphics::Camera camera = graphics::default_camera(1.0f);

float aspect_of(std::uint32_t width, std::uint32_t height) noexcept
{
    return static_cast<float>(width) / static_cast<float>(height);
}

void adopt_surface(std::uint32_t width, std::uint32_t height) noexcept
{
    surface_width = width;
    surface_height = height;
    camera.set_aspect(aspect_of(width, height));
}

}  // namespace

bool initialize(std::uint32_t width, std::uint32_t height) noexcept
{
    // Invalid dimensions fail even when already initialized, so a caller
    // can never mistake a rejected size for an active configuration.
    if (!render::ThorVGSoftwareRenderer::valid_dimensions(width, height)) {
        return false;
    }

    if (initialized) return true;

    if (tvg::Initializer::init(0) != tvg::Result::Success) return false;

    renderer = render::ThorVGSoftwareRenderer::create(width, height);
    if (!renderer) {
        static_cast<void>(tvg::Initializer::term());
        return false;
    }

    adopt_surface(width, height);
    initialized = true;
    return true;
}

bool resize(std::uint32_t width, std::uint32_t height) noexcept
{
    if (!initialized) return false;

    if (!renderer->resize(width, height)) return false;

    adopt_surface(width, height);
    return true;
}

bool render() noexcept
{
    if (!initialized) return false;

    // The model transform is identity for now, but it goes through the real
    // path so the quaternion-to-matrix step is exercised every frame.
    const math::Transform model;

    const auto scene = graphics::build_scene()          //
                       | graphics::transform(model)     //
                       | graphics::view(camera)         //
                       | graphics::project(camera)      //
                       | graphics::cull()               //
                       | graphics::depth_sort()         //
                       | graphics::viewport(surface_width, surface_height);

    return renderer->render(scene);
}

std::uintptr_t pixel_buffer() noexcept
{
    if (!initialized) return 0;

    return renderer->pixel_buffer();
}

std::uint32_t pixel_byte_length() noexcept
{
    if (!initialized) return 0;

    return renderer->pixel_byte_length();
}

void shutdown() noexcept
{
    if (!initialized) return;

    renderer.reset();
    static_cast<void>(tvg::Initializer::term());
    surface_width = 0;
    surface_height = 0;
    initialized = false;
}

bool is_initialized() noexcept
{
    return initialized;
}

}  // namespace rubiks::app
