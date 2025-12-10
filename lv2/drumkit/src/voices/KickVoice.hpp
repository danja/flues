// KickVoice.hpp
// Kick drum synthesis with pitch envelope, sine oscillator, distortion, and punch
// Parameters: Pitch (60-250Hz start), Decay (50-1500ms), Drive (1-10×), Punch (click amount)

#ifndef KICK_VOICE_HPP
#define KICK_VOICE_HPP

#include "../modules/PitchEnvelope.hpp"
#include "../modules/ADEnvelope.hpp"
#include "../modules/Distortion.hpp"
#include "../modules/DCBlocker.hpp"
#include "../modules/NoiseGenerator.hpp"
#include "../modules/BiquadFilter.hpp"
#include <cmath>
#include <algorithm>

namespace flues::drumkit {

class KickVoice {
private:
    static constexpr float TWO_PI = 6.28318530718f;

    float sampleRate;

    // Modules
    PitchEnvelope pitchEnv;
    ADEnvelope ampEnv;
    Distortion distortion;
    DCBlocker dcBlocker;
    NoiseGenerator noise;
    BiquadFilter clickFilter;  // HPF for punch

    // Oscillator state
    float phase;

    // Parameters
    float pitchStart;    // Starting pitch (Hz)
    float pitchEnd;      // Ending pitch (Hz)
    float decayTime;     // Envelope decay time (seconds)
    float driveAmount;   // Distortion drive
    float punchAmount;   // Click enhancement amount

    // Velocity
    float velocity;

    /**
     * Exponential parameter mapping
     */
    static float expoMap(float value, float min, float max) {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        return min * std::pow(max / min, clamped);
    }

public:
    explicit KickVoice(float sampleRate = 48000.0f)
        : sampleRate(sampleRate)
        , pitchEnv(sampleRate)
        , ampEnv(sampleRate)
        , distortion()
        , dcBlocker(0.999f)
        , noise(987654321u)
        , clickFilter(sampleRate, BiquadFilter::Type::Highpass)
        , phase(0.0f)
        , pitchStart(100.0f)
        , pitchEnd(40.0f)
        , decayTime(0.3f)
        , driveAmount(1.0f)
        , punchAmount(0.15f)
        , velocity(1.0f)
    {
        // Configure click filter (8kHz HPF for punch)
        clickFilter.setParameters(8000.0f, 0.707f);

        // Configure default envelopes
        ampEnv.setAttackTime(0.001f);  // 1ms attack
        ampEnv.setDecayTime(0.3f);     // 300ms decay

        pitchEnv.setParameters(100.0f, 40.0f, 0.05f);  // Quick pitch decay
    }

    /**
     * Set pitch parameter (normalized 0-1)
     * Maps to starting pitch 60-250 Hz
     */
    void setPitch(float value) {
        pitchStart = expoMap(value, 60.0f, 250.0f);
        pitchEnv.setStartFreq(pitchStart);
        pitchEnv.setEndFreq(pitchEnd);
    }

    /**
     * Set decay parameter (normalized 0-1)
     * Maps to decay time 50-1500ms
     */
    void setDecay(float value) {
        decayTime = expoMap(value, 0.05f, 1.5f);
        ampEnv.setDecayTime(decayTime);

        // Pitch envelope decays faster (20% of amp decay)
        pitchEnv.setDecayTime(decayTime * 0.2f);
    }

    /**
     * Set drive parameter (normalized 0-1)
     * Maps to distortion drive 1.0-10.0
     */
    void setDrive(float value) {
        driveAmount = 1.0f + value * 9.0f;  // Linear 1-10
        distortion.setDrive(driveAmount);
    }

    /**
     * Set punch parameter (normalized 0-1)
     * Controls amount of high-frequency click
     */
    void setPunch(float value) {
        punchAmount = std::clamp(value, 0.0f, 1.0f);
    }

    /**
     * Trigger the kick drum
     * @param vel - Velocity 0-1
     */
    void trigger(float vel = 1.0f) {
        velocity = std::clamp(vel, 0.0f, 1.0f);
        phase = 0.0f;

        pitchEnv.trigger();
        ampEnv.trigger();
        dcBlocker.reset();
    }

    /**
     * Process one sample
     */
    float process() {
        if (!ampEnv.isActive()) {
            return 0.0f;
        }

        // Get current pitch from envelope
        const float currentFreq = pitchEnv.process();

        // Generate sine wave with pitch modulation
        const float phaseIncrement = TWO_PI * currentFreq / sampleRate;
        phase += phaseIncrement;
        if (phase >= TWO_PI) {
            phase -= TWO_PI;
        }

        float sample = std::sin(phase);

        // Apply distortion
        sample = distortion.process(sample);

        // Add punch (high-frequency click burst)
        if (punchAmount > 0.01f && ampEnv.getValue() > 0.9f) {
            // Only add click during early attack
            const float clickSample = clickFilter.process(noise.process());
            sample += clickSample * punchAmount * 0.3f;
        }

        // Apply amplitude envelope
        const float env = ampEnv.process();
        sample *= env * velocity;

        // Remove DC offset
        sample = dcBlocker.process(sample);

        return sample * 0.8f;  // Output scaling
    }

    /**
     * Check if voice is active
     */
    bool isActive() const {
        return ampEnv.isActive();
    }

    /**
     * Force stop the voice
     */
    void reset() {
        ampEnv.reset();
        pitchEnv.reset();
        dcBlocker.reset();
        phase = 0.0f;
    }
};

} // namespace flues::drumkit

#endif // KICK_VOICE_HPP
