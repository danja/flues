project('flues-synth', 'c', 'cpp',
        version: '0.1.0',
        default_options: ['c_std=c11', 'cpp_std=c++17', 'warning_level=2'])

# Build-time voice configuration
max_voices = get_option('max_voices')
add_project_arguments('-DMAX_VOICES=' + max_voices.to_string(), language: 'c')

# ARM-specific optimizations for Raspberry Pi
if host_machine.cpu_family() == 'aarch64' or host_machine.cpu_family() == 'arm'
    add_project_arguments(['-mcpu=cortex-a72', '-ftree-vectorize'],
                          language: ['c', 'cpp'])
    if get_option('enable_simd')
        add_project_arguments('-mfpu=neon', language: ['c', 'cpp'])
    endif
endif

# Dependencies
alsa_dep = dependency('alsa')
threads_dep = dependency('threads')
math_dep = meson.get_compiler('c').find_library('m', required: true)

# Include directories
inc = include_directories('include')

# Source files - Phase 1
sources = [
    # Main application
    'src/main.c',
    'src/synth_engine.c',

    # Audio & MIDI backends
    'src/audio/audio_backend_alsa.c',
    'src/audio/midi_backend_alsa.c',

    # DSP Modules
    'src/audio/modules/disyn_wrapper.cpp',  # C++ wrapper
    'src/audio/modules/sources_module.c',
    'src/audio/modules/envelope_module.c',
    'src/audio/modules/formant_module.c',
    'src/audio/modules/formant_bank_module.c',
    'src/audio/modules/interface_module.c',
    'src/audio/modules/interface_strategy.c',
    'src/audio/modules/delay_lines_module.c',
    'src/audio/modules/feedback_module.c',
    'src/audio/modules/filter_module.c',
    'src/audio/modules/modulation_module.c',

    # Interface strategies (12 total)
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
]

# Build executable
executable('flues-synth',
           sources,
           include_directories: inc,
           dependencies: [alsa_dep, threads_dep, math_dep],
           install: true)
