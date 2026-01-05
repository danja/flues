#pragma once

#include <cstdint>

class TempoSync {
public:
    void setSampleRate(double sampleRate) {
        sampleRate_ = sampleRate;
    }

    void setManualBpm(float bpm) {
        manualBpm_ = bpm;
    }

    void updateHostTempo(float bpm, bool hasTempo, bool playing) {
        hostBpm_ = bpm;
        hasHostTempo_ = hasTempo;
        transportPlaying_ = playing;
    }

    void reset() {
        beatCounter_ = 0.0;
    }

    void advance(uint32_t frames) {
        const float bpm = currentBpm();
        if (bpm <= 0.0f || sampleRate_ <= 0.0) {
            return;
        }
        const double samplesPerBeat = sampleRate_ * 60.0 / bpm;
        if (samplesPerBeat <= 0.0) {
            return;
        }
        if (transportPlaying_ || !hasHostTempo_) {
            beatCounter_ += static_cast<double>(frames) / samplesPerBeat;
        }
    }

    double beatCounter() const { return beatCounter_; }
    float currentBpm() const {
        if (hasHostTempo_ && hostBpm_ > 0.0f) {
            return hostBpm_;
        }
        return manualBpm_ > 0.0f ? manualBpm_ : 120.0f;
    }

    bool hasHostTempo() const { return hasHostTempo_; }
    bool transportPlaying() const { return transportPlaying_; }

private:
    double sampleRate_ = 48000.0;
    float manualBpm_ = 120.0f;
    float hostBpm_ = 0.0f;
    bool hasHostTempo_ = false;
    bool transportPlaying_ = true;
    double beatCounter_ = 0.0;
};
