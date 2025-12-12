// Flues-Synth - Unified Polyphonic Synthesizer
// 4-voice polyphonic, headless operation with MIDI CC control

#include "synth_engine.h"
#include "audio_backend_alsa.h"
#include "midi_backend_alsa.h"
#include "midi_mapping.h"
#include "dsp_modules.h"
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
static bool g_control_notes_enabled = false;  // off by default to avoid consuming low notes
static int g_current_program = 0;  // Current MIDI program (0-7)

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
        case 0:  // Disyn Echo - Disyn through single delay line
            printf("Program 0: Disyn Echo\n");
            printf("  - Disyn directly into single delay line + interface selector\n");
            printf("  - Sliders: Alg(73→16), P1(72→17), P2(28→18), Interface(30→24), Intensity(74→1), Tuning(71→26), Feedback(1→28), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, false);
            synth_engine_enable_feedback(synth, true);  // Enable feedback for delay
            synth_engine_enable_filter(synth, false);
            synth_engine_set_disyn_level(synth, 0.6f);
            synth_engine_set_intensity(synth, 0.5f);  // Default interface intensity (still used by chosen interface)
            synth_engine_set_delay1_feedback(synth, 0.3f);
            synth_engine_set_delay2_feedback(synth, 0.0f);  // Disable second delay
            synth_engine_set_filter_feedback(synth, 0.0f);  // No filter in path
            synth_engine_set_interface_type(synth, INTERFACE_REED);
            synth_engine_set_master_gain(synth, 0.6f);
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

        case 7:  // Disyn Direct - Raw distortion synth straight to output
            printf("Program 7: Disyn Direct\n");
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

        case 8:  // ModFM Formant - Hybrid Formant Engine
            printf("Program 8: ModFM Formant\n");
            printf("  - ModFM → Formants (F1-F4) → Output\n");
            printf("  - Voice-like lead with natural FM evolution and formant control\n");
            printf("  - Sliders: Index(73→17), Ratio(72→18), F1(28→71), F2(30→10), F3(74→74), F4(71→75), Master(1→7), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, true);
            synth_engine_enable_feedback(synth, false);
            synth_engine_enable_filter(synth, false);
            synth_engine_set_disyn_algorithm(synth, 6);  // ModFM
            synth_engine_set_disyn_level(synth, 0.6f);
            synth_engine_set_f1(synth, 500.0f);
            synth_engine_set_f2(synth, 1500.0f);
            synth_engine_set_f3(synth, 2500.0f);
            synth_engine_set_f4(synth, 3500.0f);
            synth_engine_set_master_gain(synth, 0.7f);
            break;

        case 9:  // DSF Inharmonic Explorer - Metallic bells/gongs
            printf("Program 9: DSF Inharmonic Explorer\n");
            printf("  - DSF Single → Delays (feedback) → Output\n");
            printf("  - Bell synthesis with harmonic/inharmonic ratio control\n");
            printf("  - Sliders: Decay(73→17), Ratio(72→18), Dly1(28→28), Dly2(30→29), Intensity(74→1), Tuning(71→26), DlyRatio(1→27), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, false);
            synth_engine_enable_feedback(synth, true);
            synth_engine_enable_filter(synth, false);
            synth_engine_set_disyn_algorithm(synth, 1);  // DSF Single
            synth_engine_set_disyn_level(synth, 0.6f);
            synth_engine_set_delay1_feedback(synth, 0.4f);
            synth_engine_set_delay2_feedback(synth, 0.2f);
            break;

        case 10:  // PAF Direct - Vowel synthesis with filter
            printf("Program 10: PAF Direct\n");
            printf("  - PAF → Filter (SVF) → Output\n");
            printf("  - Vowel-like synthesis with filter articulation\n");
            printf("  - Sliders: Formant(73→17), Bandwidth(72→18), Freq(28→32), Q(30→33), Shape(74→34), Tuning(71→26), Master(1→7), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, false);
            synth_engine_enable_feedback(synth, false);
            synth_engine_enable_filter(synth, true);
            synth_engine_set_disyn_algorithm(synth, 5);  // PAF
            synth_engine_set_disyn_level(synth, 0.8f);
            synth_engine_set_filter_frequency(synth, 3000.0f);
            synth_engine_set_filter_q(synth, 2.0f);
            break;

        case 11:  // Cascaded DSF+PAF - Inharmonic resonator
            printf("Program 11: Cascaded DSF+PAF\n");
            printf("  - DSF Double → Formants (F1-F4) → Output\n");
            printf("  - Bell-like timbres with formant resonance\n");
            printf("  - Sliders: Decay(73→17), Ratio(72→18), F1(28→71), F2(30→10), F3(74→74), Tuning(71→26), Master(1→7), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, true);
            synth_engine_enable_feedback(synth, false);
            synth_engine_enable_filter(synth, false);
            synth_engine_set_disyn_algorithm(synth, 2);  // DSF Double
            synth_engine_set_disyn_level(synth, 0.5f);
            synth_engine_set_f1(synth, 800.0f);
            synth_engine_set_f2(synth, 2400.0f);
            synth_engine_set_f3(synth, 4000.0f);
            break;

        case 12:  // Tanh Spectral - Aggressive filtered synthesis
            printf("Program 12: Tanh Spectral\n");
            printf("  - Tanh Saw → Filter (SVF) → Feedback → Output\n");
            printf("  - Aggressive filtered synthesis with feedback resonance\n");
            printf("  - Sliders: Drive(73→17), Blend(72→18), Freq(28→32), Q(30→33), Shape(74→34), FiltFB(71→30), Intensity(1→1), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, false);
            synth_engine_enable_feedback(synth, true);
            synth_engine_enable_filter(synth, true);
            synth_engine_set_disyn_algorithm(synth, 4);  // Tanh Saw
            synth_engine_set_disyn_level(synth, 0.7f);
            synth_engine_set_filter_frequency(synth, 1500.0f);
            synth_engine_set_filter_q(synth, 3.0f);
            synth_engine_set_filter_feedback(synth, 0.4f);
            break;

        case 13:  // Hybrid DSF→Formant - Rich vocal synthesis
            printf("Program 13: Hybrid DSF→Formant\n");
            printf("  - DSF Single → Formants (F1-F4 full cascade) → Output\n");
            printf("  - Rich vocal synthesis with harmonic source\n");
            printf("  - Sliders: Decay(73→17), Ratio(72→18), F1(28→71), F2(30→10), F3(74→74), F4(71→75), Master(1→7), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, true);
            synth_engine_enable_feedback(synth, false);
            synth_engine_enable_filter(synth, false);
            synth_engine_set_disyn_algorithm(synth, 1);  // DSF Single
            synth_engine_set_disyn_level(synth, 0.4f);
            synth_engine_set_f1(synth, 600.0f);
            synth_engine_set_f2(synth, 1500.0f);
            synth_engine_set_f3(synth, 3000.0f);
            synth_engine_set_f4(synth, 4000.0f);
            break;

        case 14:  // Feedback ModFM - Metallic bell timbres
            printf("Program 14: Feedback ModFM\n");
            printf("  - ModFM → Delays → Filter → Feedback (all returns) → Output\n");
            printf("  - Metallic bell-like timbres with complex feedback behavior\n");
            printf("  - Sliders: Index(73→17), Ratio(72→18), Dly1(28→28), Dly2(30→29), FiltFB(74→30), Tuning(71→26), DlyRatio(1→27), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, false);
            synth_engine_enable_feedback(synth, true);
            synth_engine_enable_filter(synth, true);
            synth_engine_set_disyn_algorithm(synth, 6);  // ModFM
            synth_engine_set_disyn_level(synth, 0.5f);
            synth_engine_set_delay1_feedback(synth, 0.5f);
            synth_engine_set_delay2_feedback(synth, 0.3f);
            synth_engine_set_filter_feedback(synth, 0.4f);
            synth_engine_set_filter_frequency(synth, 2000.0f);
            synth_engine_set_filter_q(synth, 2.5f);
            break;

        case 15:  // Dirichlet Explorer - Harmonic pulse with filter sweeping
            printf("Program 15: Dirichlet Explorer\n");
            printf("  - Dirichlet Pulse → Filter (SVF) → Output\n");
            printf("  - Classic harmonic synthesis with filter control\n");
            printf("  - Sliders: Harmonics(73→17), Tilt(72→18), Freq(28→32), Q(30→33), Shape(74→34), Tuning(71→26), Master(1→7), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, false);
            synth_engine_enable_feedback(synth, false);
            synth_engine_enable_filter(synth, true);
            synth_engine_set_disyn_algorithm(synth, 0);  // Dirichlet Pulse
            synth_engine_set_disyn_level(synth, 0.8f);
            synth_engine_set_filter_frequency(synth, 5000.0f);
            synth_engine_set_filter_q(synth, 1.5f);
            synth_engine_set_master_gain(synth, 0.7f);
            break;

        case 16:  // Multi-Algorithm Demo - A/B comparison mode
            printf("Program 16: Multi-Algorithm Demo\n");
            printf("  - Disyn (any algo, selected by slider 1) → Output\n");
            printf("  - Educational/demo mode for comparing all 7 algorithms\n");
            printf("  - Sliders: Algorithm(73→16), P1(72→17), P2(28→18), Level(30→19), Intensity(74→1), Tuning(71→26), Master(1→7), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, false);
            synth_engine_enable_feedback(synth, false);
            synth_engine_enable_filter(synth, false);
            synth_engine_set_disyn_level(synth, 0.6f);
            synth_engine_set_master_gain(synth, 0.7f);
            break;

        case 17:  // Spectral Sculptor - Adaptive filter emulation
            printf("Program 17: Spectral Sculptor\n");
            printf("  - DSF Double → Filter (SVF) → Feedback (all returns) → Output\n");
            printf("  - Dynamic spectral animation for acid-style sounds\n");
            printf("  - Sliders: Decay(73→17), Ratio(72→18), Freq(28→32), Q(30→33), Shape(74→34), Dly1FB(71→28), FiltFB(1→30), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, false);
            synth_engine_enable_feedback(synth, true);
            synth_engine_enable_filter(synth, true);
            synth_engine_set_disyn_algorithm(synth, 2);  // DSF Double
            synth_engine_set_disyn_level(synth, 0.6f);
            synth_engine_set_delay1_feedback(synth, 0.3f);
            synth_engine_set_delay2_feedback(synth, 0.25f);
            synth_engine_set_filter_feedback(synth, 0.35f);
            synth_engine_set_filter_frequency(synth, 2000.0f);
            synth_engine_set_filter_q(synth, 2.5f);
            break;

        case 18:  // Hybrid Formant Engine - Combination 1
            printf("Program 18: Hybrid Formant Engine\n");
            printf("  - ModFM → Formants (F1-F4) → Output\n");
            printf("  - Voice-like lead with natural FM evolution and formant resonance\n");
            printf("  - Sliders: Index(73→17), Ratio(72→18), F1(28→71), F2(30→10), F3(74→74), F4(71→75), Master(1→7), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, true);
            synth_engine_enable_feedback(synth, false);
            synth_engine_enable_filter(synth, false);
            synth_engine_set_disyn_algorithm(synth, 6);  // ModFM
            synth_engine_set_disyn_level(synth, 0.5f);
            synth_engine_set_f1(synth, 800.0f);
            synth_engine_set_f2(synth, 1200.0f);
            synth_engine_set_f3(synth, 2400.0f);
            synth_engine_set_f4(synth, 3500.0f);
            synth_engine_set_master_gain(synth, 0.7f);
            break;

        case 19:  // Cascaded Spectral Sculptor - Combination 2
            printf("Program 19: Cascaded Spectral Sculptor\n");
            printf("  - DSF Single → Filter (SVF) → Feedback → Output\n");
            printf("  - Animated morphing leads with DSF harmonic control + filter shaping\n");
            printf("  - Sliders: Decay(73→17), Ratio(72→18), Freq(28→32), Q(30→33), Shape(74→34), FiltFB(71→30), Master(1→7), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, false);
            synth_engine_enable_feedback(synth, true);
            synth_engine_enable_filter(synth, true);
            synth_engine_set_disyn_algorithm(synth, 1);  // DSF Single
            synth_engine_set_disyn_level(synth, 0.6f);
            synth_engine_set_filter_frequency(synth, 3000.0f);
            synth_engine_set_filter_q(synth, 3.0f);
            synth_engine_set_filter_feedback(synth, 0.4f);
            synth_engine_set_master_gain(synth, 0.7f);
            break;

        case 20:  // Parallel Formant Bank - Combination 3
            printf("Program 20: Parallel Formant Bank\n");
            printf("  - ModFM + Noise → Formants (F1-F4) → Output\n");
            printf("  - Rich organic complexity with chorused thick timbres\n");
            printf("  - Sliders: Index(73→17), Ratio(72→18), F1(28→71), F2(30→10), F3(74→74), F4(71→75), Noise(1→20), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, true);
            synth_engine_enable_formants(synth, true);
            synth_engine_enable_feedback(synth, false);
            synth_engine_enable_filter(synth, false);
            synth_engine_set_disyn_algorithm(synth, 6);  // ModFM
            synth_engine_set_disyn_level(synth, 0.4f);
            synth_engine_set_noise_level(synth, 0.2f);
            synth_engine_set_f1(synth, 800.0f);
            synth_engine_set_f2(synth, 1500.0f);
            synth_engine_set_f3(synth, 2400.0f);
            synth_engine_set_f4(synth, 3200.0f);
            synth_engine_set_master_gain(synth, 0.7f);
            break;

        case 21:  // Feedback Loop Network - Combination 4
            printf("Program 21: Feedback Loop Network\n");
            printf("  - ModFM → Delays → Filter → Heavy Feedback → Output\n");
            printf("  - Metallic bell-like timbres with chaotic evolution\n");
            printf("  - Sliders: Index(73→17), Ratio(72→18), Dly1FB(28→28), Dly2FB(30→29), FiltFB(74→30), Freq(71→32), Q(1→33), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, false);
            synth_engine_enable_feedback(synth, true);
            synth_engine_enable_filter(synth, true);
            synth_engine_set_disyn_algorithm(synth, 6);  // ModFM
            synth_engine_set_disyn_level(synth, 0.5f);
            synth_engine_set_delay1_feedback(synth, 0.5f);
            synth_engine_set_delay2_feedback(synth, 0.45f);
            synth_engine_set_filter_feedback(synth, 0.5f);
            synth_engine_set_filter_frequency(synth, 2000.0f);
            synth_engine_set_filter_q(synth, 2.0f);
            synth_engine_set_master_gain(synth, 0.65f);
            break;

        case 22:  // Morphing Spectral - Combination 5
            printf("Program 22: Morphing Spectral Engine\n");
            printf("  - Algorithm morphing (via slider) + Formants → Output\n");
            printf("  - Live performance control for exploring timbral space\n");
            printf("  - Sliders: Algorithm(73→16), P1(72→17), P2(28→18), F1(30→71), F2(74→10), Intensity(71→1), Master(1→7), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, true);
            synth_engine_enable_feedback(synth, false);
            synth_engine_enable_filter(synth, false);
            synth_engine_set_disyn_level(synth, 0.5f);
            synth_engine_set_f1(synth, 600.0f);
            synth_engine_set_f2(synth, 1400.0f);
            synth_engine_set_master_gain(synth, 0.7f);
            break;

        case 23:  // Inharmonic Bell Resonator - Combination 6
            printf("Program 23: Inharmonic Bell Resonator\n");
            printf("  - DSF (golden ratio) → Formants → Delays → Output\n");
            printf("  - Bell synthesis with gong-like metallic percussion\n");
            printf("  - Sliders: Decay(73→17), Ratio(72→18), F1(28→71), F3(30→74), Tuning(74→26), Dly1FB(71→28), Master(1→7), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, true);
            synth_engine_enable_feedback(synth, true);
            synth_engine_enable_filter(synth, false);
            synth_engine_set_disyn_algorithm(synth, 1);  // DSF Single
            synth_engine_set_disyn_level(synth, 0.5f);
            synth_engine_set_f1(synth, 800.0f);
            synth_engine_set_f3(synth, 2800.0f);
            synth_engine_set_delay1_feedback(synth, 0.35f);
            synth_engine_set_tuning(synth, 0.0f);
            synth_engine_set_master_gain(synth, 0.7f);
            break;

        case 24:  // Filter Sweep Emulator - Combination 7
            printf("Program 24: Filter Sweep Emulator\n");
            printf("  - DSF + ModFM (mixed) → Filter → LFO Mod → Output\n");
            printf("  - Acid-style sounds with CPU-efficient filtered synthesis\n");
            printf("  - Sliders: Decay(73→17), Index(72→17), Freq(28→32), Q(30→33), LFO(74→36), AM↔FM(71→37), Master(1→7), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, false);
            synth_engine_enable_feedback(synth, false);
            synth_engine_enable_filter(synth, true);
            synth_engine_set_disyn_algorithm(synth, 6);  // ModFM (we'll use P1 for DSF-like control)
            synth_engine_set_disyn_level(synth, 0.6f);
            synth_engine_set_filter_frequency(synth, 1500.0f);
            synth_engine_set_filter_q(synth, 4.0f);
            synth_engine_set_lfo_frequency(synth, 2.0f);
            synth_engine_set_am_fm_depth(synth, 0.3f);
            synth_engine_set_master_gain(synth, 0.7f);
            break;

        case 25:  // Multi-Stage Waveshaper - Novel Extrapolation 1
            printf("Program 25: Multi-Stage Waveshaper\n");
            printf("  - Tanh Saw → Filter (cascade) → Formants (exponential shape) → Output\n");
            printf("  - Each stage adds different spectral characteristics\n");
            printf("  - Sliders: Drive(73→17), Blend(72→18), Freq(28→32), Q(30→33), F1(74→71), F2(71→10), Master(1→7), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, true);
            synth_engine_enable_feedback(synth, false);
            synth_engine_enable_filter(synth, true);
            synth_engine_set_disyn_algorithm(synth, 4);  // Tanh Saw
            synth_engine_set_disyn_level(synth, 0.6f);
            synth_engine_set_filter_frequency(synth, 2500.0f);
            synth_engine_set_filter_q(synth, 2.0f);
            synth_engine_set_f1(synth, 1000.0f);
            synth_engine_set_f2(synth, 2000.0f);
            synth_engine_set_master_gain(synth, 0.7f);
            break;

        case 26:  // Spectral Animator - Novel Extrapolation 3
            printf("Program 26: Spectral Animator\n");
            printf("  - Algorithm selection → Formants → LFO Modulation → Output\n");
            printf("  - Cross-algorithm parameter animation with coupled evolution\n");
            printf("  - Sliders: Algorithm(73→16), P1(72→17), F1(28→71), F2(30→10), LFO(74→36), AM↔FM(71→37), Master(1→7), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, true);
            synth_engine_enable_feedback(synth, false);
            synth_engine_enable_filter(synth, false);
            synth_engine_set_disyn_level(synth, 0.5f);
            synth_engine_set_f1(synth, 700.0f);
            synth_engine_set_f2(synth, 1600.0f);
            synth_engine_set_lfo_frequency(synth, 3.0f);
            synth_engine_set_am_fm_depth(synth, 0.4f);
            synth_engine_set_master_gain(synth, 0.7f);
            break;

        case 27:  // Feedback Chaos Engine - Experimental
            printf("Program 27: Feedback Chaos Engine\n");
            printf("  - DSF → Interface → Delays → Filter → Maximum Feedback → Output\n");
            printf("  - Unpredictable organic evolution with physical modeling\n");
            printf("  - Sliders: Decay(73→17), Ratio(72→18), Interface(28→24), Dly1FB(30→28), Dly2FB(74→29), FiltFB(71→30), Intensity(1→1), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, true);
            synth_engine_enable_formants(synth, false);
            synth_engine_enable_feedback(synth, true);
            synth_engine_enable_filter(synth, true);
            synth_engine_set_disyn_algorithm(synth, 2);  // DSF Double
            synth_engine_set_disyn_level(synth, 0.4f);
            synth_engine_set_noise_level(synth, 0.15f);
            synth_engine_set_interface_type(synth, 2);  // Reed
            synth_engine_set_delay1_feedback(synth, 0.6f);
            synth_engine_set_delay2_feedback(synth, 0.55f);
            synth_engine_set_filter_feedback(synth, 0.5f);
            synth_engine_set_filter_frequency(synth, 1800.0f);
            synth_engine_set_filter_q(synth, 3.5f);
            synth_engine_set_intensity(synth, 0.6f);
            synth_engine_set_master_gain(synth, 0.65f);
            break;

        case 28:  // Vocal Morph Matrix - Experimental
            printf("Program 28: Vocal Morph Matrix\n");
            printf("  - ModFM → All 4 Formants + Vocal Modes → Output\n");
            printf("  - Complete vocal synthesis with singing and expression\n");
            printf("  - Sliders: Index(73→17), Ratio(72→18), F1(28→71), F2(30→10), Nasal(74→80), Sing(71→81), Shout(1→82), Attack(27→73), Release(7→72)\n");
            synth_engine_enable_disyn(synth, true);
            synth_engine_enable_noise(synth, false);
            synth_engine_enable_formants(synth, true);
            synth_engine_enable_feedback(synth, false);
            synth_engine_enable_filter(synth, false);
            synth_engine_set_disyn_algorithm(synth, 6);  // ModFM
            synth_engine_set_disyn_level(synth, 0.5f);
            synth_engine_set_f1(synth, 700.0f);
            synth_engine_set_f2(synth, 1220.0f);
            synth_engine_set_f3(synth, 2600.0f);
            synth_engine_set_f4(synth, 3500.0f);
            synth_engine_set_nasal(synth, 0.0f);
            synth_engine_set_sing(synth, 0.0f);
            synth_engine_set_shout(synth, 0.0f);
            synth_engine_set_master_gain(synth, 0.7f);
            break;

        default:
            printf("Program %d: Unknown (using program 0)\n", program);
            handle_program_change(0, synth);
            return;
    }

    printf("═══════════════════════════════════════════════════════════\n\n");
}

// Apply parameter value to synth engine
// Clean mapping using parameter constants instead of CC numbers
static void apply_parameter(SynthEngine* synth, SynthParameter param, float value) {
    switch (param) {
        case PARAM_DISYN_ALGORITHM:
            synth_engine_set_disyn_algorithm(synth, (int)(value * 6.999f));
            break;
        case PARAM_DISYN_PARAM1:
            synth_engine_set_disyn_param1(synth, value);
            break;
        case PARAM_DISYN_PARAM2:
            synth_engine_set_disyn_param2(synth, value);
            break;
        case PARAM_DISYN_LEVEL:
            synth_engine_set_disyn_level(synth, value);
            break;
        case PARAM_NOISE_LEVEL:
            synth_engine_set_noise_level(synth, value);
            break;
        case PARAM_DC_LEVEL:
            synth_engine_set_dc_level(synth, value);
            break;
        case PARAM_INTENSITY:
            synth_engine_set_intensity(synth, value);
            break;
        case PARAM_TUNING:
            synth_engine_set_tuning(synth, (value - 0.5f) * 24.0f);  // -12 to +12 semitones
            break;
        case PARAM_RATIO:
            synth_engine_set_ratio(synth, exp_map(value, 0.5f, 2.0f));
            break;
        case PARAM_DELAY1_FEEDBACK:
            synth_engine_set_delay1_feedback(synth, value);
            break;
        case PARAM_DELAY2_FEEDBACK:
            synth_engine_set_delay2_feedback(synth, value);
            break;
        case PARAM_FILTER_FEEDBACK:
            synth_engine_set_filter_feedback(synth, value);
            break;
        case PARAM_FILTER_FREQ:
            synth_engine_set_filter_frequency(synth, exp_map(value, 20.0f, 20000.0f));
            break;
        case PARAM_FILTER_Q:
            synth_engine_set_filter_q(synth, exp_map(value, 0.1f, 10.0f));
            break;
        case PARAM_FILTER_SHAPE:
            synth_engine_set_filter_shape(synth, value);
            break;
        case PARAM_F1:
            synth_engine_set_f1(synth, exp_map(value, 200.0f, 1000.0f));
            break;
        case PARAM_F2:
            synth_engine_set_f2(synth, exp_map(value, 500.0f, 3000.0f));
            break;
        case PARAM_F3:
            synth_engine_set_f3(synth, exp_map(value, 1500.0f, 4000.0f));
            break;
        case PARAM_F4:
            synth_engine_set_f4(synth, exp_map(value, 2500.0f, 4500.0f));
            break;
        case PARAM_NASAL:
            synth_engine_set_nasal(synth, value >= 0.5f);
            break;
        case PARAM_SING:
            synth_engine_set_sing(synth, value >= 0.5f);
            break;
        case PARAM_SHOUT:
            synth_engine_set_shout(synth, value >= 0.5f);
            break;
        case PARAM_FRY:
            synth_engine_set_fry(synth, value >= 0.5f);
            break;
        case PARAM_ATTACK:
            synth_engine_set_attack(synth, value);
            break;
        case PARAM_RELEASE:
            synth_engine_set_release(synth, value);
            break;
        case PARAM_LFO_FREQ:
            synth_engine_set_lfo_frequency(synth, exp_map(value, 0.1f, 20.0f));
            break;
        case PARAM_AM_FM_DEPTH:
            synth_engine_set_am_fm_depth(synth, (value - 0.5f) * 2.0f);  // -1 to +1
            break;
        case PARAM_INTERFACE_TYPE:
            synth_engine_set_interface_type(synth, (int)(value * 11.999f));
            break;
        case PARAM_MASTER_GAIN:
            synth_engine_set_master_gain(synth, value);
            break;
        case PARAM_NONE:
            // Unmapped parameter, do nothing
            break;
    }
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

            // Clean parameter-based mapping system
            // Find which slider this CC corresponds to
            int slider_index = midi_find_slider_by_cc(cc);

            float value = event->cc_value / 127.0f;  // Normalize to 0-1

            if (slider_index >= 0) {
                // This is a hardware slider - map to parameter based on current program
                SynthParameter param = midi_get_slider_parameter(g_current_program, slider_index);
                const char* param_name = midi_get_parameter_name(param);

                printf("Slider %d (CC %d) → %s = %d\n",
                       slider_index + 1, cc, param_name, event->cc_value);

                // Apply the parameter value to the engine
                apply_parameter(synth, param, value);
            } else {
                // Non-slider CC: ignore under the simplified mapping scheme
                printf("Unmapped CC ch%d: %3u -> %3u\n", event->channel + 1, cc, event->cc_value);
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
