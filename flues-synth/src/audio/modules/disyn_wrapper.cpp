#include "disyn_wrapper.h"
#include "../../../../lv2/disyn/src/modules/OscillatorModule.hpp"

// C++ structure wrapping the OscillatorModule
struct DisynModule {
    flues::disyn::OscillatorModule osc;
    flues::disyn::AlgorithmType algorithm;
    float param1;
    float param2;

    DisynModule(float sampleRate)
        : osc(sampleRate),
          algorithm(flues::disyn::AlgorithmType::DIRICHLET_PULSE),
          param1(0.5f),
          param2(0.5f) {}
};

extern "C" {

DisynModule* disyn_create(float sample_rate) {
    return new DisynModule(sample_rate);
}

void disyn_destroy(DisynModule* disyn) {
    delete disyn;
}

void disyn_set_algorithm(DisynModule* disyn, int algorithm) {
    if (algorithm >= 0 && algorithm <= 6) {
        disyn->algorithm = static_cast<flues::disyn::AlgorithmType>(algorithm);
    }
}

void disyn_set_param1(DisynModule* disyn, float value) {
    disyn->param1 = value;
}

void disyn_set_param2(DisynModule* disyn, float value) {
    disyn->param2 = value;
}

float disyn_process(DisynModule* disyn, float frequency) {
    return disyn->osc.process(disyn->algorithm, disyn->param1, disyn->param2, frequency);
}

void disyn_reset(DisynModule* disyn) {
    disyn->osc.reset();
}

} // extern "C"
