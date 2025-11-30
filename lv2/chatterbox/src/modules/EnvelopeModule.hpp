// EnvelopeModule.hpp
// Attack/Release envelope for voice shaping
// Ported from experiments/chatterbox/src/audio/modules/EnvelopeModule.js

#ifndef ENVELOPE_MODULE_HPP
#define ENVELOPE_MODULE_HPP

#include <cmath>
#include <algorithm>

class EnvelopeAR {
private:
    float sampleRate;
    float minAttack;
    float maxAttack;
    float minRelease;
    float maxRelease;
    float attackSeconds;
    float releaseSeconds;
    float value;
    bool gate;
    bool active;

    /**
     * Exponential mapping from normalized [0,1] to [min, max]
     * @param val - Normalized value 0-1
     * @param min - Minimum output value
     * @param max - Maximum output value
     * @return Exponentially mapped value
     */
    static float map(float val, float min, float max) {
        const float clamped = std::clamp(val, 0.0f, 1.0f);
        return min * std::pow(max / min, clamped);
    }

public:
    EnvelopeAR(float sampleRate,
               float attackSeconds = 0.01f,
               float releaseSeconds = 0.05f,
               float minAttack = 0.001f,
               float maxAttack = 1.0f,
               float minRelease = 0.01f,
               float maxRelease = 3.0f)
        : sampleRate(sampleRate)
        , minAttack(minAttack)
        , maxAttack(maxAttack)
        , minRelease(minRelease)
        , maxRelease(maxRelease)
        , attackSeconds(attackSeconds)
        , releaseSeconds(releaseSeconds)
        , value(0.0f)
        , gate(false)
        , active(false)
    {
    }

    void setAttackNormalized(float val) {
        attackSeconds = map(val, minAttack, maxAttack);
    }

    void setReleaseNormalized(float val) {
        releaseSeconds = map(val, minRelease, maxRelease);
    }

    void setGate(bool on) {
        gate = on;
        if (gate) {
            active = true;
        }
    }

    void reset() {
        value = 0.0f;
        active = true;
    }

    float process() {
        if (gate) {
            const float attackRate = 1.0f / std::max(attackSeconds * sampleRate, 1.0f);
            value += attackRate;
            if (value > 1.0f) {
                value = 1.0f;
            }
        } else {
            const float releaseRate = 1.0f / std::max(releaseSeconds * sampleRate, 1.0f);
            value -= releaseRate;
            if (value <= 0.0f) {
                value = 0.0f;
                active = false;
            }
        }

        return value;
    }

    bool isActive() const {
        return active;
    }
};

class EnvelopeModule {
private:
    EnvelopeAR envelope;

public:
    EnvelopeModule(float sampleRate)
        : envelope(sampleRate)
    {
    }

    /**
     * Configure attack and release times
     * @param attack - Normalized attack time 0-1 (maps to 0.001-1.0s exponentially)
     * @param release - Normalized release time 0-1 (maps to 0.01-3.0s exponentially)
     */
    void configure(float attack, float release) {
        envelope.setAttackNormalized(attack);
        envelope.setReleaseNormalized(release);
    }

    /**
     * Set gate state (note on/off)
     * @param on - True for note-on, false for note-off
     */
    void gate(bool on) {
        envelope.setGate(on);
        if (on) {
            envelope.reset();
        }
    }

    /**
     * Process one sample
     * @return Envelope value 0-1
     */
    float process() {
        return envelope.process();
    }

    /**
     * Check if envelope is still active
     * @return True if envelope is generating signal
     */
    bool isActive() const {
        return envelope.isActive();
    }
};

#endif // ENVELOPE_MODULE_HPP
