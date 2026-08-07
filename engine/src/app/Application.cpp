#include "app/Application.hpp"

#include <memory>

#include <thorvg.h>

#include "render/Renderer.hpp"
#include "render/ThorVGSoftwareRenderer.hpp"

namespace rubiks::app {
namespace {

bool initialized = false;
std::unique_ptr<render::Renderer> renderer;

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

    initialized = true;
    return true;
}

bool resize(std::uint32_t width, std::uint32_t height) noexcept
{
    if (!initialized) return false;

    return renderer->resize(width, height);
}

bool render() noexcept
{
    if (!initialized) return false;

    return renderer->render();
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
    initialized = false;
}

bool is_initialized() noexcept
{
    return initialized;
}

}  // namespace rubiks::app
