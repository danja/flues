#pragma once

#include <algorithm>
#include <cmath>

namespace flues::disyn {

const float TWO_PI = 2.0f * M_PI;
const float EPSILON = 1e-8f;

enum class AlgorithmType : int {
    // Primitive algorithms (0-6)
    DIRICHLET_PULSE = 0,
    DSF_SINGLE = 1,
    DSF_DOUBLE = 2,
    TANH_SQUARE = 3,
    TANH_SAW = 4,
    PAF = 5,
    MOD_FM = 6,

    // Combination algorithms (7-13)
    COMBINATION_1_HYBRID_FORMANT = 7,     // ModFM + 3× PAF formants
    COMBINATION_2_CASCADED = 8,            // DSF → AsymFM → Tanh
    COMBINATION_3_PARALLEL_BANK = 9,       // 3× ModFM + 2× PAF mixed
    COMBINATION_4_FEEDBACK = 10,           // ModFM with feedback loop
    COMBINATION_5_MORPHING = 11,           // DSF ↔ ModFM ↔ PAF crossfade
    COMBINATION_6_INHARMONIC = 12,         // DSF (φ ratio) → PAF
    COMBINATION_7_ADAPTIVE_FILTER = 13,    // DSF + ModFM (filter emulation)

    // Novel extrapolations (14-16)
    NOVEL_1_MULTISTAGE = 14,               // Tanh → Exp → Ring mod
    NOVEL_2_FREQ_ASYMMETRY = 15,           // Frequency-dependent AsymFM
    NOVEL_3_CROSS_MOD = 16                 // Cross-algorithm modulation
};

// Parameter structures for each algorithm
struct DirichletParams {
    float harmonics;  // 1-64
    float tilt;       // -3 to +15 dB/oct
};

struct DSFParams {
    float decay;  // 0-0.98 or 0-0.96
    float ratio;  // 0.5-4 or 0.5-4.5
};

struct TanhParams {
    float drive;  // 0.05-5 or 0.05-4.5
    float secondary;  // trim (0.2-1.2) or blend (0-1)
};

struct PAFParams {
    float formant;    // 0.5-6 (×f0)
    float bandwidth;  // 50-3000 Hz
};

struct ModFMParams {
    float index;  // 0.01-8
    float ratio;  // 0.25-6
};

class OscillatorModule {
public:
    explicit OscillatorModule(float sampleRate = 44100.0f)
        : sampleRate(sampleRate),
          phase(0.0f),
          modPhase(0.0f),
          secondaryPhase(0.0f),
          secondaryPhaseNeg(0.0f),
          formant1Phase(0.0f),
          formant2Phase(0.0f),
          formant3Phase(0.0f),
          cascade1Phase(0.0f),
          cascade2Phase(0.0f),
          parallel1Phase(0.0f),
          parallel2Phase(0.0f),
          parallel3Phase(0.0f),
          parallel4Phase(0.0f),
          parallel5Phase(0.0f),
          feedbackSample(0.0f),
          morphAlpha(0.0f) {}

    void reset() {
        phase = 0.0f;
        modPhase = 0.0f;
        secondaryPhase = 0.0f;
        secondaryPhaseNeg = 0.0f;
        formant1Phase = 0.0f;
        formant2Phase = 0.0f;
        formant3Phase = 0.0f;
        cascade1Phase = 0.0f;
        cascade2Phase = 0.0f;
        parallel1Phase = 0.0f;
        parallel2Phase = 0.0f;
        parallel3Phase = 0.0f;
        parallel4Phase = 0.0f;
        parallel5Phase = 0.0f;
        feedbackSample = 0.0f;
        morphAlpha = 0.0f;
    }

