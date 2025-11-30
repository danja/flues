// PhonemeMapper.hpp - Maps IPA vowels to MIDI CC values for formants
// Based on Chatterbox joystick vowel positions (IPA vowel quadrilateral)

#pragma once

#include "TextParser.hpp"
#include <cstdint>

namespace chatgen {

// MIDI CC values for a single phoneme's formant configuration
struct FormantCCs {
    uint8_t f1;  // CC 71: F1 (Jaw)    - 200-1000 Hz
    uint8_t f2;  // CC 10: F2 (Tongue) - 500-3000 Hz
    uint8_t f3;  // CC 74: F3 (Lips)   - 1500-4000 Hz
    uint8_t f4;  // CC 75: F4 (Quality) - 2500-4500 Hz
};

class PhonemeMapper {
public:
    PhonemeMapper() = default;

    // Convert phoneme to MIDI CC values
    FormantCCs getFormantCCs(Phoneme phoneme) const {
        switch (phoneme) {
            case Phoneme::I:  // /i/ as in "see"
                // From Chatterbox joystick: F1=0.09, F2=0.71
                return {
                    normalizedToCC(0.09f),  // CC71 = 11
                    normalizedToCC(0.71f),  // CC10 = 90
                    63,  // F3 neutral (TODO: add consonant articulation)
                    63   // F4 neutral
                };

            case Phoneme::E:  // /e/ as in "bed"
                // From Chatterbox joystick: F1=0.41, F2=0.53
                return {
                    normalizedToCC(0.41f),  // CC71 = 52
                    normalizedToCC(0.53f),  // CC10 = 67
                    63,
                    63
                };

            case Phoneme::A:  // /a/ as in "father"
                // From Chatterbox joystick: F1=0.66, F2=0.24
                return {
                    normalizedToCC(0.66f),  // CC71 = 84
                    normalizedToCC(0.24f),  // CC10 = 30
                    63,
                    63
                };

            case Phoneme::O:  // /o/ as in "home"
                // From Chatterbox joystick: F1=0.46, F2=0.14
                return {
                    normalizedToCC(0.46f),  // CC71 = 58
                    normalizedToCC(0.14f),  // CC10 = 18
                    63,
                    63
                };

            case Phoneme::U:  // /u/ as in "boot"
                // From Chatterbox joystick: F1=0.13, F2=0.15
                return {
                    normalizedToCC(0.13f),  // CC71 = 17
                    normalizedToCC(0.15f),  // CC10 = 19
                    63,
                    63
                };

            // Consonants - basic articulation patterns
            case Phoneme::M:  // /m/ nasal
            case Phoneme::N:  // /n/ nasal
                // Nasals: lowered F2, moderate F1
                return {50, 40, 70, 63};  // Nasal resonance

            case Phoneme::L:  // /l/ liquid
                // Lateral: moderate formants
                return {45, 55, 80, 63};

            case Phoneme::R:  // /r/ liquid
                // Rhotic: lowered F3
                return {42, 50, 50, 63};

            case Phoneme::S:  // /s/ fricative
                // Fricative: high F3/F4 for hissing
                return {40, 60, 100, 100};

            case Phoneme::T:  // /t/ plosive
            case Phoneme::D:  // /d/ plosive
            case Phoneme::K:  // /k/ plosive
                // Plosives: brief closure/release (neutral)
                return {50, 50, 63, 63};

            case Phoneme::SILENCE:
            default:
                // Neutral position - midpoint for all formants
                return {63, 63, 63, 63};
        }
    }

    // Check if two formant configurations are different
    // Used to avoid sending redundant MIDI CC messages
    static bool isDifferent(const FormantCCs& a, const FormantCCs& b) {
        return a.f1 != b.f1 || a.f2 != b.f2 || a.f3 != b.f3 || a.f4 != b.f4;
    }

private:
    // Convert normalized value (0.0-1.0) to MIDI CC value (0-127)
    static uint8_t normalizedToCC(float normalized) {
        if (normalized <= 0.0f) return 0;
        if (normalized >= 1.0f) return 127;
        return static_cast<uint8_t>(normalized * 127.0f);
    }
};

} // namespace chatgen
