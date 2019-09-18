name = 'sled'

freertos = static_library(
  'freertos',
  freertos_sources,
  freertos_heap3,
  dependencies: periphdriver,
  include_directories: [
    freertos_includes,
    include_directories('.'),
    include_directories('../../epicardium'),
  ],
)

elf = executable(
  name + '.elf',
  'main.c',
  #'support.c',
  build_by_default: true,
  #dependencies: [libcard10, libff13, l0dable_startup, api_caller],
  #dependencies: [libff13, l0dable_startup, api_caller],
  dependencies: [l0dable_startup, api_caller],
  #link_with: [api_dispatcher_lib, freertos],
  link_with: [freertos],
  #link_whole: [board_card10_lib, newlib_heap_lib, l0dable_startup_lib],
  link_whole: [l0dable_startup_lib],
  include_directories: [freertos_includes],
  link_args: [
    '-Wl,-Map=' + meson.current_build_dir() + '/' + name + '.map',
    '-lgcc',
    '-lsled',
    '-L' + meson.current_source_dir() + 'lib',
  ],
  pie: true,
)
