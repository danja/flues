#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>

#include "modules/FloozySourceModule.hpp"

#include "../../pm-synth/src/modules/DelayLinesModule.hpp"
#include "../../pm-synth/src/modules/EnvelopeModule.hpp"
#include "../../pm-synth/src/modules/FeedbackModule.hpp"
#include "../../pm-synth/src/modules/FilterModule.hpp"
#include "../../pm-synth/src/modules/InterfaceModule.hpp"
#include "../../pm-synth/src/modules/ModulationModule.hpp"
#include "../../pm-synth/src/modules/ReverbModule.hpp"

namespace flues::floozy_dev {

struct FloozyParams {
    float sourceAlgorithm = 3.0f;
    float sourceParam1 = 0.55f;
    float sourceParam2 = 0.50f;
    float sourceLevel = 0.70f;
    float sourceNoise = 0.10f;
    float sourceDC = 0.50f;

    float envelopeAttack = 0.33f;
    float envelopeRelease = 0.28f;

    float interfaceType = 2.0f;
    float interfaceIntensity = 0.50f;

    float tuning = 0.50f;
    float ratio = 0.50f;

    float delay1Feedback = 0.96f;
    float delay2Feedback = 0.96f;
    float filterFeedback = 0.0f;

    float filterFrequency = 0.57f;
    float filterQ = 0.18f;
    float filterShape = 0.0f;

    float lfoFrequency = 0.74f;
    float modulationTypeLevel = 0.50f;

    float reverbSize = 0.50f;
    float reverbLevel = 0.30f;
    float masterGain = 0.80f;

    uint64_t version = 1ULL;

    void bump() { ++version; }
};

class FloozyVoice {
public:
    explicit FloozyVoice(float sampleRate = 44100.0f)
        : source_(sampleRate),
          envelope_(sampleRate),
          interfaceModule_(sampleRate),
          delayLines_(sampleRate),
          feedback_(),
          filter_(sampleRate),
          modulation_(sampleRate),
          frequency_(440.0f),
          active_(false),
          releasing_(false),
          midiNote_(-1),
          dcBlockerX1_(0.0f),
          dcBlockerY1_(0.0f),
          prevDelayOutputs_{0.0f, 0.0f},
          prevFilterOutput_(0.0f),
          paramsVersion_(0),
          ageCounter_(0),
          lastOutput_(0.0f) {}

    void noteOn(int midiNote, float frequency, const FloozyParams& params, uint64_t age) {
        midiNote_ = midiNote;
        frequency_ = frequency;
        active_ = true;
        releasing_ = false;
        ageCounter_ = age;

        resetModules();
        envelope_.setGate(true);
        syncParams(params);
    }

    void noteOff() {
        if (!active_) {
            return;
        }
        releasing_ = true;
        envelope_.setGate(false);
    }

    void forceStop() {
        active_ = false;
        releasing_ = false;
        midiNote_ = -1;
        envelope_.reset();
        interfaceModule_.reset();
        delayLines_.reset();
        feedback_.reset();
        filter_.reset();
        modulation_.reset();
        source_.reset();
        dcBlockerX1_ = 0.0f;
        dcBlockerY1_ = 0.0f;
        prevDelayOutputs_ = {0.0f, 0.0f};
        prevFilterOutput_ = 0.0f;
        lastOutput_ = 0.0f;
    }

    float process(const FloozyParams& params) {
        if (!active_) {
            lastOutput_ = 0.0f;
            return 0.0f;
        }

        syncParams(params);

        const flues::pm::ModulationState modState = modulation_.process();
        const float modulatedFrequency = frequency_ * modState.fm;
        const float sourceSignal = source_.process(modulatedFrequency);
        const float env = envelope_.process();
        const float envelopedSignal = sourceSignal * env;

        const float feedbackSignal = feedback_.process(
            prevDelayOutputs_.delay1,
            prevDelayOutputs_.delay2,
            prevFilterOutput_);

        const float cleanFeedback = dcBlock(feedbackSignal);
        const float interfaceInput = envelopedSignal + cleanFeedback;
        const float interfaceOutput = interfaceModule_.process(interfaceInput);
        const float clampedDelayInput = std::clamp(interfaceOutput, -1.0f, 1.0f);

        const auto delayOutputs = delayLines_.process(clampedDelayInput, frequency_);
        const float delayMix = (delayOutputs.delay1 + delayOutputs.delay2) * 0.5f;
        const float filterOutput = filter_.process(delayMix);
        const float preReverb = filterOutput * modState.am * params.masterGain;

        prevDelayOutputs_ = delayOutputs;
        prevFilterOutput_ = filterOutput;

        lastOutput_ = preReverb;

        if (!envelope_.isPlaying() &&
            std::fabs(preReverb) < 1e-5f &&
            std::fabs(prevDelayOutputs_.delay1) < 1e-5f &&
            std::fabs(prevDelayOutputs_.delay2) < 1e-5f) {
            active_ = false;
            releasing_ = false;
            midiNote_ = -1;
        }

        return preReverb;
    }