    // Main process function - dispatches to algorithm-specific methods
    float process(AlgorithmType algorithm, float param1, float param2, float param3, float frequency) {
        switch (algorithm) {
            case AlgorithmType::DIRICHLET_PULSE:
                return processDirichletPulse(param1, param2, frequency);
            case AlgorithmType::DSF_SINGLE:
                return processDSF(param1, param2, frequency);
            case AlgorithmType::DSF_DOUBLE:
                return processDSFDouble(param1, param2, frequency);
            case AlgorithmType::TANH_SQUARE:
                return processTanhSquare(param1, param2, frequency);
            case AlgorithmType::TANH_SAW:
                return processTanhSaw(param1, param2, frequency);
            case AlgorithmType::PAF:
                return processPAF(param1, param2, frequency);
            case AlgorithmType::MOD_FM:
                return processModFM(param1, param2, frequency);

            // Combination algorithms (7-13)
            case AlgorithmType::COMBINATION_1_HYBRID_FORMANT:
                return processCombination1HybridFormant(param1, param2, param3, frequency);
            case AlgorithmType::COMBINATION_2_CASCADED:
                return processCombination2Cascaded(param1, param2, param3, frequency);
            case AlgorithmType::COMBINATION_3_PARALLEL_BANK:
                return processCombination3ParallelBank(param1, param2, param3, frequency);
            case AlgorithmType::COMBINATION_4_FEEDBACK:
                return processCombination4Feedback(param1, param2, param3, frequency);
            case AlgorithmType::COMBINATION_5_MORPHING:
                return processCombination5Morphing(param1, param2, param3, frequency);
            case AlgorithmType::COMBINATION_6_INHARMONIC:
                return processCombination6Inharmonic(param1, param2, param3, frequency);
            case AlgorithmType::COMBINATION_7_ADAPTIVE_FILTER:
                return processCombination7AdaptiveFilter(param1, param2, param3, frequency);

            // Novel extrapolations (14-16)
            case AlgorithmType::NOVEL_1_MULTISTAGE:
                return processNovel1Multistage(param1, param2, param3, frequency);
            case AlgorithmType::NOVEL_2_FREQ_ASYMMETRY:
                return processNovel2FreqAsymmetry(param1, param2, param3, frequency);
            case AlgorithmType::NOVEL_3_CROSS_MOD:
                return processNovel3CrossMod(param1, param2, param3, frequency);

            default:
                return processSine(frequency);
        }
    }

private:
    float sampleRate;

    // Original phase accumulators (algorithms 0-6)
    float phase;
    float modPhase;
    float secondaryPhase;
    float secondaryPhaseNeg;

    // Additional phase accumulators for Combination algorithms (7-16)
    float formant1Phase;      // Hybrid Formant, Parallel Bank
    float formant2Phase;      // Hybrid Formant, Parallel Bank
    float formant3Phase;      // Hybrid Formant, Parallel Bank
    float cascade1Phase;      // Cascaded Spectral
    float cascade2Phase;      // Cascaded Spectral
    float parallel1Phase;     // Parallel Bank
    float parallel2Phase;     // Parallel Bank
    float parallel3Phase;     // Parallel Bank
    float parallel4Phase;     // Parallel Bank
    float parallel5Phase;     // Parallel Bank

    // Feedback buffer for Feedback Network (1-sample delay)
    float feedbackSample;

    // Morph state for Morphing Spectral Engine
    float morphAlpha;

    // Helper: step phase accumulator forward by frequency
    float stepPhase(float currentPhase, float freq) {
        float next = currentPhase + freq / sampleRate;
        return next - std::floor(next);
    }

    // Fallback: simple sine wave
    float processSine(float frequency) {
        phase = stepPhase(phase, frequency);
        return std::sin(phase * TWO_PI);
    }

    // Helper: Asymmetric FM synthesis (NOT one of the 7 primitive algorithms)
    // Formula: cos(ωc + k·sin(ωm)) · exp(k·(r-1/r)·cos(ωm)/2)
    // Used by Combination 2 (Cascaded) and Novel 2 (Freq Asymmetry)
    float processAsymmetricFM(float param1, float param2, float frequency,
                              float& carrierPhaseRef, float& modPhaseRef) {
        // param1 = k (modulation index), param2 = r (asymmetry ratio)
        const float k = expoMap(param1, 0.01f, 10.0f);
        const float r = expoMap(param2, 0.5f, 2.0f);
        const float modFreq = frequency;  // 1:1 ratio (can be adjusted)

        carrierPhaseRef = stepPhase(carrierPhaseRef, frequency);
        modPhaseRef = stepPhase(modPhaseRef, modFreq);

        const float modulator = std::sin(TWO_PI * modPhaseRef);
        const float asymmetry = std::exp(k * (r - 1.0f/r) * std::cos(TWO_PI * modPhaseRef) / 2.0f);
        const float carrier = std::cos(TWO_PI * carrierPhaseRef + k * modulator);

        return carrier * asymmetry * 0.5f;  // Scale to prevent clipping
    }

