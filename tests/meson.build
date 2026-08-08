catch2_dep = dependency(
  'catch2-with-main',
  default_options: ['tests=false'],
)

# cube, math, graphics and interaction link without ThorVG on purpose, and
# cube links nothing else either; see engine/src/meson.build.
subdir('cube')
subdir('math')
subdir('graphics')
subdir('interaction')

subdir('app')
subdir('render')
