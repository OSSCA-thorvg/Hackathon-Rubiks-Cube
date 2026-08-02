application_lib = static_library(
  'rubiks-engine',
  files('app/Application.cpp'),
  dependencies: thorvg_dep,
  include_directories: include_directories('.'),
)

application_dep = declare_dependency(
  dependencies: thorvg_dep,
  include_directories: include_directories('.'),
  link_with: application_lib,
)

application_sources = files('app/main.cpp')
application_suffix = host_machine.system() == 'emscripten' ? 'js' : []
application_link_args = []

if host_machine.system() == 'emscripten'
  application_sources += files('platform/web/bindings.cpp')
  application_link_args += [
    '-sALLOW_MEMORY_GROWTH=1',
    '-sNO_EXIT_RUNTIME=1',
  ]
endif

executable(
  'thorvg-rubiks',
  application_sources,
  dependencies: application_dep,
  link_args: application_link_args,
  name_suffix: application_suffix,
)