    // Algorithm 1: Dirichlet Pulse (Band-Limited Pulse)
    float processDirichletPulse(float param1, float param2, float frequency) {
        // Map parameters: param1=harmonics (1-64), param2=tilt (-3 to +15 dB/oct)
        const int harmonics = std::max(1, static_cast<int>(std::round(1.0f + param1 * 63.0f)));
        const float tilt = -3.0f + param2 * 18.0f;

        phase = stepPhase(phase, frequency);
        const float theta = phase * TWO_PI;

        const float numerator = std::sin((2.0f * harmonics + 1.0f) * theta * 0.5f);
        const float denominator = std::sin(theta * 0.5f);

        float value;
        if (std::abs(denominator) < EPSILON) {
            value = 1.0f;
        } else {
            value = (numerator / denominator) - 1.0f;
        }

        const float tiltFactor = std::pow(10.0f, tilt / 20.0f);
        return (value / static_cast<float>(harmonics)) * tiltFactor;
    }

    // Algorithm 2: Single-Sided DSF
    float processDSF(float param1, float param2, float frequency) {
        // Map parameters: param1=decay (0-0.98), param2=ratio (0.5-4)
        const float decay = std::min(param1 * 0.98f, 0.98f);
        const float ratio = expoMap(param2, 0.5f, 4.0f);

        phase = stepPhase(phase, frequency);
        secondaryPhase = stepPhase(secondaryPhase, frequency * ratio);

        const float w = phase * TWO_PI;
        const float t = secondaryPhase * TWO_PI;

        // Scale by 0.5 to match other algorithm levels (was producing 2× excessive RMS)
        return computeDSFComponent(w, t, decay) * 0.5f;
    }

    // Algorithm 3: Double-Sided DSF
    float processDSFDouble(float param1, float param2, float frequency) {
        // Map parameters: param1=decay (0-0.96), param2=ratio (0.5-4.5)
        const float decay = std::min(param1 * 0.96f, 0.96f);
        const float ratio = expoMap(param2, 0.5f, 4.5f);

        phase = stepPhase(phase, frequency);
        secondaryPhase = stepPhase(secondaryPhase, frequency * ratio);
        secondaryPhaseNeg = stepPhase(secondaryPhaseNeg, frequency * ratio);

        const float w = phase * TWO_PI;
        const float tPos = secondaryPhase * TWO_PI;
        const float tNeg = -secondaryPhaseNeg * TWO_PI;

        const float positive = computeDSFComponent(w, tPos, decay);
        const float negative = computeDSFComponent(w, tNeg, decay);

        // Scale by 0.25 to match other algorithm levels (was 0.5, producing 2× excessive RMS)
        return 0.25f * (positive + negative);
    }

    // Helper: DSF computation (Moorer discrete summation formula)
    float computeDSFComponent(float w, float t, float decay) {
        const float denominator = 1.0f - 2.0f * decay * std::cos(t) + decay * decay;
        if (std::abs(denominator) < EPSILON) {
            return 0.0f;
        }

        const float numerator = std::sin(w) - decay * std::sin(w - t);
        const float normalise = std::sqrt(1.0f - decay * decay);
        return (numerator / denominator) * normalise;
    }

    // Algorithm 4: Tanh Square (Hyperbolic Tangent Waveshaping)
    float processTanhSquare(float param1, float param2, float frequency) {
        // Map parameters: param1=drive (0.05-5), param2=trim (0.2-1.2)
        const float drive = expoMap(param1, 0.05f, 5.0f);
        const float trim = expoMap(param2, 0.2f, 1.2f);

        phase = stepPhase(phase, frequency);
        const float carrier = std::sin(phase * TWO_PI);
        return std::tanh(carrier * drive) * trim;
    }

    // Algorithm 5: Tanh Saw (Square-to-Saw Transformation)
    float processTanhSaw(float param1, float param2, float frequency) {
        // Map parameters: param1=drive (0.05-4.5), param2=blend (0-1)
        const float drive = expoMap(param1, 0.05f, 4.5f);
        const float blend = std::clamp(param2, 0.0f, 1.0f);

        phase = stepPhase(phase, frequency);
        const float sine = std::sin(phase * TWO_PI);
        const float square = std::tanh(sine * drive);

        secondaryPhase = stepPhase(secondaryPhase, frequency);
        const float cosine = std::cos(secondaryPhase * TWO_PI);
        const float saw = square + cosine * (1.0f - square * square);

        return square * (1.0f - blend) + saw * blend;
    }

