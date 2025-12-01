// PhonemeMapper.hpp - Maps IPA vowels to MIDI CC values for formants
// Based on Chatterbox joystick vowel positions (IPA vowel quadrilateral)

#pragma once

#include "TextParser.hpp"
#include <cstdint>

namespace chatgen {

// MIDI CC values for a single phoneme's formant configuration
struct FormantCCs {
    uint8_t f1;     // CC 71: F1 (Jaw)    - 200-1000 Hz
    uint8_t f2;     // CC 10: F2 (Tongue) - 500-3000 Hz
    uint8_t f3;     // CC 74: F3 (Lips)   - 1500-4000 Hz
    uint8_t f4;     // CC 75: F4 (Quality) - 2500-4500 Hz
    uint8_t noise;  // CC 102: Noise Level - 0-127 (0=pure voiced, 127=pure noise)
};

class PhonemeMapper {
public:
    PhonemeMapper() = default;

    // Convert phoneme to MIDI CC values
    FormantCCs getFormantCCs(Phoneme phoneme) const {
        switch (phoneme) {
            // Vowels (primary) - pure voiced, no noise
            case Phoneme::I:  // /i/ as in "see"
                // High front: F1=250Hz, F2=2300Hz
                return {normalizedToCC(0.09f), normalizedToCC(0.71f), 63, 63, 0};

            case Phoneme::E:  // /e/ as in "bed"
                // Mid front: F1=500Hz, F2=1700Hz
                return {normalizedToCC(0.41f), normalizedToCC(0.53f), 63, 63, 0};

            case Phoneme::A:  // /a/ as in "father"
                // Low central: F1=750Hz, F2=1200Hz
                return {normalizedToCC(0.66f), normalizedToCC(0.24f), 63, 63, 0};

            case Phoneme::O:  // /o/ as in "home"
                // Mid back: F1=550Hz, F2=850Hz
                return {normalizedToCC(0.46f), normalizedToCC(0.14f), 63, 63, 0};

            case Phoneme::U:  // /u/ as in "boot"
                // High back: F1=300Hz, F2=870Hz
                return {normalizedToCC(0.13f), normalizedToCC(0.15f), 63, 63, 0};

            // Vowels (additional) - pure voiced, no noise
            case Phoneme::AE:  // /æ/ as in "cat"
                // Low front: F1=650Hz, F2=1700Hz
                return {normalizedToCC(0.56f), normalizedToCC(0.53f), 63, 63, 0};

            case Phoneme::UH:  // /ʌ/ as in "cup"
                // Mid central: F1=600Hz, F2=1200Hz
                return {normalizedToCC(0.50f), normalizedToCC(0.24f), 63, 63, 0};

            case Phoneme::AW:  // /ɔ/ as in "thought"
                // Mid-low back: F1=600Hz, F2=900Hz
                return {normalizedToCC(0.50f), normalizedToCC(0.16f), 63, 63, 0};

            case Phoneme::IH:  // /ɪ/ as in "sit"
                // Near-high front: F1=400Hz, F2=2000Hz
                return {normalizedToCC(0.28f), normalizedToCC(0.60f), 63, 63, 0};

            case Phoneme::UU:  // /ʊ/ as in "put"
                // Near-high back: F1=400Hz, F2=1000Hz
                return {normalizedToCC(0.28f), normalizedToCC(0.20f), 63, 63, 0};

            case Phoneme::ER:  // /ɜ/ as in "bird"
                // Mid central rhotic: F1=500Hz, F2=1400Hz, low F3
                return {normalizedToCC(0.41f), normalizedToCC(0.36f), 45, 63, 5};

            // Plosives - burst noise
            case Phoneme::P:  // /p/ voiceless bilabial
            case Phoneme::B:  // /b/ voiced bilabial
                // Bilabial closure: brief silence then burst
                return {50, 50, 63, 63, 70};

            case Phoneme::T:  // /t/ voiceless alveolar
            case Phoneme::D:  // /d/ voiced alveolar
                // Alveolar closure: brief silence then burst
                return {50, 50, 70, 63, 70};

            case Phoneme::K:  // /k/ voiceless velar
            case Phoneme::G:  // /g/ voiced velar
                // Velar closure: brief silence then burst
                return {50, 50, 55, 63, 70};

            // Fricatives - high noise, reduced voicing
            case Phoneme::F:  // /f/ voiceless labiodental
            case Phoneme::V:  // /v/ voiced labiodental
                // Labiodental: high frequency noise
                return {30, 50, 90, 95, 110};

            case Phoneme::S:  // /s/ voiceless alveolar
            case Phoneme::Z:  // /z/ voiced alveolar
                // Alveolar: very high frequency hissing
                return {25, 50, 100, 100, 120};

            case Phoneme::TH:  // /θ/ voiceless dental ("think")
            case Phoneme::DH:  // /ð/ voiced dental ("this")
                // Dental: soft high frequency
                return {30, 50, 85, 90, 100};

            case Phoneme::SH:  // /ʃ/ voiceless postalveolar ("ship")
            case Phoneme::ZH:  // /ʒ/ voiced postalveolar ("measure")
                // Postalveolar: lower than /s/, broader spectrum
                return {30, 50, 95, 90, 115};

            case Phoneme::H:  // /h/ voiceless glottal
                // Breathy: noise with minimal filtering
                return {50, 50, 50, 50, 90};

            // Affricates - plosive + fricative noise
            case Phoneme::CH:  // /tʃ/ voiceless ("church")
            case Phoneme::J:   // /dʒ/ voiced ("judge")
                // Affricate: plosive + fricative
                return {45, 55, 95, 90, 95};

            // Nasals - mostly voiced with slight noise
            case Phoneme::M:  // /m/ bilabial nasal
                // Nasal: low F1, muffled F2
                return {50, 40, 70, 63, 10};

            case Phoneme::N:  // /n/ alveolar nasal
                // Nasal: mid formants with nasal pole
                return {50, 50, 70, 63, 10};

            case Phoneme::NG:  // /ŋ/ velar nasal ("sing")
                // Velar nasal: lower F2 than /n/
                return {50, 35, 70, 63, 10};

            // Liquids & Approximants - mostly voiced
            case Phoneme::L:  // /l/ lateral
                // Lateral: moderate formants, high F3
                return {45, 55, 80, 63, 5};

            case Phoneme::R:  // /r/ rhotic
                // Rhotic: lowered F3 (retroflex quality)
                return {42, 50, 50, 63, 5};

            case Phoneme::W:  // /w/ labial-velar
                // Like /u/ with quick transition
                return {normalizedToCC(0.13f), normalizedToCC(0.15f), 60, 63, 0};

            case Phoneme::Y:  // /j/ palatal
                // Like /i/ with quick transition
                return {normalizedToCC(0.09f), normalizedToCC(0.71f), 63, 63, 0};

            case Phoneme::SILENCE:
            default:
                // Neutral position - midpoint for all formants
                return {63, 63, 63, 63, 0};
        }
    }

    // Check if two formant configurations are different
    // Used to avoid sending redundant MIDI CC messages
    static bool isDifferent(const FormantCCs& a, const FormantCCs& b) {
        return a.f1 != b.f1 || a.f2 != b.f2 || a.f3 != b.f3 || a.f4 != b.f4 || a.noise != b.noise;
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
