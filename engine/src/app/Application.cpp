#include "app/Application.hpp"

#include <thorvg.h>

namespace rubiks::app {
namespace {

bool initialized = false;

}  // namespace

bool initialize() noexcept
{
    if (initialized) return true;

    initialized = tvg::Initializer::init(0) == tvg::Result::Success;
    return initialized;
}

void shutdown() noexcept
{
    if (!initialized) return;

    static_cast<void>(tvg::Initializer::term());
    initialized = false;
}

bool is_initialized() noexcept
{
    return initialized;
}

}  // namespace rubiks::app
