application_test = executable(
  'application-test',
  files('ApplicationTest.cpp'),
  dependencies: [application_dep, catch2_dep],
)

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

test('application lifecycle', application_test)
test('pointer interaction wiring', pointer_interaction_test)
test('rendered scene contract', render_contract_test)
test('input validation', input_validation_test)
