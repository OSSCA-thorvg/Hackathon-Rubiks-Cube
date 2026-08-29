cube_test = executable(
  'cube-test',
  files('AssemblyTest.cpp', 'CubeStateTest.cpp', 'InvariantSurveyTest.cpp',
        'MoveTimelineTest.cpp', 'PackedMoveTest.cpp', 'SurfaceTest.cpp'),
  dependencies: [cube_dep, catch2_dep],
)

test('cube domain', cube_test)
