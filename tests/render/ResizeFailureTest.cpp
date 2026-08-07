#include "render/ThorVGSoftwareRenderer.hpp"

#include <cstdint>
#include <memory>

#include <catch2/catch_test_macros.hpp>
#include <thorvg.h>

#include "graphics/RenderScene.hpp"

namespace {

// The scene is an input to every frame now, so these cases can use an empty
// one: they are about buffer and target lifetime, not about geometry.
const rubiks::graphics::RenderScene kEmptyScene;

/** Owns the ThorVG runtime for the lifetime of one test case. */
struct TvgRuntime {
    TvgRuntime()
    {
        REQUIRE(tvg::Initializer::init(0) == tvg::Result::Success);
    }

    ~TvgRuntime() { static_cast<void>(tvg::Initializer::term()); }
};

/**
 * Renderer whose allocation and target seams fail on demand, reaching the
 * resize rollback branches that real inputs cannot trigger.
 */
class FaultInjectingRenderer final
    : public rubiks::render::ThorVGSoftwareRenderer {
public:
    FaultInjectingRenderer() = default;

    using ThorVGSoftwareRenderer::init;

    /** Number of upcoming allocations that return nullptr. */
    int failing_allocations = 0;

    /** When set, set_target() fails for every other buffer. */
    const std::uint32_t* only_allowed_target = nullptr;

    /** When true, every set_target() call fails, including rollback. */
    bool fail_all_targets = false;

protected:
    std::uint32_t* allocate_pixels(std::uint32_t width,
                                   std::uint32_t height) noexcept override
    {
        if (failing_allocations > 0) {
            --failing_allocations;
            return nullptr;
        }
        return ThorVGSoftwareRenderer::allocate_pixels(width, height);
    }

    bool set_target(std::uint32_t* buffer, std::uint32_t width,
                    std::uint32_t height) noexcept override
    {
        if (fail_all_targets) return false;
        if (only_allowed_target != nullptr && buffer != only_allowed_target) {
            return false;
        }
        return ThorVGSoftwareRenderer::set_target(buffer, width, height);
    }
};

std::unique_ptr<FaultInjectingRenderer> make_renderer(std::uint32_t width,
                                                      std::uint32_t height)
{
    auto renderer = std::make_unique<FaultInjectingRenderer>();
    REQUIRE(renderer->init(width, height));
    REQUIRE(renderer->render(kEmptyScene));
    return renderer;
}

}  // namespace

TEST_CASE("allocation failure preserves the previous buffer and target")
{
    TvgRuntime runtime;
    auto renderer = make_renderer(32, 32);
    const auto pointer = renderer->pixel_buffer();
    const auto length = renderer->pixel_byte_length();

    renderer->failing_allocations = 1;
    REQUIRE(!renderer->resize(64, 64));

    REQUIRE(renderer->pixel_buffer() == pointer);
    REQUIRE(renderer->pixel_byte_length() == length);
    REQUIRE(renderer->render(kEmptyScene));

    // The next resize allocates normally again and succeeds.
    REQUIRE(renderer->resize(64, 64));
    REQUIRE(renderer->pixel_byte_length() == 64u * 64u * 4u);
    REQUIRE(renderer->render(kEmptyScene));
}

TEST_CASE("failed target swap rolls back to the previous target")
{
    TvgRuntime runtime;
    auto renderer = make_renderer(32, 32);
    const auto pointer = renderer->pixel_buffer();
    const auto length = renderer->pixel_byte_length();

    // Only the currently active buffer may become a target, so the swap to
    // the newly allocated buffer fails and the rollback succeeds.
    renderer->only_allowed_target =
        reinterpret_cast<const std::uint32_t*>(pointer);
    REQUIRE(!renderer->resize(64, 64));

    REQUIRE(renderer->pixel_buffer() == pointer);
    REQUIRE(renderer->pixel_byte_length() == length);
    REQUIRE(renderer->render(kEmptyScene));

    renderer->only_allowed_target = nullptr;
    REQUIRE(renderer->resize(64, 64));
    REQUIRE(renderer->render(kEmptyScene));
}

TEST_CASE("failed rollback leaves the renderer unusable until recreation")
{
    TvgRuntime runtime;
    auto renderer = make_renderer(32, 32);

    renderer->fail_all_targets = true;
    REQUIRE(!renderer->resize(64, 64));

    REQUIRE(renderer->pixel_buffer() == 0);
    REQUIRE(renderer->pixel_byte_length() == 0);
    REQUIRE(!renderer->render(kEmptyScene));
    renderer->fail_all_targets = false;
    REQUIRE(!renderer->resize(32, 32));

    // Recovery happens only through destroying and recreating the renderer.
    renderer = make_renderer(32, 32);
    REQUIRE(renderer->pixel_buffer() != 0);
}
