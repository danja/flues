// ClockSync.hpp - BPM-based timing engine for phoneme advancement
// Calculates when to advance to next phoneme based on quarter note grid

#pragma once

#include <cstdint>
#include <cmath>

namespace chatgen {

class ClockSync {
public:
    ClockSync()
        : sampleRate(48000.0)
        , bpm(120.0f)
        , samplesPerBeat(0)
        , sampleCounter(0)
        , lastBeatPosition(0)
    {
        updateSamplesPerBeat();
    }

    // Initialize with sample rate
    void init(double sr) {
        sampleRate = sr;
        updateSamplesPerBeat();
    }

    // Update BPM and recalculate timing
    void setBPM(float newBpm) {
        if (newBpm != bpm && newBpm > 0.0f) {
            bpm = newBpm;
            updateSamplesPerBeat();
        }
    }

    // Reset the clock to start position
    void reset() {
        sampleCounter = 0;
        lastBeatPosition = 0;
    }

    // Process a block of samples and detect beat boundaries
    // Returns true if a beat boundary was crossed during this block
    bool process(uint32_t nframes, uint32_t& frameOffset) {
        // Calculate beat position before and after this block
        uint32_t positionBefore = sampleCounter / samplesPerBeat;
        sampleCounter += nframes;
        uint32_t positionAfter = sampleCounter / samplesPerBeat;

        // Check if we crossed a beat boundary
        if (positionAfter > positionBefore) {
            // Calculate exact frame where beat occurred
            // Find the sample position of the first beat in this block
            uint32_t nextBeatSample = (positionBefore + 1) * samplesPerBeat;
            uint32_t blockStartSample = sampleCounter - nframes;
            frameOffset = nextBeatSample - blockStartSample;

            lastBeatPosition = positionAfter;
            return true;
        }

        return false;
    }

    // Get current beat position (for debugging)
    uint32_t getBeatPosition() const {
        return sampleCounter / samplesPerBeat;
    }

    // Get current BPM
    float getBPM() const {
        return bpm;
    }

private:
    void updateSamplesPerBeat() {
        // Samples per quarter note = (sampleRate * 60) / BPM
        if (bpm > 0.0f) {
            samplesPerBeat = static_cast<uint32_t>((sampleRate * 60.0) / bpm);
        } else {
            samplesPerBeat = 24000;  // Default fallback (120 BPM at 48kHz)
        }
    }

    double sampleRate;
    float bpm;
    uint32_t samplesPerBeat;
    uint32_t sampleCounter;
    uint32_t lastBeatPosition;
};

} // namespace chatgen
