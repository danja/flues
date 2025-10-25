#pragma once

#include <algorithm>
#include <cmath>
#include <memory>

#include "modules/FloozySourceModule.hpp"

#include "../../pm-synth/src/modules/EnvelopeModule.hpp"
#include "../../pm-synth/src/modules/InterfaceModule.hpp"
#include "../../pm-synth/src/modules/DelayLinesModule.hpp"
#include "../../pm-synth/src/modules/FeedbackModule.hpp"
#include "../../pm-synth/src/modules/FilterModule.hpp"
#include "../../pm-synth/src/modules/ModulationModule.hpp"
#include "../../pm-synth/src/modules/ReverbModule.hpp"

namespace flues::floozy {

class FloozyEngine {
public:
    explicit FloozyEngine(float sampleRate = 44100.0f)
        : sampleRate(sampleRate),
          source(sampleRate),
          envelope(sampleRate),
          interfaceModule(sampleRate),
          delayLines(sampleRate),
          feedback(),
          filter(sampleRate),
          modulation(sampleRate),
          reverb(sampleRate),
          frequency(440.0f),
          isPlaying(false),
          outputGain(0.8f),
          dcBlockerX1(0.0f),
          dcBlockerY1(0.0f),
          prevDelayOutputs{0.0f, 0.0f},
          prevFilterOutput(0.0f) {}

    void noteOn(float freq) {
        frequency = freq;
        isPlaying = true;

        source.reset();
        envelope.reset();
        interfaceModule.reset();
        interfaceModule.setGate(true);
        delayLines.reset();
        feedback.reset();
        filter.reset();
        modulation.reset();
        reverb.reset();
        dcBlockerX1 = 0.0f;
        dcBlockerY1 = 0.0f;
        prevDelayOutputs = {0.0f, 0.0f};
        prevFilterOutput = 0.0f;

        envelope.setGate(true);
    }

    void noteOff() {
        envelope.setGate(false);
        interfaceModule.setGate(false);
    }

    float process() {
        if (!isPlaying) {
            return 0.0f;
        }

        const flues::pm::ModulationState modState = modulation.process();
        const float modulatedFrequency = frequency * modState.fm;
        const float sourceSignal = source.process(modulatedFrequency);
        const float env = envelope.process();
        const float envelopedSignal = sourceSignal * env;

        const float feedbackSignal = feedback.process(
            prevDelayOutputs.delay1,
            prevDelayOutputs.delay2,
            prevFilterOutput
        );

        const float cleanFeedback = dcBlock(feedbackSignal);
        const float interfaceInput = envelopedSignal + cleanFeedback;
        const float interfaceOutput = interfaceModule.process(interfaceInput);
        const float clampedDelayInput = std::clamp(interfaceOutput, -1.0f, 1.0f);

        const auto delayOutputs = delayLines.process(clampedDelayInput, frequency);
        const float delayMix = (delayOutputs.delay1 + delayOutputs.delay2) * 0.5f;
        const float filterOutput = filter.process(delayMix);
        const float preReverb = filterOutput * modState.am * outputGain;
        const float output = reverb.process(preReverb);

        prevDelayOutputs = delayOutputs;
        prevFilterOutput = filterOutput;

        if (!envelope.isPlaying() &&
            std::abs(output) < 1e-5f &&
            std::abs(prevDelayOutputs.delay1) < 1e-5f &&
            std::abs(prevDelayOutputs.delay2) < 1e-5f) {
            isPlaying = false;
        }

        return output;
    }

    void setAlgorithm(float value) { source.setAlgorithm(value); }
    void setParam1(float value) { source.setParam1(value); }
    void setParam2(float value) { source.setParam2(value); }
    void setToneLevel(float value) { source.setToneLevel(value); }
    void setNoiseLevel(float value) { source.setNoiseLevel(value); }
    void setDCLevel(float value) { source.setDCLevel(value); }
    void setAttack(float value) { envelope.setAttack(value); }
    void setRelease(float value) { envelope.setRelease(value); }
    void setInterfaceType(float value) { interfaceModule.setType(static_cast<int>(std::round(value))); }
    void setInterfaceIntensity(float value) { interfaceModule.setIntensity(value); }
    void setTuning(float value) { delayLines.setTuning(value); }
    void setRatio(float value) { delayLines.setRatio(value); }
    void setDelay1Feedback(float value) { feedback.setDelay1Gain(value); }
    void setDelay2Feedback(float value) { feedback.setDelay2Gain(value); }
    void setFilterFeedback(float value) { feedback.setFilterGain(value); }
    void setFilterFrequency(float value) { filter.setFrequency(value); }
    void setFilterQ(float value) { filter.setQ(value); }
    void setFilterShape(float value) { filter.setShape(value); }
    void setLFOFrequency(float value) { modulation.setFrequency(value); }
    void setModulationTypeLevel(float value) { modulation.setTypeLevel(value); }
    void setReverbSize(float value) { reverb.setSize(value); }
    void setReverbLevel(float value) { reverb.setLevel(value); }
    void setMasterGain(float value) { outputGain = std::clamp(value, 0.0f, 1.0f); }

private:
    float dcBlock(float sample) {
        const float y = sample - dcBlockerX1 + 0.995f * dcBlockerY1;
        dcBlockerX1 = sample;
        dcBlockerY1 = y;
        return y;
    }

    float sampleRate;
    FloozySourceModule source;
    flues::pm::EnvelopeModule envelope;
    flues::pm::InterfaceModule interfaceModule;
    flues::pm::DelayLinesModule delayLines;
    flues::pm::FeedbackModule feedback;
    flues::pm::FilterModule filter;
    flues::pm::ModulationModule modulation;
    flues::pm::ReverbModule reverb;

    float frequency;
    bool isPlaying;
    float outputGain;
    float dcBlockerX1;
    float dcBlockerY1;
    flues::pm::DelayLinesModule::DelayOutputs prevDelayOutputs;
    float prevFilterOutput;
};

} // namespace flues::floozy
