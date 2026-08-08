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

transient_test = executable(
  'transient-test',
  files('TransientTest.cpp'),
  dependencies: [graphics_dep, catch2_dep],
)

test('transient rotation', transient_test)
