// Flues-Synth - Unified Polyphonic Synthesizer
// 4-voice polyphonic, headless operation with MIDI CC control

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
static bool g_use_mk449_remap = true;  // remap CCs for the Evolution MK-449 defaults
static bool g_control_notes_enabled = false;  // off by default to avoid consuming low notes
static int g_current_program = 0;  // Current MIDI program (0-7)

// Translate MK-449 default CCs to the synth’s parameter map
static uint8_t mk449_remap_cc(uint8_t cc_in) {
    switch (cc_in) {
        case 91: return 28;  // Effect 1 depth → Delay1 feedback
        case 92: return 29;  // Effect 2 depth → Delay2 feedback
        case 93: return 30;  // Effect 3 depth → Filter feedback
        case 84: return 27;  // Portamento control → Delay ratio
        case 12: return 20;  // Data entry → Noise level
        case 13: return 19;  // Data entry fine → Disyn level
        case 5:  return 1;   // Portamento time → Intensity (mod wheel stand-in)
        default: return cc_in;
    }
}

// Note numbers 36-42 toggle sections of the DSP chain for debugging hiss
// 36: Noise, 37: Disyn, 38: Feedback, 39: Formants, 40: Filter, 41: Hard mute, 42: Reset
static bool handle_control_note(const MidiEvent* event, SynthEngine* synth) {
    if (!g_control_notes_enabled) {
        return false;
    }

    const uint8_t note = event->note;
    if (note < 36 || note > 42) {
        return false;
    }

    // Only act on Note On with velocity > 0; each press toggles the state
    if (event->type == MIDI_NOTE_ON && event->velocity > 0) {
        switch (note) {
            case 36: {
                bool new_state = !synth_engine_is_noise_enabled(synth);
                synth_engine_enable_noise(synth, new_state);
                printf("Ctl Note 36: Noise %s\n", new_state ? "ENABLED" : "DISABLED");
                break;
            }
            case 37: {
                bool new_state = !synth_engine_is_disyn_enabled(synth);
                synth_engine_enable_disyn(synth, new_state);
                printf("Ctl Note 37: Disyn %s\n", new_state ? "ENABLED" : "DISABLED");
                break;
            }
            case 38: {
                bool new_state = !synth_engine_is_feedback_enabled(synth);
                synth_engine_enable_feedback(synth, new_state);
                printf("Ctl Note 38: Feedback %s\n", new_state ? "ENABLED" : "DISABLED");
                break;
            }
            case 39: {
                bool new_state = !synth_engine_is_formants_enabled(synth);
                synth_engine_enable_formants(synth, new_state);
                printf("Ctl Note 39: Formants %s\n", new_state ? "ENABLED" : "DISABLED");
                break;
            }
            case 40: {
                bool new_state = !synth_engine_is_filter_enabled(synth);
                synth_engine_enable_filter(synth, new_state);
                printf("Ctl Note 40: Filter %s\n", new_state ? "ENABLED" : "DISABLED");
                break;
            }
            case 41: {
                bool new_state = !synth_engine_is_hard_muted(synth);
                synth_engine_hard_mute(synth, new_state);
                printf("Ctl Note 41: HARD MUTE %s\n", new_state ? "ON" : "OFF");
                break;
            }
            case 42:
                synth_engine_reset(synth);
                // Note: reset function prints its own message
                break;
        }
        return true;  // consume control note-on (don't trigger voice)
    }

    // For note-off or velocity-0 note-on, return false to allow normal processing
    // This prevents control notes from sticking if they were accidentally triggered
    return false;
}

