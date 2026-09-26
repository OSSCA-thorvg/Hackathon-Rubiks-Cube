#include "graphics/FrameInputs.hpp"

#include "graphics/AxisGizmo.hpp"
#include "graphics/Camera.hpp"
#include "graphics/CubeGeometry.hpp"
#include "graphics/Pipeline.hpp"
#include "graphics/Rect.hpp"
#include "math/Transform.hpp"

namespace rubiks::graphics {

RenderScene compose(const FrameInputs& frame)
{
    const auto& placement = frame.placement;

    // The ground the shadow may lie on: the whole canvas. The cube's viewport
    // is a square in the middle of it and its shadow leans out into the
    // margins -- and under a flat view sharing the canvas below, which is
    // drawn over it afterwards. A shadow cut off at that view's top edge read
    // as clipped; one running on beneath the net reads as a floor the net is
    // laid on.
    const Rect stage{0.0f, 0.0f, static_cast<float>(frame.width),
                     static_cast<float>(frame.height)};

    RenderScene scene;
    if (frame.cube) {
        const CubeDrawing& drawing = *frame.cube;

        // The model transform is identity for now, but it goes through the
        // real path so the quaternion-to-matrix step is exercised every frame.
        const math::Transform model;
        const Camera camera = camera_into(drawing.orbit, placement.cube);

        // The high-contrast palette exists to keep six shades apart for eyes
        // that would otherwise merge them; shading those shades would undo
        // that, so it is drawn under a light that leaves every colour as it
        // is. The passes run either way -- it is a value, not a branch.
        const Lighting lamp = drawing.palette == Palette::HighContrast
                                  ? Lighting::unlit(drawing.lighting)
                                  : drawing.lighting;

        scene = build_cube_scene(*drawing.cube.state, drawing.rotation,
                                 drawing.palette)  //
                | transform(model)                 //
                | shadow(lamp, camera)             //
                | view(camera)                     //
                | light(lamp, camera)              //
                | project(camera)                  //
                | cull()                           //
                | depth_sort()                     //
                | viewport(placement.cube, stage);

        // Which way each axis points from here, in the colours the net's guide
        // lines use, so a cyan loop over there and a cyan arm over here are
        // plainly the same axis. In the cube's corner when the two share a
        // canvas.
        if (frame.axes) {
            append_scene(scene,
                         build_axis_gizmo(camera_into(frame.axes->orbit,
                                                      placement.cube),
                                          placement.cube));
        }
    } else if (frame.axes) {
        // On a canvas of their own the axes are the whole of it: the same
        // three arms from the same viewpoint, drawn from its middle.
        append_scene(scene, build_axis_badge(frame.axes->orbit.to_camera(1.0f),
                                             stage));
    }

    if (frame.net) {
        const NetDrawing& net = *frame.net;
        const int size = net.cube.state->size();

        if (net.painting) {
            // A draft is drawn instead of the cube, because a draft is what is
            // being edited and mostly is not a cube at all until the last
            // square is right. No guides either: nothing here turns, so there
            // is nowhere for a guide to promise.
            append_scene(scene, build_net_painting(net.painting->stickers, size,
                                                   placement.net, net.palette,
                                                   net.painting->blamed));
        } else {
            // The net is already screen-space, so it only has to be appended.
            // It gets the same rotation as the 3D scene, which is what makes
            // one gesture move every view in the same frame.
            append_scene(scene,
                         build_net_scene(*net.cube.state, placement.net,
                                         net.rotation, net.palette));

            // The rings the pressed cell could turn on, drawn over the
            // stickers so the gesture says where it is about to go before it
            // goes there.
            append_scene(scene,
                         build_net_guides(net.guides, placement.net, size));
        }
    }

    if (frame.rings) {
        const RingsDrawing& rings = *frame.rings;

        // The same rotation again. The rings a press is offering come with
        // it, since picking one out is a matter of drawing it heavier rather
        // than adding a line.
        append_scene(scene,
                     build_rings_scene(*rings.cube.state, placement.rings,
                                       rings.rotation, rings.guides,
                                       rings.palette));
    }
    return scene;
}

}  // namespace rubiks::graphics
