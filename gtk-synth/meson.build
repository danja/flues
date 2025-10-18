project('pm-synth-gtk', 'c',
  version: '0.1.0',
  default_options: ['warning_level=2', 'c_std=c11'])

# Dependencies
gtk_dep = dependency('gtk4', version: '>= 4.0')
pulse_dep = dependency('libpulse-simple')
math_dep = meson.get_compiler('c').find_library('m', required: true)
thread_dep = dependency('threads')

# Include directories
inc = include_directories('include')

# Source files
audio_sources = [
  'src/audio/pm_synth_engine.c',
  'src/audio/modules/sources_module.c',
  'src/audio/modules/envelope_module.c',
  'src/audio/modules/interface_module.c',
  'src/audio/modules/interface_strategy.c',
  'src/audio/modules/strategies/pluck_strategy.c',
  'src/audio/modules/strategies/hit_strategy.c',
  'src/audio/modules/strategies/reed_strategy.c',
  'src/audio/modules/strategies/flute_strategy.c',
  'src/audio/modules/strategies/brass_strategy.c',
  'src/audio/modules/strategies/bow_strategy.c',
  'src/audio/modules/strategies/bell_strategy.c',
  'src/audio/modules/strategies/drum_strategy.c',
  'src/audio/modules/strategies/crystal_strategy.c',
  'src/audio/modules/strategies/vapor_strategy.c',
  'src/audio/modules/strategies/quantum_strategy.c',
  'src/audio/modules/strategies/plasma_strategy.c',
  'src/audio/modules/delay_lines_module.c',
  'src/audio/modules/feedback_module.c',
  'src/audio/modules/filter_module.c',
  'src/audio/modules/modulation_module.c',
  'src/audio/modules/reverb_module.c',
  'src/audio/audio_backend_pulse.c',
]

ui_sources = [
  'src/ui/synth_window.c',
]

# Executable
executable('pm-synth-gtk',
  audio_sources + ui_sources,
  include_directories: inc,
  dependencies: [gtk_dep, pulse_dep, math_dep, thread_dep],
  install: true)
