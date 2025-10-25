#pragma once

#include <cmath>
#include <algorithm>

#include "modules/SourcesModule.hpp"
#include "modules/EnvelopeModule.hpp"
#include "modules/InterfaceModule.hpp"
#include "modules/DelayLinesModule.hpp"
#include "modules/FeedbackModule.hpp"
#include "modules/FilterModule.hpp"
#include "modules/ModulationModule.hpp"
#include "modules/ReverbModule.hpp"

namespace flues::pm {

class PMSynthEngine {
public:
    explicit PMSynthEngine(float sampleRate = 44100.0f)
        : sampleRate(sampleRate),
          sources(sampleRate),
          envelope(sampleRate),
          interfaceModule(sampleRate),
          delayLines(sampleRate),
          filter(sampleRate),
          modulation(sampleRate),
          reverb(sampleRate),
          frequency(440.0f),
          gate(false),
          isPlaying(false),
          outputGain(0.5f),
          dcBlockerX1(0.0f),
          dcBlockerY1(0.0f),
          prevDelayOutputs{0.0f, 0.0f},
          prevFilterOutput(0.0f) {}

    void noteOn(float freq) {
        frequency = freq;
        gate = true;
        isPlaying = true;

        sources.reset();
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
        gate = false;
        envelope.setGate(false);
        interfaceModule.setGate(false);
    }

    float process() {
        if (!isPlaying) {
            return 0.0f;
        }

        const ModulationState modState = modulation.process();
        const float modulatedFreq = frequency * modState.fm;
        const float sourceSignal = sources.process(modulatedFreq);
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

        const auto finalDelayOutputs = delayLines.process(clampedDelayInput, frequency);
        const float delayMix = (finalDelayOutputs.delay1 + finalDelayOutputs.delay2) * 0.5f;
        const float filterOutput = filter.process(delayMix);
        const float preReverbOutput = filterOutput * modState.am * outputGain;
        const float output = reverb.process(preReverbOutput);

        prevDelayOutputs = finalDelayOutputs;
        prevFilterOutput = filterOutput;

        if (!envelope.isPlaying() &&
            std::abs(output) < 1e-5f &&
            std::abs(prevDelayOutputs.delay1) < 1e-5f &&
            std::abs(prevDelayOutputs.delay2) < 1e-5f) {
            isPlaying = false;
        }

        return output;
    }

    // Parameter setters
    void setDCLevel(float value) { sources.setDCLevel(value); }
    void setNoiseLevel(float value) { sources.setNoiseLevel(value); }
    void setToneLevel(float value) { sources.setToneLevel(value); }
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

    bool getIsPlaying() const { return isPlaying; }

private:
    float dcBlock(float sample) {
        const float y = sample - dcBlockerX1 + 0.995f * dcBlockerY1;
        dcBlockerX1 = sample;
        dcBlockerY1 = y;
        return y;
    }

    float sampleRate;
    SourcesModule sources;
    EnvelopeModule envelope;
    InterfaceModule interfaceModule;
    DelayLinesModule delayLines;
    FeedbackModule feedback;
    FilterModule filter;
    ModulationModule modulation;
    ReverbModule reverb;

    float frequency;
    bool gate;
    bool isPlaying;
    float outputGain;
    float dcBlockerX1;
    float dcBlockerY1;
    DelayLinesModule::DelayOutputs prevDelayOutputs;
    float prevFilterOutput;
};

} // namespace flues::pm
