// TextParser.hpp - Rule-based text to phoneme converter
// Converts English text to IPA vowel sequence using simple digraph rules

#pragma once

#include <string>
#include <vector>
#include <cctype>
#include <algorithm>

namespace chatgen {

enum class Phoneme {
    // Vowels (primary)
    I,        // /i/ as in "see", "meet", "bee"
    E,        // /e/ as in "bed", "get", "let"
    A,        // /a/ as in "father", "far", "car"
    O,        // /o/ as in "home", "go", "toe"
    U,        // /u/ as in "boot", "food", "moon"

    // Vowels (additional)
    AE,       // /æ/ as in "cat", "bat", "sat"
    UH,       // /ʌ/ as in "cup", "but", "sun"
    AW,       // /ɔ/ as in "thought", "caught", "law"
    IH,       // /ɪ/ as in "sit", "bit", "hit"
    UU,       // /ʊ/ as in "put", "foot", "book"
    ER,       // /ɜ/ as in "bird", "her", "word"

    // Plosives
    P,        // /p/ voiceless bilabial plosive
    B,        // /b/ voiced bilabial plosive
    T,        // /t/ voiceless alveolar plosive
    D,        // /d/ voiced alveolar plosive
    K,        // /k/ voiceless velar plosive
    G,        // /g/ voiced velar plosive

    // Fricatives
    F,        // /f/ voiceless labiodental fricative
    V,        // /v/ voiced labiodental fricative
    S,        // /s/ voiceless alveolar fricative
    Z,        // /z/ voiced alveolar fricative
    TH,       // /θ/ voiceless dental fricative ("think")
    DH,       // /ð/ voiced dental fricative ("this")
    SH,       // /ʃ/ voiceless postalveolar fricative ("ship")
    ZH,       // /ʒ/ voiced postalveolar fricative ("measure")
    H,        // /h/ voiceless glottal fricative

    // Affricates
    CH,       // /tʃ/ voiceless postalveolar affricate ("church")
    J,        // /dʒ/ voiced postalveolar affricate ("judge")

    // Nasals
    M,        // /m/ bilabial nasal
    N,        // /n/ alveolar nasal
    NG,       // /ŋ/ velar nasal ("sing")

    // Liquids & Approximants
    L,        // /l/ lateral approximant
    R,        // /r/ rhotic approximant
    W,        // /w/ labial-velar approximant
    Y,        // /j/ palatal approximant

    SILENCE   // No sound (whitespace)
};

class TextParser {
public:
    TextParser() = default;

