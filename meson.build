project(
  'thorvg-rubiks',
  'cpp',
  version: '0.1.0',
  license: 'MIT',
  default_options: [
    'buildtype=debugoptimized',
    'cpp_std=c++17',
    'warning_level=3',
  ],
)

subdir('engine')

if not meson.is_cross_build()
  subdir('tests')
endif
