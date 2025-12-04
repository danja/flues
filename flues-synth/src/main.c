// Flues-Synth - Unified Polyphonic Synthesizer
// Phase 1: Single voice, headless operation with MIDI CC control

#include "synth_engine.h"
#include "audio_backend_alsa.h"
#include "midi_backend_alsa.h"
#include "dsp_utils.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

// Global state for signal handler
static volatile bool running = true;

// Synth engine (global for audio callback)
static SynthEngine* g_synth = NULL;

// Signal handler for clean shutdown
static void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    running = false;
}

// Audio callback
static void audio_callback(float* buffer, int num_samples, void* user_data) {
    SynthEngine* synth = (SynthEngine*)user_data;
    synth_engine_process(synth, buffer, num_samples);
}

// MIDI event handler with comprehensive CC mapping
static void midi_event_handler(const MidiEvent* event, void* user_data) {
    SynthEngine* synth = (SynthEngine*)user_data;

    switch (event->type) {
        case MIDI_NOTE_ON: {
            float freq = midi_note_to_frequency(event->note);
            synth_engine_note_on(synth, event->note, freq);
            printf("Note ON: %d (%.1f Hz), velocity %d\n",
                   event->note, freq, event->velocity);
            break;
        }

        case MIDI_NOTE_OFF:
            synth_engine_note_off(synth, event->note);
            printf("Note OFF: %d\n", event->note);
            break;

        case MIDI_CONTROL_CHANGE: {
            uint8_t cc = event->cc_number;
            float value = event->cc_value / 127.0f;  // Normalize to 0-1
            printf("CC ch%d: %3u -> %3u\n", event->channel + 1, cc, event->cc_value);

            // Map CCs to parameters
            switch (cc) {
                // Standard CCs
                case 1:  // Modulation Wheel (repurposed for intensity)
                    synth_engine_set_intensity(synth, value);
                    break;

                case 7:  // Volume (Master Gain)
                    synth_engine_set_master_gain(synth, value);
                    printf("CC 7: Master Gain = %.2f\n", value);
                    break;

                case 10:  // Pan (repurposed for F2/Tongue)
                    synth_engine_set_f2(synth, exp_map(value, 500.0f, 3000.0f));
                    break;

                // Sound Controllers
                case 71:  // Resonance (F1/Jaw)
                    synth_engine_set_f1(synth, exp_map(value, 200.0f, 1000.0f));
                    break;

                case 72:  // Release Time
                    synth_engine_set_release(synth, value);
                    printf("CC 72: Release = %.2f\n", value);
                    break;

                case 73:  // Attack Time
                    synth_engine_set_attack(synth, value);
                    printf("CC 73: Attack = %.2f\n", value);
                    break;

                case 74:  // Brightness (F3/Lips)
                    synth_engine_set_f3(synth, exp_map(value, 1500.0f, 4000.0f));
                    break;

                case 75:  // Sound Control 6 (F4/Quality)
                    synth_engine_set_f4(synth, exp_map(value, 2500.0f, 4500.0f));
                    break;

                // Vocal Mode Toggles (≥64 = ON)
                case 80:  // Nasal
                    synth_engine_set_nasal(synth, event->cc_value >= 64);
                    printf("CC 80: Nasal = %s\n", event->cc_value >= 64 ? "ON" : "OFF");
                    break;

                case 81:  // Sing (Vibrato)
                    synth_engine_set_sing(synth, event->cc_value >= 64);
                    printf("CC 81: Sing = %s\n", event->cc_value >= 64 ? "ON" : "OFF");
                    break;

                case 82:  // Shout
                    synth_engine_set_shout(synth, event->cc_value >= 64);
                    printf("CC 82: Shout = %s\n", event->cc_value >= 64 ? "ON" : "OFF");
                    break;

                case 83:  // Fry
                    synth_engine_set_fry(synth, event->cc_value >= 64);
                    printf("CC 83: Fry = %s\n", event->cc_value >= 64 ? "ON" : "OFF");
                    break;

                // Disyn Source Controls
                case 16:  // GP Controller 1 (Algorithm)
                    synth_engine_set_disyn_algorithm(synth, (int)(value * 6.999f));  // 0-6
                    break;

                case 17:  // GP Controller 2 (Param1)
                    synth_engine_set_disyn_param1(synth, value);
                    break;

                case 18:  // GP Controller 3 (Param2)
                    synth_engine_set_disyn_param2(synth, value);
                    break;

                case 19:  // GP Controller 4 (Disyn Level)
                    synth_engine_set_disyn_level(synth, value);
                    break;

                case 20:  // GP Controller 5 (Noise Level)
                    synth_engine_set_noise_level(synth, value);
                    break;

                case 21:  // GP Controller 6 (DC Level)
                    synth_engine_set_dc_level(synth, value);
                    break;

                // Interface & Delay
                case 24:  // Interface Type
                    synth_engine_set_interface_type(synth, (int)(value * 11.999f));  // 0-11
                    break;

                case 26:  // Tuning
                    synth_engine_set_tuning(synth, (value - 0.5f) * 24.0f);  // -12 to +12 semitones
                    break;

                case 27:  // Ratio
                    synth_engine_set_ratio(synth, exp_map(value, 0.5f, 2.0f));  // absolute ratio
                    break;

                // Feedback
                case 28:  // Delay1 Feedback
                    synth_engine_set_delay1_feedback(synth, value);
                    break;

                case 29:  // Delay2 Feedback
                    synth_engine_set_delay2_feedback(synth, value);
                    break;

                case 30:  // Filter Feedback
                    synth_engine_set_filter_feedback(synth, value);
                    break;

                // Filter
                case 32:  // Filter Frequency
                    synth_engine_set_filter_frequency(synth, exp_map(value, 20.0f, 20000.0f));
                    break;

                case 33:  // Filter Q
                    synth_engine_set_filter_q(synth, exp_map(value, 0.1f, 10.0f));
                    break;

                case 34:  // Filter Shape
                    synth_engine_set_filter_shape(synth, value);
                    break;

                // Modulation
                case 36:  // LFO Frequency
                    synth_engine_set_lfo_frequency(synth, exp_map(value, 0.1f, 20.0f));
                    break;

                case 37:  // AM↔FM Depth (bipolar)
                    synth_engine_set_am_fm_depth(synth, (value - 0.5f) * 2.0f);  // -1 to +1
                    break;

                default:
                    // Ignore unknown CCs
                    break;
            }
            break;
        }

        case MIDI_ALL_NOTES_OFF:
            synth_engine_all_notes_off(synth);
            printf("All Notes OFF\n");
            break;
    }
}

