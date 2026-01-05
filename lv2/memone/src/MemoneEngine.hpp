#pragma once

#include <array>
#include <cstdint>

#include "modules/LstmModule.hpp"
#include "modules/TempoSync.hpp"
#include "modules/DspUtils.hpp"

class MemoneEngine {
public:
    void prepare(double sampleRate) {
        sampleRate_ = sampleRate;
        tempo_.setSampleRate(sampleRate);
        reset();
    }

    void reset() {
        lstm_.reset();
        tempo_.reset();
        beatCounter_ = 0.0;
        warmupComplete_ = false;
    }

    void setManualBpm(float bpm) { tempo_.setManualBpm(bpm); }
    void setWarmupBeats(float beats) { warmupBeats_ = beats; }
    void setPredictGain(float gain) { predictGain_ = gain; }
    void setPredictionHorizon(float horizon) {
        const uint32_t clamped = static_cast<uint32_t>(clampf(horizon, 1.0f, kMaxPredictionHorizon));
        if (clamped != predictionHorizon_) {
            predictionHorizon_ = clamped;
        }
    }
    void setLearningRate(float rate) { learningRate_ = clampf(rate, 0.0f, 0.1f); }
    void setBpttLength(float length) { bpttLength_ = static_cast<uint32_t>(clampf(length, 1.0f, kMaxBptt)); }
    void setGradClip(float clip) { gradClip_ = clampf(clip, 0.1f, 20.0f); }
    void setHiddenSize(float size) { lstm_.setActiveSize(static_cast<uint32_t>(clampf(size, 1.0f, LstmModule::kHiddenSize))); }
    void setLearningRateClamp(bool enabled, float minRate, float maxRate) {
        lrClampEnabled_ = enabled;
        lrClampMin_ = clampf(minRate, 0.0f, 0.1f);
        lrClampMax_ = clampf(maxRate, 0.0f, 0.1f);
        if (lrClampMin_ > lrClampMax_) {
            const float tmp = lrClampMin_;
            lrClampMin_ = lrClampMax_;
            lrClampMax_ = tmp;
        }
    }
    void setTransport(float bpm, bool hasTempo, bool playing) {
        tempo_.updateHostTempo(bpm, hasTempo, playing);
    }

    void process(const float* input, float* output, uint32_t nframes) {
        if (!output) {
            return;
        }

        tempo_.advance(nframes);
        beatCounter_ = tempo_.beatCounter();
        warmupComplete_ = warmupBeats_ <= 0.0f || beatCounter_ >= warmupBeats_;

        for (uint32_t i = 0; i < nframes; ++i) {
            const float x = input ? input[i] : 0.0f;
            const float predicted = lstm_.process(x);

            float delayedPrediction = 0.0f;
            const bool hasDelayed = lstm_.getDelayedPrediction(predictionHorizon_, &delayedPrediction);
            if (hasDelayed) {
                const uint32_t bptt = predictionHorizon_ < bpttLength_ ? predictionHorizon_ : bpttLength_;
                const float lr = lrClampEnabled_ ? clampf(learningRate_, lrClampMin_, lrClampMax_) : learningRate_;
                lstm_.trainFromDelay(predictionHorizon_, x, bptt, lr, gradClip_);
            }

            if (!warmupComplete_) {
                output[i] = 0.0f;
            } else {
                output[i] = predicted * predictGain_;
            }
        }
    }

    bool isWarmupComplete() const { return warmupComplete_; }

private:
    static constexpr uint32_t kMaxPredictionHorizon = 256;
    static constexpr uint32_t kMaxBptt = 32;

    LstmModule lstm_{};
    TempoSync tempo_{};

    double sampleRate_ = 48000.0;
    float predictGain_ = 1.0f;
    float warmupBeats_ = 4.0f;
    double beatCounter_ = 0.0;
    bool warmupComplete_ = false;
    float learningRate_ = 0.0005f;
    uint32_t predictionHorizon_ = 64;
    uint32_t bpttLength_ = 32;
    float gradClip_ = 5.0f;
    bool lrClampEnabled_ = true;
    float lrClampMin_ = 0.0001f;
    float lrClampMax_ = 0.005f;
};