    bool isActive() const { return active_; }
    bool isReleasing() const { return releasing_; }
    int note() const { return midiNote_; }
    uint64_t age() const { return ageCounter_; }
    float level() const { return std::fabs(lastOutput_); }

private:
    void syncParams(const FloozyParams& params) {
        if (paramsVersion_ == params.version) {
            return;
        }
        paramsVersion_ = params.version;

        source_.setAlgorithm(params.sourceAlgorithm);
        source_.setParam1(params.sourceParam1);
        source_.setParam2(params.sourceParam2);
        source_.setToneLevel(params.sourceLevel);
        source_.setNoiseLevel(params.sourceNoise);
        source_.setDCLevel(params.sourceDC);

        envelope_.setAttack(params.envelopeAttack);
        envelope_.setRelease(params.envelopeRelease);

        interfaceModule_.setType(static_cast<int>(std::round(params.interfaceType)));
        interfaceModule_.setIntensity(params.interfaceIntensity);

        delayLines_.setTuning(params.tuning);
        delayLines_.setRatio(params.ratio);

        feedback_.setDelay1Gain(params.delay1Feedback);
        feedback_.setDelay2Gain(params.delay2Feedback);
        feedback_.setFilterGain(params.filterFeedback);

        filter_.setFrequency(params.filterFrequency);
        filter_.setQ(params.filterQ);
        filter_.setShape(params.filterShape);

        modulation_.setFrequency(params.lfoFrequency);
        modulation_.setTypeLevel(params.modulationTypeLevel);
    }

    void resetModules() {
        source_.reset();
        envelope_.reset();
        interfaceModule_.reset();
        delayLines_.reset();
        feedback_.reset();
        filter_.reset();
        modulation_.reset();
        dcBlockerX1_ = 0.0f;
        dcBlockerY1_ = 0.0f;
        prevDelayOutputs_ = {0.0f, 0.0f};
        prevFilterOutput_ = 0.0f;
        lastOutput_ = 0.0f;
        paramsVersion_ = 0;
    }

    float dcBlock(float sample) {
        const float y = sample - dcBlockerX1_ + 0.995f * dcBlockerY1_;
        dcBlockerX1_ = sample;
        dcBlockerY1_ = y;
        return y;
    }

    FloozySourceModule source_;
    flues::pm::EnvelopeModule envelope_;
    flues::pm::InterfaceModule interfaceModule_;
    flues::pm::DelayLinesModule delayLines_;
    flues::pm::FeedbackModule feedback_;
    flues::pm::FilterModule filter_;
    flues::pm::ModulationModule modulation_;

    float frequency_;
    bool active_;
    bool releasing_;
    int midiNote_;
    float dcBlockerX1_;
    float dcBlockerY1_;
    flues::pm::DelayLinesModule::DelayOutputs prevDelayOutputs_;
    float prevFilterOutput_;
    uint64_t paramsVersion_;
    uint64_t ageCounter_;
    float lastOutput_;
};

class FloozyPolyEngine {
public:
    static constexpr size_t kMaxVoices = 8;

    explicit FloozyPolyEngine(float sampleRate = 44100.0f)
        : sampleRate_(sampleRate),
          reverb_(sampleRate),
          voiceAgeCounter_(0) {
        for (auto& voice : voices_) {
            voice = std::make_unique<FloozyVoice>(sampleRate_);
        }
        reverb_.setSize(params_.reverbSize);
        reverb_.setLevel(params_.reverbLevel);
    }

