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
#include "interaction/InteractionController.hpp"
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

// The logical cube, only ever holding quarter turns. A drag in progress lives
// in the controller instead, and reaches this through a committed move.
constexpr int kCubeSize = 3;
cube::CubeState cube_state(kCubeSize);
interaction::InteractionController interaction(kCubeSize);

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

    // A drag holds screen directions derived from the old viewport, so it
    // cannot survive the change; dropping it leaves the cube untouched. A snap
    // only animates an angle, so it is unaffected and keeps running.
    interaction.cancel();

    adopt_surface(width, height);
    return true;
}

bool pointer_down(float x, float y) noexcept
{
    if (!initialized) return false;

    return interaction.pointer_down(x, y, camera, placement.cube);
}

void pointer_move(float x, float y) noexcept
{
    if (!initialized) return;

    interaction.pointer_move(x, y);
}

void pointer_up() noexcept
{
    if (!initialized) return;

    interaction.pointer_up();
}

void pointer_cancel() noexcept
{
    if (!initialized) return;

    interaction.cancel();
}

bool advance(double elapsed_ms) noexcept
{
    if (!initialized) return false;

    const bool more_frames = interaction.advance(elapsed_ms);

    // The one place a gesture becomes a change to the logical cube.
    if (const auto move = interaction.take_committed_move()) {
        cube_state.apply(*move);
    }

    return more_frames;
}

bool render() noexcept
{
    if (!initialized) return false;

    // The model transform is identity for now, but it goes through the real
    // path so the quaternion-to-matrix step is exercised every frame.
    const math::Transform model;

    auto scene = graphics::build_cube_scene(cube_state,
                                            interaction.active_rotation())  //
                 | graphics::transform(model)                               //
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
    cube_state = cube::CubeState(kCubeSize);
    interaction.reset();
    initialized = false;
}

bool is_initialized() noexcept
{
    return initialized;
}

}  // namespace rubiks::app
