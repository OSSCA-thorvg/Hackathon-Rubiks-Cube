pipeline_test = executable(
  'pipeline-test',
  files('PipelineTest.cpp'),
  dependencies: [graphics_dep, catch2_dep],
)

test('graphics pipeline', pipeline_test)

net_test = executable(
  'net-test',
  files('NetTest.cpp'),
  dependencies: [graphics_dep, catch2_dep],
)

test('net geometry', net_test)

orbit_camera_test = executable(
  'orbit-camera-test',
  files('OrbitCameraTest.cpp'),
  dependencies: [graphics_dep, catch2_dep],
)

test('orbit camera', orbit_camera_test)

transient_test = executable(
  'transient-test',
  files('TransientTest.cpp'),
  dependencies: [graphics_dep, catch2_dep],
)

test('transient rotation', transient_test)

palette_test = executable(
  'palette-test',
  files('PaletteTest.cpp'),
  dependencies: [graphics_dep, catch2_dep],
)

test('palette', palette_test)

rings_test = executable(
  'rings-test',
  files('RingsTest.cpp'),
  dependencies: [graphics_dep, catch2_dep],
)

test('ring diagram', rings_test)

canvas_theme_test = executable(
  'canvas-theme-test',
  files('CanvasThemeTest.cpp'),
  dependencies: [graphics_dep, catch2_dep],
)

test('canvas theme', canvas_theme_test)

lighting_test = executable(
  'lighting-test',
  files('LightingTest.cpp'),
  dependencies: [graphics_dep, catch2_dep],
)

test('lighting and shadow', lighting_test)
