application_test = executable(
  'application-test',
  files('ApplicationTest.cpp'),
  dependencies: [application_dep, catch2_dep],
)

speed_test = executable(
  'speed-test',
  files('SpeedTest.cpp'),
  dependencies: [application_dep, catch2_dep],
)

test('animation speed', speed_test)

render_contract_test = executable(
  'render-contract-test',
  files('RenderContractTest.cpp'),
  dependencies: [application_dep, catch2_dep],
)

input_validation_test = executable(
  'input-validation-test',
  files('InputValidationTest.cpp'),
  dependencies: [application_dep, catch2_dep],
)

pointer_interaction_test = executable(
  'pointer-interaction-test',
  files('PointerInteractionTest.cpp'),
  dependencies: [application_dep, catch2_dep],
)

ambient_test = executable(
  'ambient-test',
  files('AmbientTest.cpp'),
  dependencies: [application_dep, catch2_dep],
)

history_test = executable(
  'history-test',
  files('HistoryTest.cpp'),
  dependencies: [application_dep, catch2_dep],
)

solve_test = executable(
  'solve-test',
  files('SolveTest.cpp'),
  dependencies: [application_dep, catch2_dep],
)

canvas_theme_test = executable(
  'canvas-theme-test',
  files('CanvasThemeTest.cpp'),
  dependencies: [application_dep, catch2_dep],
)

share_restore_test = executable(
  'share-restore-test',
  files('ShareRestoreTest.cpp'),
  dependencies: [application_dep, catch2_dep],
)

test('application lifecycle', application_test)
test('ambient watching', ambient_test)
test('move history', history_test)
test('solve command', solve_test)
test('shared state restore', share_restore_test)
test('pointer interaction wiring', pointer_interaction_test)
test('rendered scene contract', render_contract_test)
test('canvas theme ground', canvas_theme_test)
test('input validation', input_validation_test)