    // Algorithm 6: Phase-Aligned Formant (PAF)
    float processPAF(float param1, float param2, float frequency) {
        // Map parameters: param1=formant (0.5-6 ×f0), param2=bandwidth (50-3000 Hz)
        const float ratio = expoMap(param1, 0.5f, 6.0f);
        const float bandwidth = expoMap(param2, 50.0f, 3000.0f);

        phase = stepPhase(phase, frequency);
        secondaryPhase = stepPhase(secondaryPhase, frequency * ratio);

        const float carrier = std::sin(secondaryPhase * TWO_PI);
        const float mod = std::sin(phase * TWO_PI);
        const float decay = std::exp(-bandwidth / sampleRate);
        modPhase = decay * modPhase + (1.0f - decay) * mod;

        // Scale by 0.5 to match other algorithm levels (was producing 0.112 RMS)
        return carrier * (0.6f + 0.4f * modPhase) * 0.5f;
    }

    // Algorithm 7: Modified FM
    float processModFM(float param1, float param2, float frequency) {
        // Map parameters: param1=index (0.01-8), param2=ratio (0.25-6)
        const float index = expoMap(param1, 0.01f, 8.0f);
        const float ratio = expoMap(param2, 0.25f, 6.0f);

        phase = stepPhase(phase, frequency);
        modPhase = stepPhase(modPhase, frequency * ratio);

        const float carrier = std::cos(phase * TWO_PI);
        const float modulator = std::cos(modPhase * TWO_PI);
        const float envelope = std::exp(-index);

        // Scale by 0.6 to match other algorithm levels (was producing 0.109 RMS)
        return carrier * std::exp(index * (modulator - 1.0f)) * envelope * 0.6f;
    }

    // ==== COMBINATION ALGORITHMS (7-13) ====

    // Combination 1: Hybrid Formant Engine
    // ModFM base + 3× PAF formants at 800, 1200, 2400 Hz
    float processCombination1HybridFormant(float param1, float param2, float param3, float frequency) {
        const float modfmIndex = expoMap(param1, 0.01f, 3.0f);  // Reduced from 8.0 to 3.0
        const float pafBandwidth = expoMap(param2, 50.0f, 3000.0f);
        const float formantSpacing = 0.8f + param3 * 0.4f;  // 0.8-1.2×

        // Generate ModFM base
        phase = stepPhase(phase, frequency);
        modPhase = stepPhase(modPhase, frequency);
        const float modulator = std::sin(TWO_PI * modPhase);
        const float carrier = std::sin(TWO_PI * phase);
        const float base = carrier * std::exp(-modfmIndex * (std::abs(modulator) - 1.0f)) * 0.4f;  // Reduced from 0.6f

        // Generate 3 PAF formants with spacing control
        formant1Phase = stepPhase(formant1Phase, 800.0f * formantSpacing);
        formant2Phase = stepPhase(formant2Phase, 1200.0f * formantSpacing);
        formant3Phase = stepPhase(formant3Phase, 2400.0f * formantSpacing);

        const float formant1 = std::sin(TWO_PI * formant1Phase) * 0.5f;
        const float formant2 = std::sin(TWO_PI * formant2Phase) * 0.5f;
        const float formant3 = std::sin(TWO_PI * formant3Phase) * 0.5f;

        return (base + formant1 + formant2 + formant3) * 0.25f;
    }

