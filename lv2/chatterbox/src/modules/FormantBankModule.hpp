// FormantBankModule.hpp
// Cascade of four formant filters (F1, F2, F3, F4)
// Ported from experiments/chatterbox/src/audio/modules/FormantBankModule.js

#ifndef FORMANT_BANK_MODULE_HPP
#define FORMANT_BANK_MODULE_HPP

#include "FormantModule.hpp"
#include <array>
#include <string>

class FormantBankModule {
private:
    float sampleRate;

    // Four formant filters (F1-F4)
    std::array<FormantModule, 4> formants;

    // Optional nasal formant (5th formant)
    FormantModule nasalFormant;
    bool nasalEnabled;

    // Gain compensation for cascade
    // With Q-based filters, we need moderate gain compensation
    float makeupGain;

public:
    FormantBankModule(float sampleRate)
        : sampleRate(sampleRate)
        , formants{FormantModule(sampleRate), FormantModule(sampleRate),
                   FormantModule(sampleRate), FormantModule(sampleRate)}
        , nasalFormant(sampleRate)
        , nasalEnabled(false)
        , makeupGain(3.0f)
    {
        // Configure nasal formant
        nasalFormant.setFrequency(250.0f);   // Typical nasal formant
        nasalFormant.setBandwidth(100.0f);   // Wide bandwidth for nasal resonance

        // Set default formant frequencies and bandwidths
        // These approximate a neutral vowel (schwa)
        setFormant(0, 500.0f, 80.0f);    // F1 - wider bandwidth
        setFormant(1, 1500.0f, 120.0f);  // F2 - wider bandwidth
        setFormant(2, 2500.0f, 150.0f);  // F3 - wider bandwidth
        setFormant(3, 3500.0f, 200.0f);  // F4 - wider bandwidth
    }

    /**
     * Set a specific formant's frequency and bandwidth
     * @param index - Formant index (0-3 for F1-F4)
     * @param frequency - Center frequency in Hz
     * @param bandwidth - Bandwidth in Hz
     */
    void setFormant(int index, float frequency, float bandwidth) {
        if (index >= 0 && index < 4) {
            formants[index].setFrequency(frequency);
            formants[index].setBandwidth(bandwidth);
        }
    }

    /**
     * Set formant frequency only
     * @param index - Formant index (0-3)
     * @param frequency - Center frequency in Hz
     */
    void setFormantFrequency(int index, float frequency) {
        if (index >= 0 && index < 4) {
            formants[index].setFrequency(frequency);
        }
    }

    /**
     * Set formant bandwidth only
     * @param index - Formant index (0-3)
     * @param bandwidth - Bandwidth in Hz
     */
    void setFormantBandwidth(int index, float bandwidth) {
        if (index >= 0 && index < 4) {
            formants[index].setBandwidth(bandwidth);
        }
    }

    /**
     * Set formant using Q factor
     * @param index - Formant index (0-3)
     * @param q - Quality factor
     */
    void setFormantQ(int index, float q) {
        if (index >= 0 && index < 4) {
            formants[index].setQ(q);
        }
    }

    /**
     * Enable or disable nasal resonance
     * @param enabled - True to add nasal formant to cascade
     */
    void setNasal(bool enabled) {
        nasalEnabled = enabled;
    }

    /**
     * Load a vowel preset
     * @param vowel - Vowel character ('a', 'e', 'i', 'o', 'u')
     */
    void setVowel(char vowel) {
        // Typical formant frequencies for vowels (average male voice)
        switch (vowel) {
            case 'a':
                setFormant(0, 730.0f, 80.0f);
                setFormant(1, 1090.0f, 120.0f);
                setFormant(2, 2440.0f, 150.0f);
                setFormant(3, 3200.0f, 200.0f);
                break;
            case 'e':
                setFormant(0, 530.0f, 80.0f);
                setFormant(1, 1840.0f, 120.0f);
                setFormant(2, 2480.0f, 150.0f);
                setFormant(3, 3500.0f, 200.0f);
                break;
            case 'i':
                setFormant(0, 270.0f, 80.0f);
                setFormant(1, 2290.0f, 120.0f);
                setFormant(2, 3010.0f, 150.0f);
                setFormant(3, 3500.0f, 200.0f);
                break;
            case 'o':
                setFormant(0, 570.0f, 80.0f);
                setFormant(1, 840.0f, 120.0f);
                setFormant(2, 2410.0f, 150.0f);
                setFormant(3, 3200.0f, 200.0f);
                break;
            case 'u':
                setFormant(0, 300.0f, 80.0f);
                setFormant(1, 870.0f, 120.0f);
                setFormant(2, 2240.0f, 150.0f);
                setFormant(3, 3200.0f, 200.0f);
                break;
        }
    }

    /**
     * Process one sample through the formant cascade
     * @param input - Input sample (excitation signal)
     * @return Filtered output
     */
    float process(float input) {
        // Pass signal through all four formants in series
        float signal = input;

        for (int i = 0; i < 4; i++) {
            signal = formants[i].process(signal);
        }

        // If nasal mode is enabled, add nasal resonance in parallel
        if (nasalEnabled) {
            const float nasal = nasalFormant.process(input) * 0.3f;  // Attenuate nasal component
            signal += nasal;
        }

        // Apply makeup gain to compensate for cascade attenuation
        return signal * makeupGain;
    }

    /**
     * Reset all formant filter states
     */
    void reset() {
        for (int i = 0; i < 4; i++) {
            formants[i].reset();
        }
        nasalFormant.reset();
    }
};

#endif // FORMANT_BANK_MODULE_HPP
