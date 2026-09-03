#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "app/Application.hpp"
#include "cube/Cubie.hpp"

#ifdef RUBIKS_LIGHTING
#include "cube/CubeMove.hpp"
#include "cube/CubeState.hpp"
#include "graphics/ActiveRotation.hpp"
#include "graphics/Camera.hpp"
#include "graphics/CubeGeometry.hpp"
#include "graphics/Layout.hpp"
#include "graphics/Light.hpp"
#include "graphics/Passes.hpp"
#include "graphics/Pipeline.hpp"
#include "math/Transform.hpp"
#include "render/ThorVGSoftwareRenderer.hpp"
#endif

// Phase 19, step 6: frame time of the 3D view at the largest cube the app
// supports, half way through a turn, with and without the lighting passes.
// Not a test -- a measurement, printed for the phase document.

namespace {

constexpr std::uint32_t kCanvas = 1024;

double median(std::vector<double> samples)
{
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

double percentile95(std::vector<double> samples)
{
    std::sort(samples.begin(), samples.end());
    return samples[(samples.size() * 95) / 100];
}

}  // namespace

int main(int argc, char** argv)
{
    const int size = argc > 1 ? std::atoi(argv[1]) : 28;
    const int frames = argc > 2 ? std::atoi(argv[2]) : 40;

    if (!rubiks::app::initialize(kCanvas, kCanvas)) return 1;
    if (!rubiks::app::set_view_mode(rubiks::graphics::ViewMode::Cube3D)) return 1;
    if (!rubiks::app::set_cube_size(size)) return 1;

    // Half a turn's worth of animation puts the layer at 45 degrees.
    if (!rubiks::app::turn_face(rubiks::cube::Face::Right, 1, 1, 1)) return 1;
    static_cast<void>(rubiks::app::advance(100.0));

    std::vector<double> samples;
    for (int i = 0; i < frames; ++i) {
        const auto start = std::chrono::steady_clock::now();
        if (!rubiks::app::render()) return 1;
        const auto end = std::chrono::steady_clock::now();
        samples.push_back(
            std::chrono::duration<double, std::milli>(end - start).count());
    }

    std::printf("size %d, %d frames at 45 degrees: median %.2f ms, p95 %.2f ms\n",
                size, frames, median(samples), percentile95(samples));

    // Optionally write the resting frame out as a binary PPM, for looking at
    // the pixels the numbers above were made of.
    if (argc > 3) {
        rubiks::app::reset_cube();
        while (rubiks::app::advance(16.0)) {
        }
        if (!rubiks::app::render()) return 1;
        const auto* pixels =
            reinterpret_cast<const std::uint8_t*>(rubiks::app::pixel_buffer());
        if (FILE* out = std::fopen(argv[3], "wb")) {
            std::fprintf(out, "P6\n%u %u\n255\n", kCanvas, kCanvas);
            for (std::uint32_t i = 0; i < kCanvas * kCanvas; ++i) {
                std::fwrite(pixels + i * 4, 1, 3, out);
            }
            std::fclose(out);
        }
    }

#ifdef RUBIKS_LIGHTING
    using namespace rubiks::graphics;
    const auto placement = layout(kCanvas, kCanvas, ViewMode::Cube3D);
    const Camera camera = default_camera(1.0f);
    const ActiveRotation turn{rubiks::cube::Axis::X,
                              rubiks::cube::layer(size - 1), 45.0f};
    const RenderScene scene = build_cube_scene(rubiks::cube::CubeState(size), turn)  //
                              | transform({})                                        //
                              | shadow(default_lighting(), camera)                        //
                              | view(camera)                                         //
                              | light(default_lighting(), camera)                         //
                              | project(camera)                                      //
                              | cull()                                               //
                              | depth_sort()                                         //
                              | viewport(placement.cube);
    std::size_t glints = 0;
    for (const auto& face : scene.faces) {
        glints += face.highlights.size();
    }
    std::printf("faces drawn %zu, with a highlight %zu (%.1f%%), shadow casters %zu\n",
                scene.faces.size(), glints,
                scene.faces.empty() ? 0.0 : 100.0 * glints / scene.faces.size(),
                scene.shadow ? scene.shadow->polygons.size() : 0);

    // Where the time goes: the same scene with parts of the shadow chain and
    // the highlights taken away, drawn straight through the renderer.
    const auto time_variant = [&](const char* name, RenderScene variant) {
        auto renderer = rubiks::render::ThorVGSoftwareRenderer::create(kCanvas, kCanvas);
        if (!renderer) return;
        std::vector<double> times;
        for (int i = 0; i < frames; ++i) {
            const auto start = std::chrono::steady_clock::now();
            if (!renderer->render(variant)) return;
            const auto end = std::chrono::steady_clock::now();
            times.push_back(std::chrono::duration<double, std::milli>(end - start).count());
        }
        std::printf("  %-28s median %6.2f ms, p95 %6.2f ms\n", name, median(times),
                    percentile95(times));
    };
    time_variant("full", scene);
    {
        RenderScene v = scene;
        v.shadow.reset();
        time_variant("no shadow", v);
    }
    {
        RenderScene v = scene;
        for (auto& face : v.faces) face.highlights.clear();
        time_variant("no highlights", v);
    }
    if (scene.shadow) {
        RenderScene v = scene;
        v.shadow->blur_sigma = 0.01f;
        time_variant("shadow, blur ~0 (fade+clip)", v);
    }
#endif

    rubiks::app::shutdown();
    return 0;
}
