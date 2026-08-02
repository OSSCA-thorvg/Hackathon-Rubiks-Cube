#include "app/Application.hpp"

/**
 * Initializes the engine runtime as a build and startup smoke check.
 *
 * @return zero when ThorVG initializes successfully; otherwise one.
 */
int main()
{
    if (!rubiks::app::initialize()) return 1;

#ifndef __EMSCRIPTEN__
    rubiks::app::shutdown();
#endif

    return 0;
}
