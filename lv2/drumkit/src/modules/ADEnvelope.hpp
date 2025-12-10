// ADEnvelope.hpp
// Attack/Decay envelope for one-shot drum sounds
// Automatically transitions from attack to decay phase

#ifndef AD_ENVELOPE_HPP
#define AD_ENVELOPE_HPP

#include <algorithm>
#include <cmath>

namespace flues::drumkit {

class ADEnvelope {
private:
    float sampleRate;
    float attackSamples;     // Attack time in samples
    float decaySamples;      // Decay time in samples
    float value;             // Current envelope value [0, 1]
    float sampleCounter;     // Current position in envelope
    bool active;             // Is envelope generating signal

    enum class Phase {
        Idle,
        Attack,
        Decay
    };
    Phase phase;

public:
    explicit ADEnvelope(float sampleRate = 48000.0f)
        : sampleRate(sampleRate)
        , attackSamples(48.0f)      // Default 1ms at 48kHz
        , decaySamples(4800.0f)     // Default 100ms at 48kHz
        , value(0.0f)
        , sampleCounter(0.0f)
        , active(false)
        , phase(Phase::Idle)
    {}

    /**
     * Set attack time in seconds
     */
    void setAttackTime(float seconds) {
        attackSamples = std::max(1.0f, seconds * sampleRate);
    }

    /**
     * Set decay time in seconds
     */
    void setDecayTime(float seconds) {
        decaySamples = std::max(1.0f, seconds * sampleRate);
    }

    /**
     * Set attack time from normalized parameter (exponential mapping)
     * Maps 0-1 to minTime-maxTime seconds
     */
    void setAttackNormalized(float value, float minTime = 0.0001f, float maxTime = 0.1f) {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        const float seconds = minTime * std::pow(maxTime / minTime, clamped);
        setAttackTime(seconds);
    }

    /**
     * Set decay time from normalized parameter (exponential mapping)
     */
    void setDecayNormalized(float value, float minTime = 0.01f, float maxTime = 3.0f) {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        const float seconds = minTime * std::pow(maxTime / minTime, clamped);
        setDecayTime(seconds);
    }

    /**
     * Trigger the envelope (start attack phase)
     */
    void trigger() {
        phase = Phase::Attack;
        sampleCounter = 0.0f;
        value = 0.0f;
        active = true;
    }

    /**
     * Reset envelope to idle state
     */
    void reset() {
        phase = Phase::Idle;
        value = 0.0f;
        sampleCounter = 0.0f;
        active = false;
    }

    /**
     * Process one sample and return envelope value
     */
    float process() {
        if (!active) {
            return 0.0f;
        }

        switch (phase) {
            case Phase::Attack:
                // Linear attack to 1.0
                value = sampleCounter / attackSamples;
                sampleCounter += 1.0f;

                if (sampleCounter >= attackSamples) {
                    // Transition to decay phase
                    phase = Phase::Decay;
                    sampleCounter = 0.0f;
                    value = 1.0f;
                }
                break;

            case Phase::Decay:
                // Linear decay to 0.0
                value = 1.0f - (sampleCounter / decaySamples);
                sampleCounter += 1.0f;

                if (value <= 0.0f || sampleCounter >= decaySamples) {
                    value = 0.0f;
                    phase = Phase::Idle;
                    active = false;
                }
                break;

            case Phase::Idle:
                value = 0.0f;
                active = false;
                break;
        }

        return std::clamp(value, 0.0f, 1.0f);
    }

    /**
     * Check if envelope is currently active
     */
    bool isActive() const {
        return active;
    }

    /**
     * Get current envelope value without processing
     */
    float getValue() const {
        return value;
    }
};

} // namespace flues::drumkit

#endif // AD_ENVELOPE_HPP
