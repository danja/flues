// PitchEnvelope.hpp
// Exponential pitch envelope for kick and tom drums
// Creates characteristic pitch sweep from start frequency to end frequency

#ifndef PITCH_ENVELOPE_HPP
#define PITCH_ENVELOPE_HPP

#include <algorithm>
#include <cmath>

namespace flues::drumkit {

class PitchEnvelope {
private:
    float sampleRate;
    float startFreq;         // Starting frequency (Hz)
    float endFreq;           // Ending frequency (Hz)
    float decayTime;         // Decay time in seconds
    float currentFreq;       // Current frequency output
    float sampleCounter;     // Current position in envelope
    bool active;

public:
    explicit PitchEnvelope(float sampleRate = 48000.0f)
        : sampleRate(sampleRate)
        , startFreq(150.0f)
        , endFreq(40.0f)
        , decayTime(0.2f)
        , currentFreq(40.0f)
        , sampleCounter(0.0f)
        , active(false)
    {}

    /**
     * Set the pitch envelope parameters
     * @param start - Starting frequency in Hz
     * @param end - Ending frequency in Hz
     * @param decay - Decay time in seconds
     */
    void setParameters(float start, float end, float decay) {
        startFreq = std::max(10.0f, start);
        endFreq = std::max(10.0f, end);
        decayTime = std::max(0.001f, decay);
    }

    /**
     * Set start frequency
     */
    void setStartFreq(float freq) {
        startFreq = std::max(10.0f, freq);
    }

    /**
     * Set end frequency
     */
    void setEndFreq(float freq) {
        endFreq = std::max(10.0f, freq);
    }

    /**
     * Set decay time in seconds
     */
    void setDecayTime(float seconds) {
        decayTime = std::max(0.001f, seconds);
    }

    /**
     * Trigger the pitch envelope
     */
    void trigger() {
        sampleCounter = 0.0f;
        currentFreq = startFreq;
        active = true;
    }

    /**
     * Reset to idle state
     */
    void reset() {
        sampleCounter = 0.0f;
        currentFreq = endFreq;
        active = false;
    }

    /**
     * Process one sample and return current frequency
     * Uses exponential decay from startFreq to endFreq
     */
    float process() {
        if (!active) {
            return endFreq;
        }

        const float decaySamples = decayTime * sampleRate;
        const float t = sampleCounter / decaySamples;  // Normalized time [0, 1]

        if (t >= 1.0f) {
            // Envelope finished, stay at end frequency
            currentFreq = endFreq;
            active = false;
        } else {
            // Exponential decay from startFreq to endFreq
            // freq(t) = endFreq * (startFreq/endFreq)^(1-t)
            const float ratio = startFreq / endFreq;
            currentFreq = endFreq * std::pow(ratio, 1.0f - t);
        }

        sampleCounter += 1.0f;
        return currentFreq;
    }

    /**
     * Check if envelope is currently active
     */
    bool isActive() const {
        return active;
    }

    /**
     * Get current frequency without processing
     */
    float getCurrentFreq() const {
        return currentFreq;
    }
};

} // namespace flues::drumkit

#endif // PITCH_ENVELOPE_HPP
