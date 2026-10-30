cube_test = executable(
  'cube-test',
  files('CubeDraftTest.cpp', 'AssemblyTest.cpp', 'CubeTest.cpp', 'CubeStorageTest.cpp',
        'InvariantSurveyTest.cpp',
        'MoveTimelineTest.cpp', 'PackedMoveTest.cpp', 'PermutationTest.cpp',
        'SurfaceTest.cpp'),
  dependencies: [cube_dep, cube_test_support_dep, catch2_dep],
)

test('cube domain', cube_test)
