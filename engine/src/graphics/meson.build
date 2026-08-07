# No ThorVG dependency by construction. tests/graphics links this target
# alone, so an accidental #include <thorvg.h> in the pipeline fails to build
# rather than passing review.
graphics_lib = static_library(
  'rubiks-graphics',
  files(
    'Camera.cpp',
    'CubeGeometry.cpp',
    'Passes.cpp',
  ),
  dependencies: math_dep,
)

graphics_dep = declare_dependency(
  dependencies: math_dep,
  link_with: graphics_lib,
)
