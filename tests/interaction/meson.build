picking_test = executable(
  'picking-test',
  files('PickingTest.cpp'),
  dependencies: [interaction_dep, catch2_dep],
)

test('pointer picking', picking_test)

interaction_test = executable(
  'interaction-test',
  files('InteractionTest.cpp'),
  dependencies: [interaction_dep, catch2_dep],
)

test('pointer interaction', interaction_test)