    // Combination 2: Cascaded Spectral Sculptor
    // DSF → Asymmetric FM → Tanh (3-stage cascade)
    float processCombination2Cascaded(float param1, float param2, float param3, float frequency) {
        const float dsfDecay = 0.5f + param1 * 0.45f;  // 0.5-0.95
        const float asymRatio = param2;  // passed to AsymFM
        const float tanhDrive = param3 * 5.0f;  // 0-5

        // Stage 1: DSF
        phase = stepPhase(phase, frequency);
        const float theta = TWO_PI * 1.5f;  // Fixed ratio
        const float denom = 1.0f - 2.0f * dsfDecay * std::cos(theta) + dsfDecay * dsfDecay;
        const float stage1 = (std::sin(TWO_PI * phase) - dsfDecay * std::sin(TWO_PI * phase - theta)) / (denom + EPSILON);

        // Stage 2: Asymmetric FM (use stage1 output scaled as modulation)
        const float stage2 = processAsymmetricFM(std::abs(stage1), asymRatio, frequency, cascade1Phase, cascade2Phase);

        // Stage 3: Tanh waveshaping
        const float stage3 = std::tanh(stage2 * tanhDrive);

        return stage3 * 0.6f;
    }

    // Combination 3: Parallel Distortion Bank
    // 3× ModFM (ratios 1:1, 3:2, 4:3) + 2× PAF (800, 2400 Hz) mixed
    float processCombination3ParallelBank(float param1, float param2, float param3, float frequency) {
        const float modfmIndex = expoMap(param1, 0.01f, 8.0f);
        const float pafBandwidth = expoMap(param2, 50.0f, 3000.0f);
        const float mixBalance = param3;  // 0=ModFM, 1=PAF

        // 3 ModFM voices with different ratios
        parallel1Phase = stepPhase(parallel1Phase, frequency);
        parallel2Phase = stepPhase(parallel2Phase, frequency * 1.0f);  // 1:1
        const float mod1 = std::cos(TWO_PI * parallel2Phase);
        const float modfm1 = std::cos(TWO_PI * parallel1Phase) * std::exp(modfmIndex * (mod1 - 1.0f));

        parallel3Phase = stepPhase(parallel3Phase, frequency);
        parallel4Phase = stepPhase(parallel4Phase, frequency * 1.5f);  // 3:2
        const float mod2 = std::cos(TWO_PI * parallel4Phase);
        const float modfm2 = std::cos(TWO_PI * parallel3Phase) * std::exp(modfmIndex * (mod2 - 1.0f));

        parallel5Phase = stepPhase(parallel5Phase, frequency);
        formant1Phase = stepPhase(formant1Phase, frequency * 1.333f);  // 4:3
        const float mod3 = std::cos(TWO_PI * formant1Phase);
        const float modfm3 = std::cos(TWO_PI * parallel5Phase) * std::exp(modfmIndex * (mod3 - 1.0f));

        // 2 PAF formants
        formant2Phase = stepPhase(formant2Phase, 800.0f);
        formant3Phase = stepPhase(formant3Phase, 2400.0f);
        const float paf1 = std::sin(TWO_PI * formant2Phase) * 0.5f;
        const float paf2 = std::sin(TWO_PI * formant3Phase) * 0.5f;

        // Mix with balance control
        const float modfmMix = (modfm1 + modfm2 + modfm3) / 3.0f;
        const float pafMix = (paf1 + paf2) / 2.0f;
        return (modfmMix * (1.0f - mixBalance) + pafMix * mixBalance) * 0.5f;
    }

    // Combination 4: Feedback Distortion Network
    // ModFM with feedback loop
    float processCombination4Feedback(float param1, float param2, float param3, float frequency) {
        const float modfmIndex = expoMap(param1, 0.01f, 8.0f);
        const float feedbackGain = param2 * 0.95f;  // 0-0.95
        // param3 could be feedback filter cutoff, but keeping simple for now

        // Add feedback from previous sample
        const float modifiedFreq = frequency + feedbackSample * feedbackGain * frequency;

        // Process ModFM
        phase = stepPhase(phase, modifiedFreq);
        modPhase = stepPhase(modPhase, modifiedFreq);
        const float modulator = std::cos(TWO_PI * modPhase);
        const float carrier = std::cos(TWO_PI * phase);
        const float output = carrier * std::exp(modfmIndex * (modulator - 1.0f));

        // Store for next sample
        feedbackSample = output;

        return output * 0.5f;
    }

