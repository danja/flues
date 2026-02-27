#ifndef FLUES_BUBBLES_ENGINE_HPP
#define FLUES_BUBBLES_ENGINE_HPP

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace flues::bubbles {

struct StereoFrame {
    float left;
    float right;
};

class OnePole {
public:
    void setCutoffHz(float sampleRate, float cutoffHz) {
        const float clamped = std::clamp(cutoffHz, 10.0f, 18000.0f);
        const float x = std::exp(-2.0f * static_cast<float>(M_PI) * clamped / sampleRate);
        a0_ = 1.0f - x;
        b1_ = x;
    }

    float process(float x) {
        z1_ = a0_ * x + b1_ * z1_;
        return z1_;
    }

private:
    float a0_ = 1.0f;
    float b1_ = 0.0f;
    float z1_ = 0.0f;
};

class BiquadBandPass {
public:
    void set(float sampleRate, float freqHz, float q) {
        const float f = std::clamp(freqHz, 20.0f, sampleRate * 0.45f);
        const float qq = std::clamp(q, 0.2f, 30.0f);
        const float w0 = 2.0f * static_cast<float>(M_PI) * f / sampleRate;
        const float alpha = std::sin(w0) / (2.0f * qq);
        const float c = std::cos(w0);

        const float b0 = alpha;
        const float b1 = 0.0f;
        const float b2 = -alpha;
        const float a0 = 1.0f + alpha;
        const float a1 = -2.0f * c;
        const float a2 = 1.0f - alpha;

        b0_ = b0 / a0;
        b1_ = b1 / a0;
        b2_ = b2 / a0;
        a1_ = a1 / a0;
        a2_ = a2 / a0;
    }

    float process(float x) {
        const float y = b0_ * x + z1_;
        z1_ = b1_ * x - a1_ * y + z2_;
        z2_ = b2_ * x - a2_ * y;
        return y;
    }

private:
    float b0_ = 0.0f;
    float b1_ = 0.0f;
    float b2_ = 0.0f;
    float a1_ = 0.0f;
    float a2_ = 0.0f;
    float z1_ = 0.0f;
    float z2_ = 0.0f;
};

class DCBlocker {
public:
    float process(float x) {
        const float y = x - x1_ + r_ * y1_;
        x1_ = x;
        y1_ = y;
        return y;
    }

private:
    float r_ = 0.9985f;
    float x1_ = 0.0f;
    float y1_ = 0.0f;
};

class BubblesEngine {
public:
    enum Mode : int {
        FLOW = 0,
        BUBBLE = 1,
        DRIP = 2,
        UNDERWATER = 3,
        HYBRID = 4
    };

    explicit BubblesEngine(float sampleRate)
        : sampleRate_(sampleRate)
    {
        updateFilters();
        updateBodyModes();
        updateTurbulenceBands(true);
    }

    void setMode(float v) {
        mode_ = std::clamp(static_cast<int>(std::lround(v)), 0, 4);
        updateFilters();
        updateTurbulenceBands(true);
    }
    void setIntensity(float v) { intensity_ = std::clamp(v, 0.0f, 1.0f); }
    void setDensity(float v) { density_ = std::clamp(v, 0.0f, 1.0f); }
    void setSize(float v) {
        sizeControl_ = std::clamp(v, 0.0f, 1.0f);
        applySizeMapping();
    }
    void setFlowRate(float v) { flowRate_ = std::clamp(v, 0.0f, 1.0f); }
    void setBrightness(float v) { brightness_ = std::clamp(v, 0.0f, 1.0f); updateFilters(); }
    void setResonance(float v) { resonance_ = std::clamp(v, 0.0f, 1.0f); updateBodyModes(); }
    void setDepth(float v) { depth_ = std::clamp(v, 0.0f, 1.0f); updateFilters(); }
    void setSpace(float v) { space_ = std::clamp(v, 0.0f, 1.0f); }
    void setRandomness(float v) { randomness_ = std::clamp(v, 0.0f, 1.0f); }
    void setHeat(float v) { heat_ = std::clamp(v, 0.0f, 1.0f); }
    void setOutput(float v) { output_ = std::clamp(v, 0.0f, 1.0f); }
    void setDrive(float v) { drive_ = std::clamp(v, 0.0f, 1.0f); }
    void setNoiseFloor(float v) { noiseFloor_ = std::clamp(v, 0.0f, 1.0f); }

    void noteOn(uint8_t midi, float velocity) {
        gate_ = true;
        note_ = midi;
        velocity_ = std::clamp(velocity, 0.0f, 1.0f);
        const float midiNorm = std::clamp((static_cast<float>(midi) - 36.0f) / 60.0f, 0.0f, 1.0f);
        midiSize_ = 1.0f - midiNorm;
        applySizeMapping();
    }

    void noteOff(uint8_t midi) {
        if (midi == note_) {
            gate_ = false;
        }
    }

    void allNotesOff() { gate_ = false; }

    StereoFrame process() {
        const float target = gate_ ? 1.0f : 0.0f;
        env_ += (target - env_) * (gate_ ? 0.0018f : 0.0007f);

        if (env_ < 1.0e-5f) {
            return {0.0f, 0.0f};
        }

        updateTurbulenceBands(false);

        const ModeMix mix = modeMix();

        const float n = whiteNoise();
        const float low1 = flowLP1_.process(n);
        const float low2 = flowLP2_.process(low1);
        const float band = low1 - low2;

        flowAmp_ += (flowTarget_ - flowAmp_) * 0.0008f;
        const float flowEnergy =
            (0.005f + noiseFloor_ * 0.09f + flowRate_ * 0.35f)
            * (0.20f + flowAmp_ * 0.80f)
            * (0.70f + density_ * 0.50f + heat_ * 0.20f);
        const float turb1 = turbulenceBand1_.process(n);
        const float turb2 = turbulenceBand2_.process(n);
        const float turbulence = (0.28f * band + 0.10f * turb1 + 0.05f * turb2) * flowEnergy;

        maybeSpawnBubble(mix.bubbleRateMul);
        maybeSpawnDrip(mix.dripRateMul);

        float bubble = 0.0f;
        for (auto& v : bubbles_) {
            if (!v.active) {
                continue;
            }
            v.env *= v.decay;
            v.impulse *= v.impulseDecay;
            const float exc = v.impulse + whiteNoise() * (0.005f + noiseFloor_ * 0.05f);
            const float reson = v.reson.process(exc);
            bubble += (reson * 0.92f + exc * 0.08f) * v.env;
            if (v.env < 8.0e-5f) {
                v.active = false;
            }
        }

        float drip = 0.0f;
        for (auto& d : drips_) {
            if (!d.active) {
                continue;
            }
            d.env *= d.decay;
            d.impact *= d.impactDecay;
            const float exc = d.impact + whiteNoise() * (0.005f + noiseFloor_ * 0.05f);
            const float ring = d.resonA.process(exc) + 0.65f * d.resonB.process(exc);
            drip += (ring * 0.9f + exc * 0.1f) * d.env;
            if (d.env < 8.0e-5f) {
                d.active = false;
            }
        }

        const float core = turbulence * mix.flow + bubble * mix.bubble + drip * mix.drip;

        float body = core * mix.bodyDirect;
        for (size_t i = 0; i < bodyModes_.size(); ++i) {
            body += bodyModes_[i].process(core) * bodyModeMix_[i] * mix.body;
        }

        const float depthMix = std::clamp(depth_ + mix.depthBias, 0.0f, 1.0f);
        const float wetBody = underwaterFilter_.process(body);
        const float mixed = (1.0f - depthMix) * body + depthMix * wetBody;

        float out = mixed;
        const float synthGain = (0.20f + intensity_ * 1.8f) * velocity_ * env_;
        out *= synthGain;
        out = dcBlocker_.process(out);

        // First saturator catches dense event spikes while preserving transients.
        const float preDrive = 1.2f + drive_ * 2.2f;
        out = softClip(out, preDrive);

        const StereoFrame spatial = processSpace(out);

        // Output control now has a wider usable range with a final safety clipper.
        const float outGain = 0.25f + output_ * 3.75f;
        const float postDrive = 1.8f + drive_ * 3.6f;
        const float left = softClip(spatial.left * outGain, postDrive);
        const float right = softClip(spatial.right * outGain, postDrive);
        return {left, right};
    }

private:
    struct BubbleVoice {
        bool active = false;
        float env = 0.0f;
        float decay = 1.0f;
        float impulse = 0.0f;
        float impulseDecay = 1.0f;
        BiquadBandPass reson;
    };

    struct DripVoice {
        bool active = false;
        float env = 0.0f;
        float decay = 1.0f;
        float impact = 0.0f;
        float impactDecay = 1.0f;
        BiquadBandPass resonA;
        BiquadBandPass resonB;
    };

    struct ModeMix {
        float flow;
        float bubble;
        float drip;
        float body;
        float bodyDirect;
        float bubbleRateMul;
        float dripRateMul;
        float depthBias;
    };

    float sampleRate_ = 48000.0f;

    int mode_ = HYBRID;
    float intensity_ = 0.5f;
    float density_ = 0.5f;
    float size_ = 0.45f;
    float sizeControl_ = 0.45f;
    float midiSize_ = 0.45f;
    float flowRate_ = 0.5f;
    float brightness_ = 0.5f;
    float resonance_ = 0.55f;
    float depth_ = 0.2f;
    float space_ = 0.35f;
    float randomness_ = 0.4f;
    float heat_ = 0.3f;
    float output_ = 0.8f;
    float drive_ = 0.35f;
    float noiseFloor_ = 0.12f;

    bool gate_ = false;
    uint8_t note_ = 0;
    float velocity_ = 1.0f;
    float env_ = 0.0f;

    uint32_t rng_ = 0x12345678u;

    std::array<BubbleVoice, 36> bubbles_{};
    std::array<DripVoice, 16> drips_{};

    OnePole flowLP1_{};
    OnePole flowLP2_{};
    OnePole underwaterFilter_{};

    BiquadBandPass turbulenceBand1_{};
    BiquadBandPass turbulenceBand2_{};

    std::array<BiquadBandPass, 4> bodyModes_{};
    std::array<float, 4> bodyModeMix_{};

    DCBlocker dcBlocker_{};

    std::array<float, 2048> delayL_{};
    std::array<float, 3072> delayR_{};
    size_t idxL_ = 0;
    size_t idxR_ = 0;

    float flowAmp_ = 0.5f;
    float flowTarget_ = 0.5f;
    uint32_t turbUpdateCounter_ = 0;

    float uniform() {
        rng_ = 1664525u * rng_ + 1013904223u;
        return static_cast<float>(rng_ >> 8) * (1.0f / 16777216.0f);
    }

    float whiteNoise() {
        return uniform() * 2.0f - 1.0f;
    }

    float decayFromMs(float ms) const {
        const float samples = ms * 0.001f * sampleRate_;
        return std::exp(-1.0f / std::max(samples, 1.0f));
    }

    ModeMix modeMix() const {
        switch (mode_) {
            case FLOW:
                return {0.62f, 0.52f, 0.24f, 0.68f, 0.62f, 0.65f, 0.45f, 0.0f};
            case BUBBLE:
                return {0.06f, 1.55f, 0.22f, 0.60f, 0.52f, 2.10f, 0.70f, 0.0f};
            case DRIP:
                return {0.05f, 0.16f, 1.68f, 0.62f, 0.58f, 0.25f, 2.45f, 0.0f};
            case UNDERWATER:
                return {0.22f, 0.78f, 0.22f, 0.44f, 0.40f, 1.45f, 0.75f, 0.55f};
            case HYBRID:
            default:
                return {0.22f, 1.05f, 1.00f, 0.64f, 0.56f, 1.15f, 1.10f, 0.0f};
        }
    }

    void maybeSpawnBubble(float modeRateMul) {
        const float baseRate = 0.8f + density_ * 13.0f;
        const float boilRate = heat_ * (6.0f + density_ * 28.0f);
        const float flowCoupling = 0.7f + flowRate_ * 0.6f;
        const float p = (baseRate + boilRate) * modeRateMul * flowCoupling / sampleRate_;
        if (uniform() >= p) {
            return;
        }

        for (auto& v : bubbles_) {
            if (v.active) {
                continue;
            }

            const float r = std::clamp(size_ + (uniform() - 0.5f) * randomness_, 0.04f, 0.97f);
            const float freq = chooseBubbleFreq(r);
            const float q = 0.7f + resonance_ * 3.0f + brightness_ * 0.8f;
            const float decayMs = 14.0f + r * 140.0f + heat_ * 24.0f;

            v.active = true;
            v.env = (0.04f + intensity_ * 0.14f) * (0.65f + 0.7f * uniform());
            v.decay = decayFromMs(decayMs);
            v.impulse = 1.0f + 0.8f * uniform();
            v.impulseDecay = decayFromMs(1.2f + uniform() * 2.5f);
            v.reson.set(sampleRate_, freq, q);
            return;
        }
    }

    void maybeSpawnDrip(float modeRateMul) {
        const float sparseBias = 1.0f - flowRate_;
        const float rate = (0.2f + density_ * 2.7f + sparseBias * 1.5f + heat_ * 0.6f) * modeRateMul;
        const float p = rate / sampleRate_;
        if (uniform() >= p) {
            return;
        }

        for (auto& d : drips_) {
            if (d.active) {
                continue;
            }

            const float baseFreq = chooseDripFreq(size_);
            const float spread = 1.28f + uniform() * 0.45f;

            d.active = true;
            d.env = 0.14f + intensity_ * 0.15f;
            d.decay = decayFromMs(24.0f + uniform() * 120.0f);
            d.impact = 0.9f + uniform() * 0.5f;
            d.impactDecay = decayFromMs(2.5f + uniform() * 7.0f);
            d.resonA.set(sampleRate_, baseFreq, 1.0f + resonance_ * 1.7f + brightness_ * 0.4f);
            d.resonB.set(sampleRate_, baseFreq * spread, 0.7f + resonance_ * 1.1f + brightness_ * 0.35f);
            return;
        }
    }

    void updateFilters() {
        float effectiveBrightness = brightness_;
        if (mode_ == UNDERWATER) {
            effectiveBrightness *= 0.45f;
        }

        const float lp1 = 350.0f + effectiveBrightness * 4200.0f;
        const float lp2 = 90.0f + effectiveBrightness * 1300.0f;
        flowLP1_.setCutoffHz(sampleRate_, lp1);
        flowLP2_.setCutoffHz(sampleRate_, lp2);

        const float depthCut = 200.0f + (1.0f - depth_) * 4200.0f;
        underwaterFilter_.setCutoffHz(sampleRate_, depthCut);
    }

    void updateBodyModes() {
        const float base = 130.0f + size_ * 220.0f;
        const float q = 0.5f + resonance_ * 2.6f;

        bodyModes_[0].set(sampleRate_, base * 1.00f, q * 0.80f);
        bodyModes_[1].set(sampleRate_, base * 2.41f, q * 0.68f);
        bodyModes_[2].set(sampleRate_, base * 4.37f, q * 0.58f);
        bodyModes_[3].set(sampleRate_, base * 6.96f, q * 0.50f);

        bodyModeMix_[0] = 0.18f;
        bodyModeMix_[1] = 0.13f;
        bodyModeMix_[2] = 0.09f;
        bodyModeMix_[3] = 0.06f;
    }

    void applySizeMapping() {
        // MIDI pitch controls bubble size; size knob blends in as offset/control.
        size_ = std::clamp(0.65f * midiSize_ + 0.35f * sizeControl_, 0.0f, 1.0f);
        updateBodyModes();
    }

    float chooseBubbleFreq(float sizeNorm) {
        static constexpr std::array<float, 8> kBubbleHz = {
            230.0f, 310.0f, 420.0f, 560.0f, 730.0f, 920.0f, 1180.0f, 1480.0f
        };
        const int maxIndex = static_cast<int>(kBubbleHz.size()) - 1;
        const int idxFromSize = static_cast<int>((1.0f - sizeNorm) * static_cast<float>(maxIndex));
        const int jitter = static_cast<int>(uniform() * 3.0f) - 1;
        const int idx = std::clamp(idxFromSize + jitter, 0, maxIndex);
        return kBubbleHz[idx] * (0.95f + 0.1f * uniform());
    }

    float chooseDripFreq(float sizeNorm) {
        static constexpr std::array<float, 7> kDripHz = {
            380.0f, 520.0f, 690.0f, 860.0f, 1040.0f, 1270.0f, 1540.0f
        };
        const int maxIndex = static_cast<int>(kDripHz.size()) - 1;
        const int idxFromSize = static_cast<int>((1.0f - sizeNorm) * static_cast<float>(maxIndex));
        const int jitterRange = 1 + static_cast<int>(randomness_ * 2.0f);
        const int jitter = static_cast<int>(uniform() * static_cast<float>(2 * jitterRange + 1)) - jitterRange;
        const int idx = std::clamp(idxFromSize + jitter, 0, maxIndex);
        return kDripHz[idx] * (0.96f + 0.08f * uniform());
    }

    float softClip(float x, float drive) const {
        const float d = std::max(drive, 0.01f);
        return std::tanh(x * d) / std::tanh(d);
    }

    void updateTurbulenceBands(bool force) {
        constexpr uint32_t updateStride = 64;
        if (!force) {
            ++turbUpdateCounter_;
            if (turbUpdateCounter_ < updateStride) {
                return;
            }
        }
        turbUpdateCounter_ = 0;

        float bright = 0.25f + brightness_ * 0.75f;
        if (mode_ == UNDERWATER) {
            bright *= 0.5f;
        }

        const float randAmt = 0.25f + randomness_ * 0.75f;
        const float center1 = 90.0f + bright * 700.0f + uniform() * (120.0f + 220.0f * randAmt);
        const float center2 = 320.0f + bright * 1200.0f + uniform() * (220.0f + 500.0f * randAmt);

        turbulenceBand1_.set(sampleRate_, center1, 0.45f + bright * 1.2f);
        turbulenceBand2_.set(sampleRate_, center2, 0.35f + bright * 0.9f);

        flowTarget_ = (0.2f + 0.8f * uniform()) * (0.75f + 0.5f * randomness_);
    }

    StereoFrame processSpace(float x) {
        const float fb = 0.16f + space_ * 0.42f;

        const float dl = delayL_[idxL_];
        const float dr = delayR_[idxR_];

        delayL_[idxL_] = x + dr * fb;
        delayR_[idxR_] = x + dl * fb;

        idxL_ = (idxL_ + 1) % delayL_.size();
        idxR_ = (idxR_ + 1) % delayR_.size();

        const float dryGain = 1.0f - 0.50f * space_;
        const float wetGain = 0.05f + 0.30f * space_;

        return {
            x * dryGain + dl * wetGain,
            x * dryGain + dr * wetGain
        };
    }
};

} // namespace flues::bubbles

#endif // FLUES_BUBBLES_ENGINE_HPP
