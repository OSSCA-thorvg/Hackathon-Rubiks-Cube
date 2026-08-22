engine_inc = include_directories('.')

# is_system keeps the vendored header's warnings out of our -Wall build.
linalg_inc = include_directories('../third_party/linalg', is_system: true)

subdir('cube')
subdir('math')
subdir('graphics')
subdir('interaction')

# ThorVG enters here and nowhere earlier: app/ and render/ are the only parts
# of the engine allowed to know about the rendering backend.
application_lib = static_library(
  'rubiks-engine',
  files(
    'app/Application.cpp',
    'render/ThorVGSoftwareRenderer.cpp',
  ),
  dependencies: [interaction_dep, cube_solver_dep, thorvg_dep],
)

application_dep = declare_dependency(
  dependencies: [interaction_dep, cube_solver_dep, thorvg_dep],
  link_with: application_lib,
)

if host_machine.system() == 'emscripten'
  # The C ABI exported to TypeScript; nothing else leaves the module.
  wasm_c_abi = [
    '_thorvg_rubiks_initialize',
    '_thorvg_rubiks_resize',
    '_thorvg_rubiks_render',
    '_thorvg_rubiks_pixel_buffer',
    '_thorvg_rubiks_pixel_byte_length',
    '_thorvg_rubiks_shutdown',
    '_thorvg_rubiks_pointer_down',
    '_thorvg_rubiks_pointer_move',
    '_thorvg_rubiks_pointer_up',
    '_thorvg_rubiks_pointer_cancel',
    '_thorvg_rubiks_advance',
    '_thorvg_rubiks_scramble',
    '_thorvg_rubiks_reset_cube',
    '_thorvg_rubiks_restore_buffer',
    '_thorvg_rubiks_restore_apply',
    '_thorvg_rubiks_ambient_start',
    '_thorvg_rubiks_ambient_stop',
    '_thorvg_rubiks_is_ambient',
    '_thorvg_rubiks_is_solved',
    '_thorvg_rubiks_committed_move_count',
    '_thorvg_rubiks_undo',
    '_thorvg_rubiks_redo',
    '_thorvg_rubiks_solve_rewind',
    '_thorvg_rubiks_can_solve',
    '_thorvg_rubiks_solve',
    '_thorvg_rubiks_stop_playback',
    '_thorvg_rubiks_timeline_length',
    '_thorvg_rubiks_timeline_cursor',
    '_thorvg_rubiks_timeline_scramble_end',
    '_thorvg_rubiks_timeline_move',
    '_thorvg_rubiks_turn_face',
    '_thorvg_rubiks_set_cube_size',
    '_thorvg_rubiks_cube_size',
    '_thorvg_rubiks_set_view_mode',
    '_thorvg_rubiks_view_mode',
    '_thorvg_rubiks_set_flat_style',
    '_thorvg_rubiks_flat_style',
    '_thorvg_rubiks_set_palette',
    '_thorvg_rubiks_palette',
    '_thorvg_rubiks_set_canvas_theme',
    '_thorvg_rubiks_canvas_theme',
    '_thorvg_rubiks_set_speed_scale',
    '_thorvg_rubiks_speed_scale',
    '_thorvg_rubiks_reset_view',
    '_thorvg_rubiks_is_busy',
  ]

  executable(
    'thorvg-rubiks',
    files('platform/web/bindings.cpp'),
    dependencies: application_dep,
    link_args: [
      '--no-entry',
      '-sMODULARIZE=1',
      '-sEXPORT_ES6=1',
      '-sENVIRONMENT=web',
      '-sALLOW_MEMORY_GROWTH=1',
      '-sNO_EXIT_RUNTIME=1',
      '-sEXPORTED_FUNCTIONS=' + ','.join(wasm_c_abi),
      '-sEXPORTED_RUNTIME_METHODS=HEAPU8',
    ],
    name_suffix: 'js',
  )
else
  executable(
    'thorvg-rubiks',
    files('app/main.cpp'),
    dependencies: application_dep,
  )
endif