    // Combination 5: Morphing Spectral Engine
    // Continuous crossfade DSF ↔ ModFM ↔ PAF
    float processCombination5Morphing(float param1, float param2, float param3, float frequency) {
        const float morphPos = param1;  // 0=DSF, 0.5=ModFM, 1.0=PAF
        const float character = param2;  // Shared parameter

        float output = 0.0f;

        if (morphPos < 0.5f) {
            // DSF ↔ ModFM blend
            const float alpha = morphPos * 2.0f;  // 0-1

            // DSF
            phase = stepPhase(phase, frequency);
            const float dsfDecay = 0.5f + character * 0.4f;
            const float theta = TWO_PI * 1.5f;
            const float denom = 1.0f - 2.0f * dsfDecay * std::cos(theta) + dsfDecay * dsfDecay;
            const float dsf = (std::sin(TWO_PI * phase) - dsfDecay * std::sin(TWO_PI * phase - theta)) / (denom + EPSILON);

            // ModFM
            modPhase = stepPhase(modPhase, frequency);
            secondaryPhase = stepPhase(secondaryPhase, frequency);
            const float modfmIndex = expoMap(character, 0.01f, 8.0f);
            const float mod = std::cos(TWO_PI * secondaryPhase);
            const float modfm = std::cos(TWO_PI * modPhase) * std::exp(modfmIndex * (mod - 1.0f));

            output = dsf * (1.0f - alpha) + modfm * alpha;
        } else {
            // ModFM ↔ PAF blend
            const float alpha = (morphPos - 0.5f) * 2.0f;  // 0-1

            // ModFM
            modPhase = stepPhase(modPhase, frequency);
            secondaryPhase = stepPhase(secondaryPhase, frequency);
            const float modfmIndex = expoMap(character, 0.01f, 8.0f);
            const float mod = std::cos(TWO_PI * secondaryPhase);
            const float modfm = std::cos(TWO_PI * modPhase) * std::exp(modfmIndex * (mod - 1.0f));

            // PAF (simplified)
            formant1Phase = stepPhase(formant1Phase, frequency * 2.0f);
            const float paf = std::sin(TWO_PI * formant1Phase) * 0.5f;

            output = modfm * (1.0f - alpha) + paf * alpha;
        }

        return output * 0.6f;
    }

    // Combination 6: Inharmonic Resonator
    // DSF (golden ratio) → PAF (shifted formant)
    float processCombination6Inharmonic(float param1, float param2, float param3, float frequency) {
        const float phiRatio = 1.618034f;  // Golden ratio
        const float pafShift = expoMap(param2, 5.0f, 50.0f);
        const float dsfDecay = 0.5f + param1 * 0.4f;

        // Stage 1: DSF with irrational ratio
        phase = stepPhase(phase, frequency);
        const float theta = TWO_PI * phiRatio;
        const float denom = 1.0f - 2.0f * dsfDecay * std::cos(theta) + dsfDecay * dsfDecay;
        const float dsf = (std::sin(TWO_PI * phase) - dsfDecay * std::sin(TWO_PI * phase - theta)) / (denom + EPSILON);

        // Stage 2: PAF with shifted formant
        const float formantFreq = frequency * 2.0f + pafShift;
        formant1Phase = stepPhase(formant1Phase, formantFreq);
        const float paf = std::sin(TWO_PI * formant1Phase) * 0.5f;

        return (dsf + paf) * 0.5f;
    }

    // Combination 7: Adaptive Filter Emulation
    // DSF (N, a) + ModFM (k) mixed to emulate filter sweep
    float processCombination7AdaptiveFilter(float param1, float param2, float param3, float frequency) {
        const float cutoff = param1;      // Controls DSF "partials"
        const float resonance = param2;   // Controls DSF decay

        // DSF acts as "filter cutoff" via decay parameter
        const float dsfDecay = 0.5f + resonance * 0.49f;  // 0.5-0.99
        phase = stepPhase(phase, frequency);
        const float theta = TWO_PI * (1.0f + cutoff * 2.0f);  // Cutoff affects ratio
        const float denom = 1.0f - 2.0f * dsfDecay * std::cos(theta) + dsfDecay * dsfDecay;
        const float dsf = (std::sin(TWO_PI * phase) - dsfDecay * std::sin(TWO_PI * phase - theta)) / (denom + EPSILON);

        // ModFM acts as "filter character"
        const float modfmIndex = expoMap(cutoff, 0.01f, 2.0f);  // Reduced from 8.0 to 2.0
        modPhase = stepPhase(modPhase, frequency);
        secondaryPhase = stepPhase(secondaryPhase, frequency);
        const float mod = std::cos(TWO_PI * secondaryPhase);
        const float modfm = std::cos(TWO_PI * modPhase) * std::exp(modfmIndex * (mod - 1.0f));

        return (dsf + modfm) * 0.15f;  // Reduced from 0.5f to 0.15f
    }

