solver_test = executable(
  'solver-test',
  files('LayerByLayerTest.cpp', 'ProjectionTest.cpp', 'ReductionBench.cpp',
        'ReductionTest.cpp'),
  dependencies: [cube_solver_dep, catch2_dep],
)

# Far longer than the default thirty seconds: this executable reduces big cubes
# as well as solving small ones, and a reduction is a search over a board that
# grows with the square of the size.
#
# The number is six hundred rather than the hundred and eighty it was, because
# a hundred and eighty had no margin. Eighty seconds on a developer's machine
# looks like plenty of room until a hosted runner takes two and a half times as
# long and lands on a hundred and eighty-four -- which is what it did, and the
# build went red without anything having got slower. A timeout is meant to
# catch a hang, not to be a stopwatch the slowest machine has to beat.
test('cube solver', solver_test, timeout: 600)
