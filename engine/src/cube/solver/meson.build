# One step further in than cube itself: this reads the domain and the domain
# does not read it. Declared against cube_dep alone, so an include of graphics,
# interaction or the application from a solver is a build failure.
cube_solver_lib = static_library(
  'rubiks-cube-solver',
  files(
    'LayerByLayer.cpp',
    'Projection.cpp',
    'Reduction.cpp',
  ),
  dependencies: cube_dep,
)

cube_solver_dep = declare_dependency(
  dependencies: cube_dep,
  link_with: cube_solver_lib,
)