    // Parse text into phoneme sequence
    std::vector<Phoneme> parse(const std::string& text) {
        std::vector<Phoneme> phonemes;
        if (text.empty()) return phonemes;

        // Convert to lowercase for processing
        std::string lower = text;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        size_t i = 0;
        while (i < lower.length()) {
            char c = lower[i];

            // Check for trigraphs (3-character patterns) first
            if (i + 2 < lower.length()) {
                std::string trigraph = lower.substr(i, 3);

                // ng + vowel → /ŋ/
                if (trigraph.substr(0, 2) == "ng" && (trigraph[2] == 'a' || trigraph[2] == 'e' || trigraph[2] == 'i' || trigraph[2] == 'o' || trigraph[2] == 'u' || trigraph[2] == ' ')) {
                    phonemes.push_back(Phoneme::NG);
                    i += 2;
                    continue;
                }
            }

            // Check for digraphs (2-character patterns)
            if (i + 1 < lower.length()) {
                std::string digraph = lower.substr(i, 2);

                // Vowels
                if (digraph == "ee" || digraph == "ea") {
                    phonemes.push_back(Phoneme::I);  // /i/
                    i += 2;
                    continue;
                }
                if (digraph == "oo") {
                    phonemes.push_back(Phoneme::U);  // /u/
                    i += 2;
                    continue;
                }
                if (digraph == "ah") {
                    phonemes.push_back(Phoneme::A);  // /a/
                    i += 2;
                    continue;
                }
                if (digraph == "oh") {
                    phonemes.push_back(Phoneme::O);  // /o/
                    i += 2;
                    continue;
                }
                if (digraph == "aw") {
                    phonemes.push_back(Phoneme::AW);  // /ɔ/
                    i += 2;
                    continue;
                }
                if (digraph == "uh") {
                    phonemes.push_back(Phoneme::UH);  // /ʌ/
                    i += 2;
                    continue;
                }
                if (digraph == "er" || digraph == "ur" || digraph == "ir") {
                    phonemes.push_back(Phoneme::ER);  // /ɜ/
                    i += 2;
                    continue;
                }
                if (digraph == "ou") {
                    phonemes.push_back(Phoneme::UU);  // /ʊ/ in "should"
                    i += 2;
                    continue;
                }

                // Consonants
                if (digraph == "sh") {
                    phonemes.push_back(Phoneme::SH);  // /ʃ/
                    i += 2;
                    continue;
                }
                if (digraph == "ch") {
                    phonemes.push_back(Phoneme::CH);  // /tʃ/
                    i += 2;
                    continue;
                }
                if (digraph == "th") {
                    // Context-sensitive: voiced in "the", "this"; voiceless in "think"
                    // Default to voiceless
                    phonemes.push_back(Phoneme::TH);  // /θ/
                    i += 2;
                    continue;
                }
                if (digraph == "dh") {
                    phonemes.push_back(Phoneme::DH);  // /ð/
                    i += 2;
                    continue;
                }
                if (digraph == "zh") {
                    phonemes.push_back(Phoneme::ZH);  // /ʒ/
                    i += 2;
                    continue;
                }
                if (digraph == "ng") {
                    phonemes.push_back(Phoneme::NG);  // /ŋ/
                    i += 2;
                    continue;
                }
            }

            // Single character mapping
            switch (c) {
                // Vowels
                case 'i':
                    phonemes.push_back(Phoneme::IH);  // /ɪ/ as in "sit"
                    break;

                case 'e':
                    phonemes.push_back(Phoneme::E);  // /e/ as in "bed"
                    break;

                case 'a':
                    phonemes.push_back(Phoneme::AE);  // /æ/ as in "cat"
                    break;

                case 'o':
                    phonemes.push_back(Phoneme::O);  // /o/ as in "go"
                    break;

                case 'u':
                    phonemes.push_back(Phoneme::UH);  // /ʌ/ as in "cup"
                    break;

                case 'y':  // y as /j/ approximant
                    phonemes.push_back(Phoneme::Y);
                    break;

                // Plosives
                case 'p':
                    phonemes.push_back(Phoneme::P);
                    break;

                case 'b':
                    phonemes.push_back(Phoneme::B);
                    break;

                case 't':
                    phonemes.push_back(Phoneme::T);
                    break;

                case 'd':
                    phonemes.push_back(Phoneme::D);
                    break;

                case 'k':
                case 'c':  // c as /k/ in "cat"
                    phonemes.push_back(Phoneme::K);
                    break;

                case 'g':
                    phonemes.push_back(Phoneme::G);
                    break;

                // Fricatives
                case 'f':
                    phonemes.push_back(Phoneme::F);
                    break;

                case 'v':
                    phonemes.push_back(Phoneme::V);
                    break;

                case 's':
                    phonemes.push_back(Phoneme::S);
                    break;

                case 'z':
                    phonemes.push_back(Phoneme::Z);
                    break;

                case 'h':
                    phonemes.push_back(Phoneme::H);
                    break;

                // Affricates
                case 'j':
                    phonemes.push_back(Phoneme::J);  // /dʒ/ as in "judge"
                    break;

                // Nasals
                case 'm':
                    phonemes.push_back(Phoneme::M);
                    break;

                case 'n':
                    phonemes.push_back(Phoneme::N);
                    break;

                // Liquids & Approximants
                case 'l':
                    phonemes.push_back(Phoneme::L);
                    break;

                case 'r':
                    phonemes.push_back(Phoneme::R);
                    break;

                case 'w':
                    phonemes.push_back(Phoneme::W);
                    break;

                // Whitespace and punctuation → skip
                case ' ':
                case '.':
                case ',':
                case '!':
                case '?':
                case ';':
                case ':':
                case '-':
                case '_':
                case '\'':
                case '"':
                    // Skip silently
                    break;

                // Unhandled characters → skip
                default:
                    break;
            }

            i++;
        }

        // If parsing resulted in empty sequence, add one silence
        if (phonemes.empty()) {
            phonemes.push_back(Phoneme::SILENCE);
        }

        return phonemes;
    }

    // Get phoneme name for debugging/display
    static const char* getPhonemeName(Phoneme p) {
        switch (p) {
            // Vowels (primary)
            case Phoneme::I: return "i";
            case Phoneme::E: return "e";
            case Phoneme::A: return "a";
            case Phoneme::O: return "o";
            case Phoneme::U: return "u";
            // Vowels (additional)
            case Phoneme::AE: return "ae";
            case Phoneme::UH: return "uh";
            case Phoneme::AW: return "aw";
            case Phoneme::IH: return "ih";
            case Phoneme::UU: return "uu";
            case Phoneme::ER: return "er";
            // Plosives
            case Phoneme::P: return "p";
            case Phoneme::B: return "b";
            case Phoneme::T: return "t";
            case Phoneme::D: return "d";
            case Phoneme::K: return "k";
            case Phoneme::G: return "g";
            // Fricatives
            case Phoneme::F: return "f";
            case Phoneme::V: return "v";
            case Phoneme::S: return "s";
            case Phoneme::Z: return "z";
            case Phoneme::TH: return "th";
            case Phoneme::DH: return "dh";
            case Phoneme::SH: return "sh";
            case Phoneme::ZH: return "zh";
            case Phoneme::H: return "h";
            // Affricates
            case Phoneme::CH: return "ch";
            case Phoneme::J: return "j";
            // Nasals
            case Phoneme::M: return "m";
            case Phoneme::N: return "n";
            case Phoneme::NG: return "ng";
            // Liquids & Approximants
            case Phoneme::L: return "l";
            case Phoneme::R: return "r";
            case Phoneme::W: return "w";
            case Phoneme::Y: return "y";
            case Phoneme::SILENCE: return "-";
        }
        return "?";
    }

    // Debug output: show phoneme sequence
    static std::string formatPhonemes(const std::vector<Phoneme>& phonemes) {
        std::string result;
        for (const auto& p : phonemes) {
            if (!result.empty()) result += " ";
            result += "[";
            result += getPhonemeName(p);
            result += "]";
        }
        return result;
    }
};

} // namespace chatgen
