#include "app/Application.hpp"

#include <emscripten/emscripten.h>

/**
 * Initializes the Rubik cube engine for browser clients.
 *
 * @return one when initialization succeeds; otherwise zero.
 */
extern "C" EMSCRIPTEN_KEEPALIVE int thorvg_rubiks_initialize() noexcept
{
    return rubiks::app::initialize() ? 1 : 0;
}

/**
 * Releases the Rubik cube engine runtime for browser clients.
 */
extern "C" EMSCRIPTEN_KEEPALIVE void thorvg_rubiks_shutdown() noexcept
{
    rubiks::app::shutdown();
}
