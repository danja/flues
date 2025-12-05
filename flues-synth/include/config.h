#ifndef FLUES_SYNTH_CONFIG_H
#define FLUES_SYNTH_CONFIG_H

// Voice count (from meson option, default 4)
#ifndef MAX_VOICES
#define MAX_VOICES 4
#endif

// Audio configuration
#define DEFAULT_SAMPLE_RATE 32000.0f
#define DEFAULT_BUFFER_SIZE 512

// MIDI configuration
#define MIDI_QUEUE_SIZE 256

// Delay line size
#define MAX_DELAY_LENGTH 8192

// DC blocker coefficient (increased for tighter DC rejection to prevent feedback latching)
#define DC_BLOCKER_R 0.999f

// Post-release damping factor
#define POST_RELEASE_DAMP_FACTOR 0.995f

// Auto-stop thresholds
#define AUTO_STOP_DAMP_THRESHOLD 1e-4f
#define AUTO_STOP_OUTPUT_THRESHOLD 1e-5f

// Enable optimizations
#ifdef ENABLE_SIMD
#include <arm_neon.h>
#endif

#endif // FLUES_SYNTH_CONFIG_H
