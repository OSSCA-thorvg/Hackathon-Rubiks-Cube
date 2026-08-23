solver_test = executable(
  'solver-test',
  files('LayerByLayerTest.cpp', 'ProjectionTest.cpp', 'ReductionBench.cpp',
        'ReductionTest.cpp'),
  dependencies: [cube_solver_dep, catch2_dep],
)

# Longer than the default thirty seconds: this executable now reduces big
# cubes as well as solving small ones, and a reduction is a search over a board
# that grows with the square of the size.
test('cube solver', solver_test, timeout: 180)
