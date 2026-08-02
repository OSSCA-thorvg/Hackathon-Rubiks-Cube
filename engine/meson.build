thorvg_options = [
  'default_library=static',
  'engines=cpu',
  'extra=',
  'file=false',
  'loaders=',
  'savers=',
  'static=true',
  'threads=' + (host_machine.system() == 'emscripten' ? 'false' : 'true'),
  'tools=',
]

thorvg_dep = subproject(
  'thorvg',
  default_options: thorvg_options,
).get_variable('thorvg_dep')

subdir('src')
