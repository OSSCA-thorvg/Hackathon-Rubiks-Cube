#include <cstdint>

#include "app/Application.hpp"

namespace {

constexpr std::uint32_t kSmokeWidth = 256;
constexpr std::uint32_t kSmokeHeight = 256;

}  // namespace

/**
 * Initializes the engine and renders one frame as a native smoke check.
 *
 * @return zero when initialization and rendering succeed; otherwise one.
 */
int main()
{
    if (!rubiks::app::initialize(kSmokeWidth, kSmokeHeight)) return 1;

    const bool rendered = rubiks::app::render();
    rubiks::app::shutdown();

    return rendered ? 0 : 1;
}