    void setAlgorithm(float value) { setAndBump(params_.sourceAlgorithm, std::clamp(value, 0.0f, 6.0f)); }
    void setParam1(float value) { setAndBump(params_.sourceParam1, std::clamp(value, 0.0f, 1.0f)); }
    void setParam2(float value) { setAndBump(params_.sourceParam2, std::clamp(value, 0.0f, 1.0f)); }
    void setToneLevel(float value) { setAndBump(params_.sourceLevel, std::clamp(value, 0.0f, 1.0f)); }
    void setNoiseLevel(float value) { setAndBump(params_.sourceNoise, std::clamp(value, 0.0f, 1.0f)); }
    void setDCLevel(float value) { setAndBump(params_.sourceDC, std::clamp(value, 0.0f, 1.0f)); }
    void setAttack(float value) { setAndBump(params_.envelopeAttack, std::clamp(value, 0.0f, 1.0f)); }
    void setRelease(float value) { setAndBump(params_.envelopeRelease, std::clamp(value, 0.0f, 1.0f)); }
    void setInterfaceType(float value) { setAndBump(params_.interfaceType, std::clamp(value, 0.0f, 11.0f)); }
    void setInterfaceIntensity(float value) { setAndBump(params_.interfaceIntensity, std::clamp(value, 0.0f, 1.0f)); }
    void setTuning(float value) { setAndBump(params_.tuning, std::clamp(value, 0.0f, 1.0f)); }
    void setRatio(float value) { setAndBump(params_.ratio, std::clamp(value, 0.0f, 1.0f)); }
    void setDelay1Feedback(float value) { setAndBump(params_.delay1Feedback, std::clamp(value, 0.0f, 1.0f)); }
    void setDelay2Feedback(float value) { setAndBump(params_.delay2Feedback, std::clamp(value, 0.0f, 1.0f)); }
    void setFilterFeedback(float value) { setAndBump(params_.filterFeedback, std::clamp(value, 0.0f, 1.0f)); }
    void setFilterFrequency(float value) { setAndBump(params_.filterFrequency, std::clamp(value, 0.0f, 1.0f)); }
    void setFilterQ(float value) { setAndBump(params_.filterQ, std::clamp(value, 0.0f, 1.0f)); }
    void setFilterShape(float value) { setAndBump(params_.filterShape, std::clamp(value, 0.0f, 1.0f)); }
    void setLFOFrequency(float value) { setAndBump(params_.lfoFrequency, std::clamp(value, 0.0f, 1.0f)); }
    void setModulationTypeLevel(float value) { setAndBump(params_.modulationTypeLevel, std::clamp(value, 0.0f, 1.0f)); }
    void setReverbSize(float value) {
        float clamped = std::clamp(value, 0.0f, 1.0f);
        if (params_.reverbSize != clamped) {
            params_.reverbSize = clamped;
            params_.bump();
            reverb_.setSize(clamped);
        }
    }
    void setReverbLevel(float value) {
        float clamped = std::clamp(value, 0.0f, 1.0f);
        if (params_.reverbLevel != clamped) {
            params_.reverbLevel = clamped;
            params_.bump();
            reverb_.setLevel(clamped);
        }
    }
    void setMasterGain(float value) { setAndBump(params_.masterGain, std::clamp(value, 0.0f, 1.0f)); }

    void noteOn(int midiNote, float frequency) {
        if (auto* existing = findVoiceByNote(midiNote)) {
            existing->noteOn(midiNote, frequency, params_, ++voiceAgeCounter_);
            return;
        }

        if (auto* idle = findIdleVoice()) {
            idle->noteOn(midiNote, frequency, params_, ++voiceAgeCounter_);
            return;
        }

        auto* victim = selectVoiceToSteal();
        if (victim) {
            victim->noteOn(midiNote, frequency, params_, ++voiceAgeCounter_);
        }
    }

    void noteOff(int midiNote) {
        if (auto* voice = findVoiceByNote(midiNote)) {
            voice->noteOff();
        }
    }

    void allNotesOff() {
        for (auto& voice : voices_) {
            voice->forceStop();
        }
        reverb_.reset();
    }

    float process() {
        float accum = 0.0f;
        for (auto& voice : voices_) {
            accum += voice->process(params_);
        }
        return reverb_.process(accum);
    }

private:
    void setAndBump(float& target, float value) {
        if (target == value) {
            return;
        }
        target = value;
        params_.bump();
    }

    FloozyVoice* findVoiceByNote(int midiNote) {
        for (auto& voice : voices_) {
            if (voice->isActive() && voice->note() == midiNote) {
                return voice.get();
            }
        }
        return nullptr;
    }

    FloozyVoice* findIdleVoice() {
        for (auto& voice : voices_) {
            if (!voice->isActive()) {
                return voice.get();
            }
        }
        return nullptr;
    }

    FloozyVoice* selectVoiceToSteal() {
        FloozyVoice* candidate = nullptr;
        uint64_t oldestAge = std::numeric_limits<uint64_t>::max();

        for (auto& voice : voices_) {
            if (voice->isReleasing() && voice->age() < oldestAge) {
                candidate = voice.get();
                oldestAge = voice->age();
            }
        }

        if (candidate) {
            return candidate;
        }

        float lowestLevel = std::numeric_limits<float>::max();
        for (auto& voice : voices_) {
            if (voice->level() < lowestLevel) {
                lowestLevel = voice->level();
                candidate = voice.get();
            }
        }
        return candidate;
    }

    float sampleRate_;
    FloozyParams params_;
    std::array<std::unique_ptr<FloozyVoice>, kMaxVoices> voices_;
    flues::pm::ReverbModule reverb_;
    uint64_t voiceAgeCounter_;
};

} // namespace flues::floozy_dev
