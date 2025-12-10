// SnareVoice.hpp
// Snare drum synthesis with dual resonators and noise burst
// Parameters: Tone (body/shell mix + Q), Snap (noise level + HPF cutoff)

#ifndef SNARE_VOICE_HPP
#define SNARE_VOICE_HPP

#include "../modules/BiquadFilter.hpp"
#include "../modules/NoiseGenerator.hpp"
#include "../modules/ADEnvelope.hpp"
#include <algorithm>
#include <cmath>

namespace flues::drumkit {

class SnareVoice {
private:
    float sampleRate;

    // Modules
    BiquadFilter bodyResonator;    // 180 Hz resonator
    BiquadFilter shellResonator;   // 330 Hz resonator
    BiquadFilter noiseFilter;      // HPF for snap
    NoiseGenerator noise;
    ADEnvelope ampEnv;             // Main amplitude envelope
    ADEnvelope noiseEnv;           // Noise burst envelope

    // Parameters
    float toneParam;       // Body/shell mix and Q
    float snapParam;       // Noise level and filter frequency
    float velocity;

    /**
     * Exponential parameter mapping
     */
    static float expoMap(float value, float min, float max) {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        return min * std::pow(max / min, clamped);
    }

public:
    explicit SnareVoice(float sampleRate = 48000.0f)
        : sampleRate(sampleRate)
        , bodyResonator(sampleRate, BiquadFilter::Type::Bandpass)
        , shellResonator(sampleRate, BiquadFilter::Type::Bandpass)
        , noiseFilter(sampleRate, BiquadFilter::Type::Highpass)
        , noise(111222333u)
        , ampEnv(sampleRate)
        , noiseEnv(sampleRate)
        , toneParam(0.5f)
        , snapParam(0.6f)
        , velocity(1.0f)
    {
        // Configure resonators (fixed frequencies)
        bodyResonator.setParameters(180.0f, 10.0f);   // Body: 180 Hz, Q=10
        shellResonator.setParameters(330.0f, 8.0f);   // Shell: 330 Hz, Q=8

        // Configure noise filter
        noiseFilter.setParameters(2000.0f, 0.707f);

        // Configure envelopes
        ampEnv.setAttackTime(0.001f);     // 1ms attack
        ampEnv.setDecayTime(0.15f);       // 150ms decay

        noiseEnv.setAttackTime(0.003f);   // 3ms attack
        noiseEnv.setDecayTime(0.15f);     // 150ms decay
    }

    /**
     * Set tone parameter (normalized 0-1)
     * Controls body/shell mix and resonator Q
     * 0 = more body (low), 1 = more shell (high)
     */
    void setTone(float value) {
        toneParam = std::clamp(value, 0.0f, 1.0f);

        // Q increases with tone (4-20 range for metallic character)
        const float Q = 4.0f + toneParam * 16.0f;
        bodyResonator.setQ(Q);
        shellResonator.setQ(Q);
    }

    /**
     * Set snap parameter (normalized 0-1)
     * Controls noise level and HPF cutoff
     */
    void setSnap(float value) {
        snapParam = std::clamp(value, 0.0f, 1.0f);

        // HPF cutoff: 500Hz - 4kHz (exponential)
        const float cutoff = expoMap(snapParam, 500.0f, 4000.0f);
        noiseFilter.setFrequency(cutoff);
    }

    /**
     * Trigger the snare
     * @param vel - Velocity 0-1
     */
    void trigger(float vel = 1.0f) {
        velocity = std::clamp(vel, 0.0f, 1.0f);

        ampEnv.trigger();
        noiseEnv.trigger();

        bodyResonator.reset();
        shellResonator.reset();
        noiseFilter.reset();
    }

    /**
     * Process one sample
     */
    float process() {
        if (!ampEnv.isActive()) {
            return 0.0f;
        }

        // Generate excitation noise burst
        const float noiseSample = noise.process();

        // Process through resonators
        const float bodyOut = bodyResonator.process(noiseSample);
        const float shellOut = shellResonator.process(noiseSample);

        // Mix resonators based on tone parameter
        const float bodyMix = 1.0f - toneParam;
        const float shellMix = toneParam;
        float tonal = bodyOut * bodyMix + shellOut * shellMix;

        // Tonal part with main envelope
        tonal *= ampEnv.process();

        // Add filtered noise burst with separate envelope
        const float filteredNoise = noiseFilter.process(noiseSample);
        const float noiseOut = filteredNoise * noiseEnv.process() * snapParam;

        // Combine tonal and noise components
        float sample = tonal * 0.7f + noiseOut * 0.5f;

        // Apply velocity
        sample *= velocity;

        return sample * 0.6f;  // Output scaling
    }

    /**
     * Check if voice is active
     */
    bool isActive() const {
        return ampEnv.isActive() || noiseEnv.isActive();
    }

    /**
     * Force stop the voice
     */
    void reset() {
        ampEnv.reset();
        noiseEnv.reset();
        bodyResonator.reset();
        shellResonator.reset();
        noiseFilter.reset();
    }
};

} // namespace flues::drumkit

#endif // SNARE_VOICE_HPP
