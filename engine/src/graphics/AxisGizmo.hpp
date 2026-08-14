#pragma once

#include "graphics/Camera.hpp"
#include "graphics/Rect.hpp"
#include "graphics/RenderScene.hpp"

namespace rubiks::graphics {

/**
 * The three axes as they point from the current viewpoint, drawn in a corner.
 *
 * Three segments out of one shared point, in the colors the net's guide lines
 * use. That shared palette is the whole purpose: the net says a press will turn
 * about X by drawing a cyan loop, and the cube says which way X actually points
 * by drawing a cyan arm. Neither view can say that alone -- the net has no
 * viewpoint and the cube has no loops -- so the color is what carries the
 * meaning between them.
 *
 * Only the viewpoint's rotation is used, so the gizmo is the same size wherever
 * the cube is and however far away: it reports a direction, not a position.
 */

/** Arm length, as a share of the cube region's shorter side. */
inline constexpr float kAxisGizmoArmShare = 0.085f;

/** How far the shared point sits in from the region's top-right corner. */
inline constexpr float kAxisGizmoInsetShare = 0.115f;

/** Line thickness, on the same share. */
inline constexpr float kAxisGizmoWidthShare = 0.013f;

/** Empty for a region with no extent, which is what a view mode not shown is. */
[[nodiscard]] RenderScene build_axis_gizmo(const Camera& camera,
                                           const Rect& region);

}  // namespace rubiks::graphics
