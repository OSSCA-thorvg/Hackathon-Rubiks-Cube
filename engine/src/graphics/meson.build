# Depends on cube and math but not ThorVG, by construction. tests/graphics
# links this target alone, so an accidental #include <thorvg.h> in the
# pipeline fails to build rather than passing review.
graphics_lib = static_library(
  'rubiks-graphics',
  files(
    'AxisGizmo.cpp',
    'Camera.cpp',
    'CanvasTheme.cpp',
    'ConvexHull.cpp',
    'CubeGeometry.cpp',
    'Light.cpp',
    'LightingPasses.cpp',
    'Layout.cpp',
    'NetGeometry.cpp',
    'NetRing.cpp',
    'SlotRing.cpp',
    'OrbitCamera.cpp',
    'Palette.cpp',
    'RingsGeometry.cpp',
    'Passes.cpp',
  ),
  dependencies: [cube_dep, math_dep],
)

graphics_dep = declare_dependency(
  dependencies: [cube_dep, math_dep],
  link_with: graphics_lib,
)
