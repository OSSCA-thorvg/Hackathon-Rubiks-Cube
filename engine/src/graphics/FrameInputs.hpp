#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "cube/CubeState.hpp"
#include "cube/Cubie.hpp"
#include "graphics/ActiveRotation.hpp"
#include "graphics/Layout.hpp"
#include "graphics/Light.hpp"
#include "graphics/NetGeometry.hpp"
#include "graphics/OrbitCamera.hpp"
#include "graphics/Palette.hpp"
#include "graphics/RenderScene.hpp"
#include "graphics/RingsGeometry.hpp"

/**
 * What one canvas's frame is drawn from, and the drawing made of it.
 *
 * A canvas skips a frame when what it would be drawn from is what its last
 * one was drawn from, so the drawing and the comparison have to agree on what
 * that is: a drawing that read something the comparison did not look at would
 * leave a picture standing after it had changed. They agree here by
 * construction. Each drawing is handed one value holding everything it reads,
 * compose() is given nothing else, and two frames are the same when those
 * values are.
 *
 * So a drawing that starts reading something new gets it by adding a field to
 * its value -- there is nowhere else for it to come from -- and the one place
 * to remember is that value's comparison, written directly beneath it.
 */
namespace rubiks::graphics {

/**
 * The cube a drawing is made of, named by how many times it has changed.
 *
 * Held by address rather than copied: at the largest size it is twenty-two
 * thousand cubies, and three of the four drawings read it on every frame. Two
 * drawings of one revision of one cube are drawings of the same cube, so that
 * is what is compared -- which leaves whoever hands one over to move the
 * number whenever the cube moves.
 */
struct CubeRevision {
    const cube::CubeState* state = nullptr;
    std::uint64_t revision = 0;
};

[[nodiscard]] constexpr bool operator==(const CubeRevision& a,
                                        const CubeRevision& b) noexcept
{
    return a.state == b.state && a.revision == b.revision;
}

/** The cube in three dimensions: everything its drawing reads. */
struct CubeDrawing {
    CubeRevision cube;
    std::optional<ActiveRotation> rotation;
    Palette palette = Palette::Classic;
    Lighting lighting;
    OrbitCamera orbit;
};

[[nodiscard]] inline bool operator==(const CubeDrawing& a,
                                     const CubeDrawing& b) noexcept
{
    return a.cube == b.cube && a.rotation == b.rotation &&
           a.palette == b.palette && a.lighting == b.lighting &&
           a.orbit == b.orbit;
}

/**
 * A colouring being made, which the net draws in place of the cube.
 *
 * With the squares the last refusal blamed, kept on it while they are being
 * mended, and the keyboard's place on it while the net has the keyboard.
 */
struct NetPainting {
    std::vector<cube::FaceColor> stickers;
    std::vector<int> blamed;
    std::optional<NetPosition> cursor;
};

[[nodiscard]] inline bool operator==(const NetPainting& a,
                                     const NetPainting& b) noexcept
{
    return a.stickers == b.stickers && a.blamed == b.blamed &&
           a.cursor == b.cursor;
}

/**
 * The net: everything its drawing reads.
 *
 * The cube even while a colouring is drawn instead of it, for its size.
 */
struct NetDrawing {
    CubeRevision cube;
    std::optional<ActiveRotation> rotation;
    Palette palette = Palette::Classic;
    std::vector<NetGuide> guides;
    std::optional<NetPainting> painting;
};

[[nodiscard]] inline bool operator==(const NetDrawing& a,
                                     const NetDrawing& b) noexcept
{
    return a.cube == b.cube && a.rotation == b.rotation &&
           a.palette == b.palette && a.guides == b.guides &&
           a.painting == b.painting;
}

/** The ring diagram: everything its drawing reads. */
struct RingsDrawing {
    CubeRevision cube;
    std::optional<ActiveRotation> rotation;
    Palette palette = Palette::Classic;
    std::vector<RingsGuide> guides;
};

[[nodiscard]] inline bool operator==(const RingsDrawing& a,
                                     const RingsDrawing& b) noexcept
{
    return a.cube == b.cube && a.rotation == b.rotation &&
           a.palette == b.palette && a.guides == b.guides;
}

/** The axes: which way the cube's axes point from the viewpoint. */
struct AxesDrawing {
    OrbitCamera orbit;
};

[[nodiscard]] constexpr bool operator==(const AxesDrawing& a,
                                        const AxesDrawing& b) noexcept
{
    return a.orbit == b.orbit;
}

/**
 * Everything one canvas's frame is drawn from.
 *
 * A drawing is present when the canvas shows it. Where each sits on the
 * canvas is the layout's, worked out from the size and which of them are
 * present; it is carried here rather than worked out again so that a press
 * is read against the very regions that were drawn.
 */
struct FrameInputs {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    CanvasLayout placement;

    std::optional<CubeDrawing> cube;
    std::optional<NetDrawing> net;
    std::optional<RingsDrawing> rings;
    std::optional<AxesDrawing> axes;
};

[[nodiscard]] inline bool operator==(const FrameInputs& a,
                                     const FrameInputs& b) noexcept
{
    return a.width == b.width && a.height == b.height &&
           a.placement == b.placement && a.cube == b.cube && a.net == b.net &&
           a.rings == b.rings && a.axes == b.axes;
}

/**
 * The one scene a canvas draws for a frame, made of nothing but the frame.
 *
 * The drawings in a fixed order, each over the last: the cube, the axes, the
 * net, the rings. The cube's shadow runs out over the whole canvas, under
 * anything laid on it afterwards.
 */
[[nodiscard]] RenderScene compose(const FrameInputs& frame);

}  // namespace rubiks::graphics