// Try a list of candidate ALSA device names until one opens
static AudioBackendALSA* create_audio_with_fallback(const char* cli_device,
                                                    float sample_rate,
                                                    int buffer_size,
                                                    SynthEngine* synth,
                                                    const char** chosen_device_out) {
    const char* candidates[] = {
        cli_device && strlen(cli_device) > 0 ? cli_device : NULL,
        "hw:Headphones",   // Raspberry Pi headphone jack (common)
        "plughw:Headphones",
        "hw:2,0",          // bcm2835 Headphones on many Pis
        "plughw:2,0",
        "hw:1,0",
        "plughw:1,0",
        "default",
        NULL
    };

    const char* last_tried = NULL;
    for (int i = 0; candidates[i] != NULL; i++) {
        const char* name = candidates[i];
        // Skip duplicates in the list
        if (last_tried && strcmp(name, last_tried) == 0) {
            continue;
        }
        last_tried = name;

        AudioBackendALSA* backend = audio_backend_alsa_create(name,
                                                              sample_rate,
                                                              buffer_size,
                                                              audio_callback,
                                                              synth);
        if (backend) {
            if (chosen_device_out) {
                *chosen_device_out = name;
            }
            return backend;
        }
    }

    return NULL;
}

int main(int argc, char* argv[]) {
    printf("=== Flues-Synth v0.1.0 ===\n");
    printf("Unified Polyphonic Synthesizer (Phase 1: Single Voice)\n");
    printf("Target: Raspberry Pi 4 (ARM Cortex-A72)\n\n");

    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Configuration
    const char* audio_device_arg = NULL;
    const float sample_rate = DEFAULT_SAMPLE_RATE;
    const int buffer_size = DEFAULT_BUFFER_SIZE;

    // Allow override via command line
    if (argc > 1) {
        audio_device_arg = argv[1];
    }

    printf("Requested audio device: %s\n", audio_device_arg ? audio_device_arg : "(auto)");
    printf("Sample rate: %.0f Hz\n", sample_rate);
    printf("Buffer size: %d frames (%.1f ms)\n\n",
           buffer_size, (buffer_size * 1000.0f) / sample_rate);

    // Create synth engine
    g_synth = synth_engine_create(sample_rate);
    if (!g_synth) {
        fprintf(stderr, "Failed to create synth engine\n");
        return 1;
    }
    printf("Synth engine initialized\n");

    // Create MIDI backend (auto-connects to external MIDI port)
    MidiBackendALSA* midi = midi_backend_alsa_create(midi_event_handler, g_synth);
    if (!midi) {
        fprintf(stderr, "Failed to create MIDI backend\n");
        synth_engine_destroy(g_synth);
        return 1;
    }

    // Create audio backend (auto-fallback to common Pi headphone devices)
    const char* chosen_device = NULL;
    AudioBackendALSA* audio = create_audio_with_fallback(audio_device_arg,
                                                         sample_rate,
                                                         buffer_size,
                                                         g_synth,
                                                         &chosen_device);
    if (!audio) {
        fprintf(stderr, "Failed to create audio backend (tried requested device and common fallbacks)\n");
        midi_backend_alsa_destroy(midi);
        synth_engine_destroy(g_synth);
        return 1;
    }
    printf("Audio device: %s\n", chosen_device ? chosen_device : "(unknown)");

    // Start MIDI
    if (!midi_backend_alsa_start(midi)) {
        fprintf(stderr, "Failed to start MIDI backend\n");
        audio_backend_alsa_destroy(audio);
        midi_backend_alsa_destroy(midi);
        synth_engine_destroy(g_synth);
        return 1;
    }

    // Start audio
    if (!audio_backend_alsa_start(audio)) {
        fprintf(stderr, "Failed to start audio backend\n");
        midi_backend_alsa_stop(midi);
        audio_backend_alsa_destroy(audio);
        midi_backend_alsa_destroy(midi);
        synth_engine_destroy(g_synth);
        return 1;
    }

    printf("\n=== Flues-Synth Running ===\n");
    printf("Listening for MIDI input...\n");
    printf("Press Ctrl+C to quit\n\n");

    // Main loop (just sleep, threads handle everything)
    while (running) {
        sleep(1);
    }

    // Cleanup
    printf("\nShutting down...\n");
    audio_backend_alsa_stop(audio);
    midi_backend_alsa_stop(midi);
    audio_backend_alsa_destroy(audio);
    midi_backend_alsa_destroy(midi);
    synth_engine_destroy(g_synth);

    printf("Goodbye!\n");
    return 0;
}
