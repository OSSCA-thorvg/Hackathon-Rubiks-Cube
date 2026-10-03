# Depends on cube and math but not ThorVG, by construction. tests/graphics
# links this target alone, so an accidental #include <thorvg.h> in the
# pipeline fails to build rather than passing review.
#
# One library, filed in folders. The files beside this one are the types
# every view shares and the layout that places the views, and they include
# nothing from a folder. 3d/ and 2d/ include those and never each other. Only
# frame/, where a canvas's frame is composed from whichever views it shows,
# includes them all. The build cannot hold these walls, since everything here
# is one target, so grep does. Run from this directory, each of these prints
# nothing while the walls stand:
#   grep -nE '#include "graphics/(3d|2d|frame)/' *.hpp *.cpp
#   grep -rnE '#include "graphics/(2d|frame)/' 3d
#   grep -rnE '#include "graphics/(3d|frame)/' 2d
graphics_lib = static_library(
  'rubiks-graphics',
  files(
    'CanvasTheme.cpp',
    'Layout.cpp',
    'Palette.cpp',
    '3d/AxisGizmo.cpp',
    '3d/ConvexHull.cpp',
    '3d/CubeGeometry.cpp',
    '3d/Light.cpp',
    '3d/LightingPasses.cpp',
    '3d/OrbitCamera.cpp',
    '3d/Passes.cpp',
    '2d/SlotRing.cpp',
    '2d/net/NetGeometry.cpp',
    '2d/net/NetRing.cpp',
    '2d/rings/RingsGeometry.cpp',
    'frame/FrameInputs.cpp',
  ),
  dependencies: [cube_dep, math_dep],
)

graphics_dep = declare_dependency(
  dependencies: [cube_dep, math_dep],
  link_with: graphics_lib,
)
