# Depends on graphics for the camera and the viewport rectangle, but not on
# ThorVG: picking is a ray cast against the cube's own geometry, so every test
# here runs without a renderer. tests/interaction links this target alone.
interaction_lib = static_library(
  'rubiks-interaction',
  files(
    'DragResolver.cpp',
    'InteractionController.cpp',
    'Picking.cpp',
  ),
  dependencies: [graphics_dep],
)

interaction_dep = declare_dependency(
  dependencies: [graphics_dep],
  link_with: interaction_lib,
)
