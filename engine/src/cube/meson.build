# Declared with no dependencies at all, not even math_dep: domain coordinates
# are integers, so a float vector library would only be a way for graphics to
# leak in. tests/cube links this target alone, which turns an accidental
# include of graphics or linalg into a build failure.
cube_lib = static_library(
  'rubiks-cube',
  files(
    'CubeMove.cpp',
    'CubeState.cpp',
    'Scramble.cpp',
  ),
  include_directories: engine_inc,
)

cube_dep = declare_dependency(
  include_directories: engine_inc,
  link_with: cube_lib,
)
