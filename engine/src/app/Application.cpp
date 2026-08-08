#include "app/Application.hpp"

#include <memory>

#include <thorvg.h>

#include "cube/CubeState.hpp"
#include "graphics/Camera.hpp"
#include "graphics/CubeGeometry.hpp"
#include "graphics/Layout.hpp"
#include "graphics/NetGeometry.hpp"
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
graphics::CanvasLayout placement;

graphics::Camera camera = graphics::default_camera(1.0f);

// The logical cube. Interaction in Phase 5 mutates this and nothing else.
cube::CubeState cube_state(3);

void adopt_surface(std::uint32_t width, std::uint32_t height) noexcept
{
    placement = graphics::layout(width, height);
    // Follows the cube region rather than the canvas. That region is square,
    // so this is always 1, but deriving it keeps the two in step if the
    // layout ever changes.
    camera.set_aspect(placement.cube.width / placement.cube.height);
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

    auto scene = graphics::build_cube_scene(cube_state)  //
                 | graphics::transform(model)            //
                 | graphics::view(camera)                //
                 | graphics::project(camera)             //
                 | graphics::cull()                      //
                 | graphics::depth_sort()                //
                 | graphics::viewport(placement.cube);

    // The net is already screen-space, so it only has to be appended. The two
    // regions do not overlap, so the order between them does not matter.
    const auto net = graphics::build_net_scene(cube_state, placement.net);
    scene.faces.insert(scene.faces.end(), net.faces.begin(), net.faces.end());

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
    placement = graphics::CanvasLayout{};
    cube_state = cube::CubeState(3);
    initialized = false;
}

bool is_initialized() noexcept
{
    return initialized;
}

}  // namespace rubiks::app
