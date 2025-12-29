#ifndef FLUES_SYNTH_DISYN_WRAPPER_H
#define FLUES_SYNTH_DISYN_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

// Opaque structure (hides C++ implementation)
typedef struct DisynModule DisynModule;

// Create Disyn module
DisynModule* disyn_create(float sample_rate);

// Destroy Disyn module
void disyn_destroy(DisynModule* disyn);

// Set algorithm (0-17)
// 0-6: Primitive algorithms (Dirichlet, DSF Single/Double, Tanh Square/Saw, PAF, ModFM)
// 7-13: Combination algorithms (Hybrid Formant, Cascaded, Parallel, Feedback, Morphing, Inharmonic, Adaptive)
// 14-17: Novel extrapolations (Multi-Stage, Freq Asymmetry, Cross-Mod, Taylor)
void disyn_set_algorithm(DisynModule* disyn, int algorithm);

// Set param1 (0-1, algorithm-specific meaning)
void disyn_set_param1(DisynModule* disyn, float value);

// Set param2 (0-1, algorithm-specific meaning)
void disyn_set_param2(DisynModule* disyn, float value);

// Set param3 (0-1, algorithm-specific meaning, used by algorithms 7-16)
void disyn_set_param3(DisynModule* disyn, float value);

// Process one sample at given frequency
float disyn_process(DisynModule* disyn, float frequency);

// Reset phase accumulators
void disyn_reset(DisynModule* disyn);

#ifdef __cplusplus
}
#endif

#endif // FLUES_SYNTH_DISYN_WRAPPER_H
