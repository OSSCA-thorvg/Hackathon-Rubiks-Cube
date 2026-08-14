picking_test = executable(
  'picking-test',
  files('PickingTest.cpp'),
  dependencies: [interaction_dep, catch2_dep],
)

test('pointer picking', picking_test)

net_picking_test = executable(
  'net-picking-test',
  files('NetPickingTest.cpp'),
  dependencies: [interaction_dep, catch2_dep],
)

test('net picking', net_picking_test)

interaction_test = executable(
  'interaction-test',
  files('InteractionTest.cpp'),
  dependencies: [interaction_dep, catch2_dep],
)

test('pointer interaction', interaction_test)

orbit_test = executable(
  'orbit-test',
  files('OrbitTest.cpp'),
  dependencies: [interaction_dep, catch2_dep],
)

test('viewpoint orbit', orbit_test)
