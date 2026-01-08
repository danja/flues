#pragma once

#include <cmath>

#include "../algorithms/AlgorithmOutput.hpp"
#include "../algorithms/AlgorithmTypes.hpp"
#include "../algorithms/AlgorithmUtils.hpp"
#include "../algorithms/Combination1HybridFormantAlgorithm.hpp"
#include "../algorithms/Combination2CascadedAlgorithm.hpp"
#include "../algorithms/Combination3ParallelBankAlgorithm.hpp"
#include "../algorithms/Combination4FeedbackAlgorithm.hpp"
#include "../algorithms/Combination5MorphingAlgorithm.hpp"
#include "../algorithms/Combination6InharmonicAlgorithm.hpp"
#include "../algorithms/Combination7AdaptiveFilterAlgorithm.hpp"
#include "../algorithms/DirichletPulseAlgorithm.hpp"
#include "../algorithms/DSFDoubleAlgorithm.hpp"
#include "../algorithms/DSFSingleAlgorithm.hpp"
#include "../algorithms/ModFMAlgorithm.hpp"
#include "../algorithms/Novel1MultistageAlgorithm.hpp"
#include "../algorithms/Novel2FreqAsymmetryAlgorithm.hpp"
#include "../algorithms/Novel3CrossModAlgorithm.hpp"
#include "../algorithms/Novel4TaylorAlgorithm.hpp"
#include "../algorithms/PAFAlgorithm.hpp"
#include "../algorithms/TanhSawAlgorithm.hpp"
#include "../algorithms/TanhSquareAlgorithm.hpp"
#include "../algorithms/TrajectoryAlgorithm.hpp"

namespace flues::disyn {

class OscillatorModule {
public:
    explicit OscillatorModule(float sampleRate = 44100.0f)
        : sampleRate(sampleRate),
          fallbackPhase(0.0f),
          dirichlet(sampleRate),
          dsfSingle(sampleRate),
          dsfDouble(sampleRate),
          tanhSquare(sampleRate),
          tanhSaw(sampleRate),
          paf(sampleRate),
          modfm(sampleRate),
          combination1(sampleRate),
          combination2(sampleRate),
          combination3(sampleRate),
          combination4(sampleRate),
          combination5(sampleRate),
          combination6(sampleRate),
          combination7(sampleRate),
          novel1(sampleRate),
          novel2(sampleRate),
          novel3(sampleRate),
          novel4(sampleRate),
          trajectory(sampleRate) {}

    void reset() {
        fallbackPhase = 0.0f;
        dirichlet.reset();
        dsfSingle.reset();
        dsfDouble.reset();
        tanhSquare.reset();
        tanhSaw.reset();
        paf.reset();
        modfm.reset();
        combination1.reset();
        combination2.reset();
        combination3.reset();
        combination4.reset();
        combination5.reset();
        combination6.reset();
        combination7.reset();
        novel1.reset();
        novel2.reset();
        novel3.reset();
        novel4.reset();
        trajectory.reset();
    }

    // param3 defaults for compatibility with older hosts/presets that only provided two params.
    AlgorithmOutput process(AlgorithmType algorithm, float pitch, float param1, float param2, float param3 = 0.5f) {
        switch (algorithm) {
            case AlgorithmType::DIRICHLET_PULSE:
                return dirichlet.process(pitch, param1, param2, param3);
            case AlgorithmType::DSF_SINGLE:
                return dsfSingle.process(pitch, param1, param2, param3);
            case AlgorithmType::DSF_DOUBLE:
                return dsfDouble.process(pitch, param1, param2, param3);
            case AlgorithmType::TANH_SQUARE:
                return tanhSquare.process(pitch, param1, param2, param3);
            case AlgorithmType::TANH_SAW:
                return tanhSaw.process(pitch, param1, param2, param3);
            case AlgorithmType::PAF:
                return paf.process(pitch, param1, param2, param3);
            case AlgorithmType::MOD_FM:
                return modfm.process(pitch, param1, param2, param3);

            case AlgorithmType::COMBINATION_1_HYBRID_FORMANT:
                return combination1.process(pitch, param1, param2, param3);
            case AlgorithmType::COMBINATION_2_CASCADED:
                return combination2.process(pitch, param1, param2, param3);
            case AlgorithmType::COMBINATION_3_PARALLEL_BANK:
                return combination3.process(pitch, param1, param2, param3);
            case AlgorithmType::COMBINATION_4_FEEDBACK:
                return combination4.process(pitch, param1, param2, param3);
            case AlgorithmType::COMBINATION_5_MORPHING:
                return combination5.process(pitch, param1, param2, param3);
            case AlgorithmType::COMBINATION_6_INHARMONIC:
                return combination6.process(pitch, param1, param2, param3);
            case AlgorithmType::COMBINATION_7_ADAPTIVE_FILTER:
                return combination7.process(pitch, param1, param2, param3);

            case AlgorithmType::NOVEL_1_MULTISTAGE:
                return novel1.process(pitch, param1, param2, param3);
            case AlgorithmType::NOVEL_2_FREQ_ASYMMETRY:
                return novel2.process(pitch, param1, param2, param3);
            case AlgorithmType::NOVEL_3_CROSS_MOD:
                return novel3.process(pitch, param1, param2, param3);
            case AlgorithmType::NOVEL_4_TAYLOR:
                return novel4.process(pitch, param1, param2, param3);
            case AlgorithmType::TRAJECTORY:
                return trajectory.process(pitch, param1, param2, param3);

            default:
                return processSine(pitch);
        }
    }

private:
    AlgorithmOutput processSine(float pitch) {
        fallbackPhase = stepPhase(fallbackPhase, pitch, sampleRate);
        const float output = std::sin(fallbackPhase * TWO_PI);
        return {output, output};
    }

    float sampleRate;
    float fallbackPhase;

    DirichletPulseAlgorithm dirichlet;
    DSFSingleAlgorithm dsfSingle;
    DSFDoubleAlgorithm dsfDouble;
    TanhSquareAlgorithm tanhSquare;
    TanhSawAlgorithm tanhSaw;
    PAFAlgorithm paf;
    ModFMAlgorithm modfm;

    Combination1HybridFormantAlgorithm combination1;
    Combination2CascadedAlgorithm combination2;
    Combination3ParallelBankAlgorithm combination3;
    Combination4FeedbackAlgorithm combination4;
    Combination5MorphingAlgorithm combination5;
    Combination6InharmonicAlgorithm combination6;
    Combination7AdaptiveFilterAlgorithm combination7;

    Novel1MultistageAlgorithm novel1;
    Novel2FreqAsymmetryAlgorithm novel2;
    Novel3CrossModAlgorithm novel3;
    Novel4TaylorAlgorithm novel4;
    TrajectoryAlgorithm trajectory;
};

} // namespace flues::disyn
