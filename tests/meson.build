catch2_dep = dependency(
  'catch2-with-main',
  default_options: ['tests=false'],
)

# Helpers the tests share, one directory per layer. A test executable is given
# the directories its own library may see and no others, so the walls between
# the libraries stand between their tests as well.
cube_test_support_dep = declare_dependency(
  include_directories: include_directories('support/cube'),
)

graphics_test_support_dep = declare_dependency(
  include_directories: include_directories('support/graphics'),
  dependencies: cube_test_support_dep,
)

# cube, math, graphics and interaction link without ThorVG on purpose, and
# cube links nothing else either; see engine/src/meson.build.
subdir('cube')
subdir('solver')
subdir('math')
subdir('graphics')
subdir('interaction')

subdir('app')
