#pragma once

namespace flues::disyn {

enum class AlgorithmType : int {
    // Primitive algorithms (0-6)
    DIRICHLET_PULSE = 0,
    DSF_SINGLE = 1,
    DSF_DOUBLE = 2,
    TANH_SQUARE = 3,
    TANH_SAW = 4,
    PAF = 5,
    MOD_FM = 6,

    // Combination algorithms (7-13)
    COMBINATION_1_HYBRID_FORMANT = 7,     // ModFM + 3× PAF formants
    COMBINATION_2_CASCADED = 8,            // DSF → AsymFM → Tanh
    COMBINATION_3_PARALLEL_BANK = 9,       // 3× ModFM + 2× PAF mixed
    COMBINATION_4_FEEDBACK = 10,           // ModFM with feedback loop
    COMBINATION_5_MORPHING = 11,           // DSF ↔ ModFM ↔ PAF crossfade
    COMBINATION_6_INHARMONIC = 12,         // DSF (φ ratio) → PAF
    COMBINATION_7_ADAPTIVE_FILTER = 13,    // DSF + ModFM (filter emulation)

    // Novel extrapolations (14-17)
    NOVEL_1_MULTISTAGE = 14,               // Tanh → Exp → Ring mod
    NOVEL_2_FREQ_ASYMMETRY = 15,           // Frequency-dependent AsymFM
    NOVEL_3_CROSS_MOD = 16,                // Cross-algorithm modulation
    NOVEL_4_TAYLOR = 17,                   // Taylor series approximation
    TRAJECTORY = 18                        // Polygonal trajectory oscillator
};

} // namespace flues::disyn
