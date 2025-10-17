#pragma once

#include <algorithm>

namespace flues::pm {

class FeedbackModule {
public:
    FeedbackModule()
        : delay1Gain(0.95f),
          delay2Gain(0.95f),
          filterGain(0.0f) {}

    void setDelay1Gain(float value) {
        delay1Gain = std::clamp(value, 0.0f, 1.0f) * 0.99f;
    }

    void setDelay2Gain(float value) {
        delay2Gain = std::clamp(value, 0.0f, 1.0f) * 0.99f;
    }

    void setFilterGain(float value) {
        filterGain = std::clamp(value, 0.0f, 1.0f) * 0.99f;
    }

    float process(float delay1Output, float delay2Output, float filterOutput) const {
        return delay1Output * delay1Gain +
               delay2Output * delay2Gain +
               filterOutput * filterGain;
    }

    void reset() {}

private:
    float delay1Gain;
    float delay2Gain;
    float filterGain;
};

} // namespace flues::pm
