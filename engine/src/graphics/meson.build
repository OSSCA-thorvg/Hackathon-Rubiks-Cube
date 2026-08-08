# Depends on cube and math but not ThorVG, by construction. tests/graphics
# links this target alone, so an accidental #include <thorvg.h> in the
# pipeline fails to build rather than passing review.
graphics_lib = static_library(
  'rubiks-graphics',
  files(
    'Camera.cpp',
    'CubeGeometry.cpp',
    'Layout.cpp',
    'NetGeometry.cpp',
    'Palette.cpp',
    'Passes.cpp',
  ),
  dependencies: [cube_dep, math_dep],
)

graphics_dep = declare_dependency(
  dependencies: [cube_dep, math_dep],
  link_with: graphics_lib,
)
