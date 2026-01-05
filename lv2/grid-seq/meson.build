project('grid-seq', 'c',
  version: '0.1.1',
  default_options: ['c_std=c99', 'warning_level=3']
)

# Dependencies
lv2_dep = dependency('lv2')
cairo_dep = dependency('cairo')
x11_dep = dependency('x11')
gtk3_dep = dependency('gtk+-3.0')
threads_dep = dependency('threads')

# Include directories
inc = include_directories('include')

# Plugin sources
plugin_sources = [
  'src/grid_seq.c',
  'src/sequencer.c',
  'src/state.c',
  'src/launchpad.c',
]

# UI sources - raw X11 + Cairo (no GTK)
ui_sources = [
  'src/gui_x11.c',
  'src/state.c',
]

# Build plugin shared library
shared_library('grid_seq',
  plugin_sources,
  include_directories: inc,
  dependencies: [lv2_dep],
  name_prefix: '',
  install: true,
  install_dir: get_option('prefix') / 'grid-seq.lv2'
)

# Build UI shared library (X11 + Cairo only)
shared_library('grid_seq_ui',
  ui_sources,
  include_directories: inc,
  dependencies: [lv2_dep, cairo_dep, x11_dep, threads_dep],
  name_prefix: '',
  install: true,
  install_dir: get_option('prefix') / 'grid-seq.lv2'
)

# Install TTL files
install_data(
  'ttl/manifest.ttl',
  'ttl/grid_seq.ttl',
  install_dir: get_option('prefix') / 'grid-seq.lv2'
)
