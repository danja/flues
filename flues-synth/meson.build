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

# Core DSP sources (shared by app and tests)
core_sources = [
    'src/synth_engine.c',

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

core_lib = static_library('flues_core',
                          core_sources,
                          include_directories: inc,
                          dependencies: [math_dep])

# Main application sources (audio + MIDI + main)
app_sources = [
    'src/main.c',
    'src/midi_mapping.c',
    'src/audio/audio_backend_alsa.c',
    'src/audio/midi_backend_alsa.c',
]

executable('flues-synth',
           app_sources,
           include_directories: inc,
           dependencies: [alsa_dep, threads_dep, math_dep],
           link_with: core_lib,
           install: true)

# Smoke test to ensure default signal path produces audio energy
smoke = executable('engine-smoke',
                   ['tests/engine_smoke.c'],
                   include_directories: inc,
                   dependencies: [math_dep],
                   link_with: core_lib)
test('engine-smoke', smoke)

# Disyn level analysis test to measure waveform amplitude
disyn_levels = executable('disyn-levels',
                          ['tests/disyn_levels.c'],
                          include_directories: inc,
                          dependencies: [math_dep],
                          link_with: core_lib)
test('disyn-levels', disyn_levels)

# Envelope and note timing test
envelope_test = executable('envelope-test',
                           ['tests/envelope_test.c'],
                           include_directories: inc,
                           dependencies: [math_dep],
                           link_with: core_lib)
test('envelope-test', envelope_test)

# Noise isolation test - systematic parameter sweep to identify noise sources
noise_isolation = executable('noise-isolation',
                             ['tests/noise_isolation.c'],
                             include_directories: inc,
                             dependencies: [math_dep],
                             link_with: core_lib)
test('noise-isolation', noise_isolation)

# Envelope debug test
envelope_debug = executable('envelope-debug',
                            ['tests/envelope_debug.c'],
                            include_directories: inc,
                            dependencies: [math_dep],
                            link_with: core_lib)

# Polyphony smoke test (voice allocation/steal + velocity scaling)
polyphony_smoke = executable('polyphony-smoke',
                             ['tests/polyphony_smoke.c'],
                             include_directories: inc,
                             dependencies: [math_dep],
                             link_with: core_lib)
test('polyphony-smoke', polyphony_smoke)