// Handle MIDI program change
// Programs configure signal chain and remap the 9 keyboard sliders
// Slider CCs: 73, 72, 28, 30, 74, 71, 1, 27, 7
// New order: Sliders 1-7 are context-dependent, Sliders 8-9 are Attack/Release
static void handle_program_change(uint8_t program, SynthEngine* synth) {
    g_current_program = program;

    printf("\n═══════════════════════════════════════════════════════════\n");
    printf("PROGRAM CHANGE: %d\n", program);

    switch (program) {
        case 0:  // Disyn Direct - Raw distortion synth straight to output
            printf("Program 0: Disyn Direct\n");
            printf("  - Disyn enabled, all other processing bypassed\n");
            printf("  - Sliders: Alg(73→16), P1(72→17), P2(28→18), Level(30→19), Intensity(74→1), Tuning(71→26), Ratio(1→27), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, false);
            synth_engine_enable_feedback(synth, false);
            synth_engine_enable_filter(synth, false);
            synth_engine_set_disyn_level(synth, 0.8f);
            synth_engine_set_master_gain(synth, 0.7f);
            break;

        case 1:  // Disyn + Delays - Add delay lines to Disyn
            printf("Program 1: Disyn + Delays\n");
            printf("  - Disyn + delay lines, feedback enabled\n");
            printf("  - Sliders: Dly1(73→28), Dly2(72→29), Filt(28→30), Level(30→19), Intensity(74→1), Tuning(71→26), Ratio(1→27), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, false);
            synth_engine_enable_feedback(synth, true);
            synth_engine_enable_filter(synth, false);
            synth_engine_set_disyn_level(synth, 0.6f);
            synth_engine_set_delay1_feedback(synth, 0.3f);
            synth_engine_set_delay2_feedback(synth, 0.3f);
            synth_engine_set_filter_feedback(synth, 0.0f);
            break;

        case 2:  // Disyn + Filter - Add state-variable filter
            printf("Program 2: Disyn + Filter\n");
            printf("  - Disyn + delays + filter\n");
            printf("  - Sliders: Freq(73→32), Q(72→33), Shape(28→34), Level(30→19), Intensity(74→1), Tuning(71→26), Ratio(1→27), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, false);
            synth_engine_enable_feedback(synth, true);
            synth_engine_enable_filter(synth, true);
            synth_engine_set_disyn_level(synth, 0.5f);
            synth_engine_set_delay1_feedback(synth, 0.25f);
            synth_engine_set_delay2_feedback(synth, 0.25f);
            synth_engine_set_filter_feedback(synth, 0.2f);
            synth_engine_set_filter_frequency(synth, 1000.0f);
            synth_engine_set_filter_q(synth, 2.0f);
            break;

        case 3:  // Formant Voice - Vocal formants only
            printf("Program 3: Formant Voice\n");
            printf("  - Noise source through formant cascade\n");
            printf("  - Sliders: F1(73→71), F2(72→10), F3(28→74), F4(30→75), Noise(74→20), Nasal(71→80), Intensity(1), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, false);
            synth_engine_enable_noise(synth, true);
            synth_engine_enable_formants(synth, true);
            synth_engine_enable_feedback(synth, false);
            synth_engine_enable_filter(synth, false);
            synth_engine_set_noise_level(synth, 0.5f);
            synth_engine_set_f1(synth, 500.0f);
            synth_engine_set_f2(synth, 1500.0f);
            synth_engine_set_f3(synth, 2500.0f);
            synth_engine_set_f4(synth, 3500.0f);
            break;

        case 4:  // Hybrid Speech - Disyn + Formants
            printf("Program 4: Hybrid Speech\n");
            printf("  - Disyn through formants (speech synthesis)\n");
            printf("  - Sliders: F1(73→71), F2(72→10), F3(28→74), F4(30→75), DisLvl(74→19), Noise(71→20), Intensity(1), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, true);
            synth_engine_enable_formants(synth, true);
            synth_engine_enable_feedback(synth, false);
            synth_engine_enable_filter(synth, false);
            synth_engine_set_disyn_level(synth, 0.4f);
            synth_engine_set_noise_level(synth, 0.15f);
            synth_engine_set_f1(synth, 500.0f);
            synth_engine_set_f2(synth, 1500.0f);
            synth_engine_set_f3(synth, 2500.0f);
            synth_engine_set_f4(synth, 3500.0f);
            break;

        case 5:  // Physical Model - Full PM chain (no formants)
            printf("Program 5: Physical Model\n");
            printf("  - Noise through interface/delays/filter (pure PM)\n");
            printf("  - Sliders: Dly1(73→28), Dly2(72→29), FiltFB(28→30), Interface(30→24), Intensity(74→1), Tuning(71→26), Ratio(1→27), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, false);
            synth_engine_enable_noise(synth, true);
            synth_engine_enable_formants(synth, false);
            synth_engine_enable_feedback(synth, true);
            synth_engine_enable_filter(synth, true);
            synth_engine_set_noise_level(synth, 0.3f);
            synth_engine_set_delay1_feedback(synth, 0.4f);
            synth_engine_set_delay2_feedback(synth, 0.4f);
            synth_engine_set_filter_feedback(synth, 0.3f);
            synth_engine_set_interface_type(synth, 2);  // Reed
            break;

        case 6:  // Full Hybrid - All modules enabled (DISABLED - causes segfault)
            printf("Program 6: Full Hybrid (DISABLED)\n");
            printf("  - This program is temporarily disabled due to stability issues\n");
            printf("  - Using Program 5 instead\n");
            handle_program_change(5, synth);
            return;

        case 7:  // Experimental - Formants in feedback loop (DISABLED - causes segfault)
            printf("Program 7: Experimental (DISABLED)\n");
            printf("  - This program is temporarily disabled due to stability issues\n");
            printf("  - Using Program 4 instead\n");
            handle_program_change(4, synth);
            return;

        default:
            printf("Program %d: Unknown (using program 0)\n", program);
            handle_program_change(0, synth);
            return;
    }

    printf("═══════════════════════════════════════════════════════════\n\n");
}

// Remap slider CCs based on current program
// The 9 hardware sliders send: 73, 72, 28, 30, 74, 71, 1, 27, 7
// New layout: Sliders 1-7 = context, Slider 8 = Attack(27→73), Slider 9 = Release(7→72)
static uint8_t remap_slider_cc(uint8_t cc) {
    // Attack and Release are always on sliders 8 and 9
    if (cc == 27) return 73;  // Slider 8 → Attack
    if (cc == 7) return 72;   // Slider 9 → Release

    switch (g_current_program) {
        case 0:  // Disyn Direct
            if (cc == 73) return 16;  // Slider 1 → Disyn Algorithm
            if (cc == 72) return 17;  // Slider 2 → Disyn Param1
            if (cc == 28) return 18;  // Slider 3 → Disyn Param2
            if (cc == 30) return 19;  // Slider 4 → Disyn Level
            if (cc == 74) return 1;   // Slider 5 → Intensity
            if (cc == 71) return 26;  // Slider 6 → Tuning
            if (cc == 1) return 27;   // Slider 7 → Ratio
            break;

        case 1:  // Disyn + Delays
            if (cc == 73) return 28;  // Slider 1 → Delay1 Feedback
            if (cc == 72) return 29;  // Slider 2 → Delay2 Feedback
            if (cc == 28) return 30;  // Slider 3 → Filter Feedback
            if (cc == 30) return 19;  // Slider 4 → Disyn Level
            if (cc == 74) return 1;   // Slider 5 → Intensity
            if (cc == 71) return 26;  // Slider 6 → Tuning
            if (cc == 1) return 27;   // Slider 7 → Ratio
            break;

        case 2:  // Disyn + Filter
            if (cc == 73) return 32;  // Slider 1 → Filter Frequency
            if (cc == 72) return 33;  // Slider 2 → Filter Q
            if (cc == 28) return 34;  // Slider 3 → Filter Shape
            if (cc == 30) return 19;  // Slider 4 → Disyn Level
            if (cc == 74) return 1;   // Slider 5 → Intensity
            if (cc == 71) return 26;  // Slider 6 → Tuning
            if (cc == 1) return 27;   // Slider 7 → Ratio
            break;

        case 3:  // Formant Voice
            if (cc == 73) return 71;  // Slider 1 → F1 (Jaw)
            if (cc == 72) return 10;  // Slider 2 → F2 (Tongue)
            if (cc == 28) return 74;  // Slider 3 → F3 (Lips)
            if (cc == 30) return 75;  // Slider 4 → F4 (Quality)
            if (cc == 74) return 20;  // Slider 5 → Noise Level
            if (cc == 71) return 80;  // Slider 6 → Nasal toggle
            // Slider 7 (1) stays Intensity
            break;

        case 4:  // Hybrid Speech
            if (cc == 73) return 71;  // Slider 1 → F1 (Jaw)
            if (cc == 72) return 10;  // Slider 2 → F2 (Tongue)
            if (cc == 28) return 74;  // Slider 3 → F3 (Lips)
            if (cc == 30) return 75;  // Slider 4 → F4 (Quality)
            if (cc == 74) return 19;  // Slider 5 → Disyn Level
            if (cc == 71) return 20;  // Slider 6 → Noise Level
            // Slider 7 (1) stays Intensity
            break;

        case 5:  // Physical Model
            if (cc == 73) return 28;  // Slider 1 → Delay1 Feedback
            if (cc == 72) return 29;  // Slider 2 → Delay2 Feedback
            if (cc == 28) return 30;  // Slider 3 → Filter Feedback
            if (cc == 30) return 24;  // Slider 4 → Interface Type
            if (cc == 74) return 1;   // Slider 5 → Intensity
            if (cc == 71) return 26;  // Slider 6 → Tuning
            if (cc == 1) return 27;   // Slider 7 → Ratio
            break;

        case 6:  // Full Hybrid (disabled, redirects to 5)
        case 7:  // Experimental (disabled, redirects to 4)
            // These programs redirect, so no remapping needed
            break;
    }

    return cc;  // No remapping for this CC/program
}

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
            if (handle_control_note(event, synth)) {
                break;
            }

            float freq = midi_note_to_frequency(event->note);
            synth_engine_note_on(synth, event->note, freq, event->velocity);
            printf("Note ON: %d (%.1f Hz), velocity %d\n",
                   event->note, freq, event->velocity);
            break;
        }

        case MIDI_NOTE_OFF:
            if (handle_control_note(event, synth)) {
                break;
            }

            synth_engine_note_off(synth, event->note);
            printf("Note OFF: %d\n", event->note);
            break;

        case MIDI_CONTROL_CHANGE: {
            uint8_t cc = event->cc_number;

            // Apply MK-449 remapping first (if enabled)
            if (g_use_mk449_remap) {
                cc = mk449_remap_cc(cc);
            }

            // Apply program-based slider remapping
            cc = remap_slider_cc(cc);

            float value = event->cc_value / 127.0f;  // Normalize to 0-1

            // Track first occurrence of each CC to print its name
            static bool cc_seen[128] = {0};

            // Print CC with name on first occurrence
            const char* cc_name = NULL;
            switch (cc) {
                case 1:  cc_name = "Intensity"; break;
                case 7:  cc_name = "Master Gain"; break;
                case 10: cc_name = "F2 (Tongue)"; break;
                case 16: cc_name = "Disyn Algorithm"; break;
                case 17: cc_name = "Disyn Param1"; break;
                case 18: cc_name = "Disyn Param2"; break;
                case 19: cc_name = "Disyn Level"; break;
                case 20: cc_name = "Noise Level"; break;
                case 21: cc_name = "DC Level"; break;
                case 24: cc_name = "Interface Type"; break;
                case 26: cc_name = "Tuning"; break;
                case 27: cc_name = "Delay Ratio"; break;
                case 28: cc_name = "Delay1 Feedback"; break;
                case 29: cc_name = "Delay2 Feedback"; break;
                case 30: cc_name = "Filter Feedback"; break;
                case 32: cc_name = "Filter Frequency"; break;
                case 33: cc_name = "Filter Q"; break;
                case 34: cc_name = "Filter Shape"; break;
                case 36: cc_name = "LFO Frequency"; break;
                case 37: cc_name = "AM↔FM Depth"; break;
                case 71: cc_name = "F1 (Jaw)"; break;
                case 72: cc_name = "Release"; break;
                case 73: cc_name = "Attack"; break;
                case 74: cc_name = "F3 (Lips)"; break;
                case 75: cc_name = "F4 (Quality)"; break;
                case 80: cc_name = "Nasal"; break;
                case 81: cc_name = "Sing"; break;
                case 82: cc_name = "Shout"; break;
                case 83: cc_name = "Fry"; break;
                default: break;
            }

            if (cc_name && !cc_seen[cc]) {
                printf("CC ch%d: %3u -> %3u  [%s]\n", event->channel + 1, cc, event->cc_value, cc_name);
                cc_seen[cc] = true;
            } else {
                printf("CC ch%d: %3u -> %3u\n", event->channel + 1, cc, event->cc_value);
            }

            // Map CCs to parameters
            switch (cc) {
                // Standard CCs
                case 1:  // Modulation Wheel (repurposed for intensity)
                    synth_engine_set_intensity(synth, value);
                    break;

                case 7:  // Volume (Master Gain)
                    synth_engine_set_master_gain(synth, value);
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

        case MIDI_PROGRAM_CHANGE:
            handle_program_change(event->program_number, synth);
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
    printf("Unified Polyphonic Synthesizer (4-voice default)\n");
    printf("Target: Raspberry Pi 4 (ARM Cortex-A72)\n\n");

    const char* mk_env = getenv("FLUES_MK449_MAP");
    if (mk_env && mk_env[0] == '0') {
        g_use_mk449_remap = false;
    }
    if (g_use_mk449_remap) {
        printf("MK-449 CC remap: enabled (91→28, 92→29, 93→30, 84→27, 5→1)\n");
    }

    const char* ctl_env = getenv("FLUES_CONTROL_NOTES");
    if (ctl_env && ctl_env[0] == '1') {
        g_control_notes_enabled = true;
    }
    if (g_control_notes_enabled) {
        printf("Control notes 36-42: ENABLED (debug toggles)\n");
    } else {
        printf("Control notes 36-42: disabled (set FLUES_CONTROL_NOTES=1 to enable)\n");
    }

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

    // Initialize with Program 0
    handle_program_change(0, g_synth);

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
