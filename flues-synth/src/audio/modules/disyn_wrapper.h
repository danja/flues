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

// Set algorithm (0-6)
// 0: Dirichlet Pulse, 1: DSF Single, 2: DSF Double
// 3: Tanh Square, 4: Tanh Saw, 5: PAF, 6: Modified FM
void disyn_set_algorithm(DisynModule* disyn, int algorithm);

// Set param1 (0-1, algorithm-specific meaning)
void disyn_set_param1(DisynModule* disyn, float value);

// Set param2 (0-1, algorithm-specific meaning)
void disyn_set_param2(DisynModule* disyn, float value);

// Process one sample at given frequency
float disyn_process(DisynModule* disyn, float frequency);

// Reset phase accumulators
void disyn_reset(DisynModule* disyn);

#ifdef __cplusplus
}
#endif

#endif // FLUES_SYNTH_DISYN_WRAPPER_H
