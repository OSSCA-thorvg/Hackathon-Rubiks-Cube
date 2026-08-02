#include "app/Application.hpp"

#include <cassert>

/**
 * Verifies that application initialization and shutdown are idempotent.
 *
 * @return zero when all lifecycle assertions pass.
 */
int main()
{
    assert(!rubiks::app::is_initialized());
    assert(rubiks::app::initialize());
    assert(rubiks::app::initialize());
    assert(rubiks::app::is_initialized());

    rubiks::app::shutdown();
    rubiks::app::shutdown();
    assert(!rubiks::app::is_initialized());
    return 0;
}
