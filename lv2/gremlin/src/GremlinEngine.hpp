#ifndef FLUES_GREMLIN_ENGINE_HPP
#define FLUES_GREMLIN_ENGINE_HPP

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace flues::gremlin {

struct StereoFrame {
    float left;
    float right;
};

static constexpr float kPi = 3.14159265358979323846f;
static constexpr int kDelayBufferSize = 131072;

class OnePoleLowPass {
public:
    void setCutoffHz(float sampleRate, float cutoffHz) {
        const float clamped = std::clamp(cutoffHz, 20.0f, sampleRate * 0.45f);
        const float pole = std::exp(-2.0f * kPi * clamped / sampleRate);
        a0_ = 1.0f - pole;
        b1_ = pole;
    }

    float process(float x) {
        z1_ = a0_ * x + b1_ * z1_;
        return z1_;
    }

    void reset(float value = 0.0f) { z1_ = value; }

private:
    float a0_ = 1.0f;
    float b1_ = 0.0f;
    float z1_ = 0.0f;
};

class DCBlocker {
public:
    float process(float x) {
        const float y = x - x1_ + 0.9985f * y1_;
        x1_ = x;
        y1_ = y;
        return y;
    }

    void reset() {
        x1_ = 0.0f;
        y1_ = 0.0f;
    }

private:
    float x1_ = 0.0f;
    float y1_ = 0.0f;
};

class GremlinEngine {
public:
    enum Mode : int {
        SHARD = 0,
        SERVO = 1,
        SPRAY = 2,
        COLLAPSE = 3
    };

    explicit GremlinEngine(float sampleRate)
        : sampleRate_(sampleRate)
    {
        updateAttack();
        updateRelease();
        updateTone();
        updateDamping();
    }

    void setMode(float value) {
        mode_ = std::clamp(static_cast<int>(std::lround(value)), 0, 3);
    }

    void setDamage(float value) { damage_ = std::clamp(value, 0.0f, 1.0f); }
    void setChaos(float value) { chaos_ = std::clamp(value, 0.0f, 1.0f); }
    void setNoise(float value) { noise_ = std::clamp(value, 0.0f, 1.0f); }
    void setDrift(float value) { drift_ = std::clamp(value, 0.0f, 1.0f); }
    void setCrunch(float value) { crunch_ = std::clamp(value, 0.0f, 1.0f); }
    void setFold(float value) { fold_ = std::clamp(value, 0.0f, 1.0f); }
    void setDelayTime(float value) { delayTime_ = std::clamp(value, 0.0f, 1.0f); }
    void setFeedback(float value) { feedback_ = std::clamp(value, 0.0f, 1.0f); }
    void setWarp(float value) { warp_ = std::clamp(value, 0.0f, 1.0f); }
    void setStutter(float value) { stutter_ = std::clamp(value, 0.0f, 1.0f); }

    void setTone(float value) {
        tone_ = std::clamp(value, 0.0f, 1.0f);
        updateTone();
    }

    void setDamping(float value) {
        damping_ = std::clamp(value, 0.0f, 1.0f);
        updateDamping();
    }

    void setSpace(float value) { space_ = std::clamp(value, 0.0f, 1.0f); }

    void setAttack(float value) {
        attack_ = std::clamp(value, 0.0f, 1.0f);
        updateAttack();
    }

    void setRelease(float value) {
        release_ = std::clamp(value, 0.0f, 1.0f);
        updateRelease();
    }

    void setOutput(float value) { output_ = std::clamp(value, 0.0f, 1.0f); }

    void noteOn(uint8_t midiNote, float velocity) {
        currentMidiNote_ = midiNote;
        gate_ = true;
        velocity_ = std::clamp(velocity, 0.0f, 1.0f);
        baseFrequency_ = midiToHz(midiNote);
        transient_ = 1.0f;
    }

    void noteOff(uint8_t midiNote) {
        if (currentMidiNote_ == midiNote) {
            gate_ = false;
        }
    }

    void allNotesOff() {
        gate_ = false;
    }

    StereoFrame process() {
        const float target = gate_ ? 1.0f : 0.0f;
        env_ += (target - env_) * (gate_ ? attackStep_ : releaseStep_);
        transient_ *= 0.9955f - damage_ * 0.024f;

        if (env_ < 1.0e-5f && !gate_ && std::fabs(lastLeft_) < 1.0e-5f && std::fabs(lastRight_) < 1.0e-5f) {
            return {0.0f, 0.0f};
        }

        updateChaosState();
        updateSampleHold();

        const float freqA = std::clamp(baseFrequency_ * pitchModSemitone(0.85f), 18.0f, sampleRate_ * 0.45f);
        const float freqB = std::clamp(baseFrequency_ * (1.35f + 0.45f * chaos_ + 0.22f * damage_)
                                       * pitchRatio(0.25f * slowChaos_ + 0.15f * fastChaos_), 18.0f, sampleRate_ * 0.45f);
        const float freqC = std::clamp(baseFrequency_ * (2.0f + 0.9f * damage_ + 0.35f * chaosAbs_)
                                       * pitchRatio(0.12f * tentChaos_), 18.0f, sampleRate_ * 0.45f);

        phaseA_ = wrapPhase(phaseA_ + freqA / sampleRate_);
        phaseB_ = wrapPhase(phaseB_ + freqB / sampleRate_);
        phaseC_ = wrapPhase(phaseC_ + freqC / sampleRate_);

        const float sawA = 2.0f * phaseA_ - 1.0f;
        const float sawB = 2.0f * phaseB_ - 1.0f;
        const float triC = 1.0f - 4.0f * std::fabs(phaseC_ - 0.5f);
        const float squareA = phaseA_ < (0.5f + 0.12f * fastChaos_) ? 1.0f : -1.0f;
        const float squareB = phaseB_ < (0.48f - 0.10f * tentChaos_) ? 1.0f : -1.0f;
        const float sineA = std::sin(2.0f * kPi * (phaseA_ + 0.07f * fastChaos_ * chaos_));
        const float comparator = sawA > sawB ? 1.0f : -1.0f;
        const float chaosAudio = std::tanh((henonChaos_ * 0.9f + fastChaos_ * 0.7f + tentChaos_ * 0.35f) * (1.4f + 2.0f * chaos_));

        float source = 0.0f;
        switch (mode_) {
            case SHARD:
                source = 0.50f * sawA + 0.28f * comparator + 0.18f * (squareA * squareB)
                       + 0.20f * transient_ + 0.12f * chaosAudio;
                break;
            case SERVO:
                source = 0.72f * std::sin(2.0f * kPi * (phaseA_ + chaosAudio * (0.18f + 0.32f * chaos_)))
                       + 0.30f * (sawB * triC)
                       + 0.12f * heldNoise_;
                break;
            case SPRAY:
                source = 0.42f * heldNoise_ + 0.26f * (squareA * sawB) + 0.22f * comparator
                       + 0.34f * transient_ * (0.5f + 0.5f * chaosAbs_);
                break;
            case COLLAPSE:
            default:
                source = 0.34f * chaosAudio + 0.20f * sawA + 0.22f * sineA + 0.18f * comparator
                       + 0.20f * heldNoise_ + 0.14f * transient_;
                break;
        }

        source += whiteNoise() * noise_ * (0.08f + 0.22f * chaosAbs_ + 0.16f * transient_);
        source *= (0.24f + velocity_ * 0.9f) * env_;

        const float crushed = processCrunch(source);
        const float driven = crushed * (1.0f + damage_ * 3.5f + fold_ * 4.5f);
        const float folded = foldback(driven, std::max(0.14f, 1.0f - fold_ * 0.8f));
        const float shaped = std::tanh(folded * (1.0f + damage_ * 1.7f));
        const float toned = toneFilter_.process(shaped);

        const StereoFrame delayed = processDelay(toned);
        const float outGain = 0.08f + output_ * 1.65f;
        const float left = dcLeft_.process(delayed.left * outGain);
        const float right = dcRight_.process(delayed.right * outGain);

        lastLeft_ = left;
        lastRight_ = right;
        return {left, right};
    }

private:
    static float wrapPhase(float phase) {
        if (phase >= 1.0f) {
            phase -= std::floor(phase);
        }
        return phase;
    }

    static float mixf(float a, float b, float t) {
        return a + (b - a) * t;
    }

    static float clampDelay(float value) {
        return std::clamp(value, 1.0f, static_cast<float>(kDelayBufferSize - 4));
    }

    static float pitchRatio(float semitoneOffset) {
        return std::pow(2.0f, semitoneOffset / 12.0f);
    }

    static float midiToHz(uint8_t midiNote) {
        return 440.0f * std::pow(2.0f, (static_cast<int>(midiNote) - 69) / 12.0f);
    }

    float pitchModSemitone(float scale) const {
        const float semitone = drift_ * slowChaos_ * 7.0f + chaos_ * fastChaos_ * 11.0f * scale;
        return pitchRatio(semitone);
    }

    float whiteNoise() {
        rngState_ = rngState_ * 1664525u + 1013904223u;
        return static_cast<float>((rngState_ >> 8) & 0x00FFFFFFu) * (1.0f / 8388607.5f) - 1.0f;
    }

    void updateChaosState() {
        const float logisticR = 3.55f + chaos_ * 0.43f + damage_ * 0.015f;
        logistic_ = std::clamp(logisticR * logistic_ * (1.0f - logistic_), 0.0001f, 0.9999f);

        const float tentMu = 1.60f + chaos_ * 0.38f;
        tent_ = tent_ < 0.5f ? tent_ * tentMu : (1.0f - tent_) * tentMu;
        tent_ = tent_ - std::floor(tent_);

        const float a = 1.16f + chaos_ * 0.22f + damage_ * 0.08f;
        const float b = 0.18f + 0.10f * space_;
        const float nextHenonX = 1.0f - a * henonX_ * henonX_ + henonY_;
        const float nextHenonY = b * henonX_;
        henonX_ = std::clamp(nextHenonX, -1.5f, 1.5f);
        henonY_ = std::clamp(nextHenonY, -0.5f, 0.5f);

        fastChaos_ = std::clamp(0.55f * (logistic_ * 2.0f - 1.0f)
                              + 0.30f * (tent_ * 2.0f - 1.0f)
                              + 0.22f * (henonX_ * 0.7f), -1.0f, 1.0f);
        tentChaos_ = std::clamp(tent_ * 2.0f - 1.0f, -1.0f, 1.0f);
        henonChaos_ = std::clamp(henonX_ * 0.9f + henonY_ * 0.7f, -1.0f, 1.0f);
        chaosAbs_ = std::clamp(std::fabs(fastChaos_) * 0.65f + std::fabs(henonChaos_) * 0.35f, 0.0f, 1.0f);
        slowChaos_ += (fastChaos_ - slowChaos_) * (0.0009f + drift_ * 0.0024f);
    }

    void updateSampleHold() {
        if (--noiseHoldCounter_ <= 0) {
            const float interval = 1.0f + (1.0f - crunch_) * 18.0f + chaos_ * 36.0f;
            noiseHoldCounter_ = static_cast<int>(interval);
            heldNoise_ = whiteNoise();
        }
    }

    float processCrunch(float input) {
        if (--decimationCounter_ <= 0) {
            decimationCounter_ = 1 + static_cast<int>(crunch_ * 48.0f + damage_ * 28.0f);
            crushedSample_ = input;
        }

        const int bits = std::clamp(15 - static_cast<int>(crunch_ * 10.0f + damage_ * 3.0f), 3, 16);
        const float scale = static_cast<float>((1u << bits) - 1u);
        return std::round(crushedSample_ * scale) / scale;
    }

    static float foldback(float input, float threshold) {
        const float t = std::max(0.001f, threshold);
        float x = input;
        if (x > t || x < -t) {
            x = std::fabs(std::fmod(x - t, t * 4.0f));
            x = std::fabs(x - t * 2.0f) - t;
        }
        return x;
    }

    float readDelay(const std::array<float, kDelayBufferSize>& buffer, float delaySamples) const {
        const float readPos = static_cast<float>(writePos_) - delaySamples;
        const int i0 = wrapIndex(static_cast<int>(std::floor(readPos)));
        const int i1 = wrapIndex(i0 + 1);
        const float frac = readPos - std::floor(readPos);
        return buffer[static_cast<size_t>(i0)] + (buffer[static_cast<size_t>(i1)] - buffer[static_cast<size_t>(i0)]) * frac;
    }

    int wrapIndex(int index) const {
        int wrapped = index % kDelayBufferSize;
        if (wrapped < 0) {
            wrapped += kDelayBufferSize;
        }
        return wrapped;
    }

    void maybeTriggerGlitch(float baseDelaySamples) {
        if (glitchHold_ > 0) {
            --glitchHold_;
            return;
        }

        if (stutter_ <= 0.0001f || env_ <= 0.03f) {
            glitchBlend_ = 0.0f;
            return;
        }

        const float chance = (0.00001f + stutter_ * 0.00018f) * (0.55f + chaos_ * 0.8f + damage_ * 0.4f);
        if ((whiteNoise() * 0.5f + 0.5f) < chance) {
            const float maxShortDelay = std::max(24.0f, std::min(baseDelaySamples * (0.28f + 0.46f * chaosAbs_), sampleRate_ * 0.12f));
            glitchDelaySamples_ = 12.0f + (whiteNoise() * 0.5f + 0.5f) * (maxShortDelay - 12.0f);
            glitchBlend_ = 0.4f + 0.5f * stutter_;
            glitchHold_ = 96 + static_cast<int>((0.005f + 0.04f * stutter_ + 0.02f * chaos_) * sampleRate_ * (0.7f + 0.3f * chaosAbs_));
        } else {
            glitchBlend_ = 0.0f;
        }
    }

    StereoFrame processDelay(float source) {
        const float baseDelaySamples = 12.0f + std::pow(delayTime_, 1.8f) * (sampleRate_ * 0.65f);
        maybeTriggerGlitch(baseDelaySamples);

        const float warpSamples = warp_ * (18.0f + baseDelaySamples * 0.22f) * (0.75f * fastChaos_ + 0.35f * henonChaos_);
        const float spaceSamples = space_ * (10.0f + baseDelaySamples * 0.14f);

        float leftDelay = clampDelay(baseDelaySamples + warpSamples + spaceSamples);
        float rightDelay = clampDelay(baseDelaySamples - warpSamples - spaceSamples);

        if (glitchBlend_ > 0.0f) {
            leftDelay = clampDelay(mixf(leftDelay, glitchDelaySamples_ * (1.0f + 0.08f * fastChaos_), glitchBlend_));
            rightDelay = clampDelay(mixf(rightDelay, glitchDelaySamples_ * (1.0f - 0.08f * tentChaos_), glitchBlend_));
        }

        const float delayedLeft = readDelay(delayLeft_, leftDelay);
        const float delayedRight = readDelay(delayRight_, rightDelay);

        const float filteredLeft = feedbackFilterLeft_.process(delayedLeft);
        const float filteredRight = feedbackFilterRight_.process(delayedRight);

        const float feedbackGain = std::min(0.985f, 0.06f + std::pow(feedback_, 1.2f) * 0.91f + damage_ * 0.03f);
        const float crossAmount = space_ * (0.08f + 0.16f * chaos_);
        const float collapsePolarity = (mode_ == COLLAPSE && fastChaos_ < 0.0f) ? -0.82f : 1.0f;

        const float writeLeft = std::tanh(source + (filteredLeft + filteredRight * crossAmount) * feedbackGain);
        const float writeRight = std::tanh(source + (filteredRight * collapsePolarity + filteredLeft * crossAmount) * feedbackGain);

        delayLeft_[static_cast<size_t>(writePos_)] = writeLeft;
        delayRight_[static_cast<size_t>(writePos_)] = writeRight;
        writePos_ = (writePos_ + 1) % kDelayBufferSize;

        const float wet = 0.28f + feedback_ * 0.40f + warp_ * 0.18f + stutter_ * 0.14f;
        const float dry = 0.90f - feedback_ * 0.24f;
        float left = source * dry + delayedLeft * wet;
        float right = source * dry + delayedRight * wet;

        const float mid = 0.5f * (left + right);
        const float side = (left - right) * 0.5f * (0.55f + space_ * 1.2f);
        left = std::tanh((mid + side) * (1.0f + damage_ * 0.5f));
        right = std::tanh((mid - side) * (1.0f + damage_ * 0.5f));
        return {left, right};
    }

    void updateAttack() {
        const float attackSeconds = 0.001f + std::pow(attack_, 2.0f) * 0.35f;
        attackStep_ = 1.0f - std::exp(-1.0f / (attackSeconds * sampleRate_));
    }

    void updateRelease() {
        const float releaseSeconds = 0.01f + std::pow(release_, 2.0f) * 2.7f;
        releaseStep_ = 1.0f - std::exp(-1.0f / (releaseSeconds * sampleRate_));
    }

    void updateTone() {
        const float cutoff = 180.0f + std::pow(tone_, 1.7f) * 16000.0f;
        toneFilter_.setCutoffHz(sampleRate_, cutoff);
    }

    void updateDamping() {
        const float cutoff = 300.0f + std::pow(1.0f - damping_, 1.6f) * 14000.0f;
        feedbackFilterLeft_.setCutoffHz(sampleRate_, cutoff);
        feedbackFilterRight_.setCutoffHz(sampleRate_, cutoff);
    }

    float sampleRate_ = 48000.0f;
    int mode_ = SHARD;

    float damage_ = 0.55f;
    float chaos_ = 0.60f;
    float noise_ = 0.30f;
    float drift_ = 0.35f;
    float crunch_ = 0.45f;
    float fold_ = 0.35f;
    float delayTime_ = 0.32f;
    float feedback_ = 0.55f;
    float warp_ = 0.45f;
    float stutter_ = 0.35f;
    float tone_ = 0.65f;
    float damping_ = 0.45f;
    float space_ = 0.55f;
    float attack_ = 0.05f;
    float release_ = 0.25f;
    float output_ = 0.70f;

    bool gate_ = false;
    uint8_t currentMidiNote_ = 0;
    float velocity_ = 0.0f;
    float baseFrequency_ = 110.0f;
    float env_ = 0.0f;
    float transient_ = 0.0f;
    float attackStep_ = 0.01f;
    float releaseStep_ = 0.001f;

    float phaseA_ = 0.0f;
    float phaseB_ = 0.0f;
    float phaseC_ = 0.0f;

    uint32_t rngState_ = 0x73419a5du;
    float logistic_ = 0.63f;
    float tent_ = 0.37f;
    float henonX_ = 0.11f;
    float henonY_ = 0.09f;
    float fastChaos_ = 0.0f;
    float slowChaos_ = 0.0f;
    float tentChaos_ = 0.0f;
    float henonChaos_ = 0.0f;
    float chaosAbs_ = 0.0f;

    int noiseHoldCounter_ = 1;
    float heldNoise_ = 0.0f;
    int decimationCounter_ = 1;
    float crushedSample_ = 0.0f;

    std::array<float, kDelayBufferSize> delayLeft_ {};
    std::array<float, kDelayBufferSize> delayRight_ {};
    int writePos_ = 0;
    int glitchHold_ = 0;
    float glitchDelaySamples_ = 48.0f;
    float glitchBlend_ = 0.0f;

    OnePoleLowPass toneFilter_;
    OnePoleLowPass feedbackFilterLeft_;
    OnePoleLowPass feedbackFilterRight_;
    DCBlocker dcLeft_;
    DCBlocker dcRight_;

    float lastLeft_ = 0.0f;
    float lastRight_ = 0.0f;
};

} // namespace flues::gremlin

#endif