    // ==== NOVEL EXTRAPOLATIONS (14-16) ====

    // Novel 1: Multi-Stage Waveshaping Cascade
    // Tanh → Exponential → Ring mod
    float processNovel1Multistage(float param1, float param2, float param3, float frequency) {
        const float tanhDrive = expoMap(param1, 0.1f, 10.0f);
        const float expDepth = expoMap(param2, 0.1f, 1.5f);  // Reduced from 5.0 to 1.5
        const float ringCarrierMult = 0.5f + param3 * 4.5f;  // 0.5x-5x

        // Generate base waveform
        phase = stepPhase(phase, frequency);
        const float input = std::sin(TWO_PI * phase);

        // Stage 1: Tanh
        const float stage1 = std::tanh(tanhDrive * input);

        // Stage 2: Exponential shaping
        const float stage2 = stage1 * std::exp(expDepth * stage1);

        // Stage 3: Ring modulation
        modPhase = stepPhase(modPhase, frequency * ringCarrierMult);
        const float carrier = std::sin(TWO_PI * modPhase);
        const float stage3 = stage2 * (1.0f + carrier);

        return stage3 * 0.25f;  // Reduced from 0.4f to 0.25f
    }

    // Novel 2: Frequency-Dependent Asymmetry
    // Asymmetric FM with frequency-band-specific r values
    float processNovel2FreqAsymmetry(float param1, float param2, float param3, float frequency) {
        const float lowR = 0.5f + param1 * 0.5f;    // 0.5-1.0
        const float highR = 1.0f + param2 * 1.0f;   // 1.0-2.0

        // Determine r based on frequency
        float r;
        if (frequency < 500.0f) {
            r = lowR;  // Low band: downward shift
        } else if (frequency > 2000.0f) {
            r = highR;  // High band: upward shift
        } else {
            // Mid band: linear interpolation
            const float alpha = (frequency - 500.0f) / 1500.0f;
            r = lowR * (1.0f - alpha) + highR * alpha;
        }

        // Process Asymmetric FM with calculated r
        return processAsymmetricFM(0.5f, r / 2.0f, frequency, phase, modPhase);
    }

    // Novel 3: Cross-Algorithm Modulation
    // DSF.a → ModFM.k, ModFM.k → DSF.N (circular modulation)
    float processNovel3CrossMod(float param1, float param2, float param3, float frequency) {
        const float mod1Depth = param1;  // DSF→ModFM depth
        const float mod2Depth = param2;  // ModFM→DSF depth

        // Base parameters
        const float baseDsfDecay = 0.7f;
        const float baseDsfRatio = 1.5f;
        const float baseModfmIndex = 0.25f;  // Reduced from 0.5f

        // Circular modulation (reduced multipliers)
        const float dsfRatio = baseDsfRatio + mod2Depth * baseModfmIndex * 0.5f;  // Reduced from 2.0f
        const float modfmIndex = baseModfmIndex + mod1Depth * baseDsfDecay * 1.0f;  // Reduced from 5.0f

        // Generate DSF
        phase = stepPhase(phase, frequency);
        const float theta = TWO_PI * dsfRatio;
        const float denom = 1.0f - 2.0f * baseDsfDecay * std::cos(theta) + baseDsfDecay * baseDsfDecay;
        const float dsf = (std::sin(TWO_PI * phase) - baseDsfDecay * std::sin(TWO_PI * phase - theta)) / (denom + EPSILON);

        // Generate ModFM
        modPhase = stepPhase(modPhase, frequency);
        secondaryPhase = stepPhase(secondaryPhase, frequency);
        const float mod = std::cos(TWO_PI * secondaryPhase);
        const float modfm = std::cos(TWO_PI * modPhase) * std::exp(modfmIndex * (mod - 1.0f));

        return (dsf + modfm) * 0.35f;  // Reduced from 0.5f
    }

    // Helper: exponential mapping from normalized 0-1 to min-max range
    float expoMap(float value, float min, float max) {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        return min * std::pow(max / min, clamped);
    }
};

} // namespace flues::disyn
