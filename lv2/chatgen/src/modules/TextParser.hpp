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

    // Consonants (basic articulation)
    M,        // /m/ nasal
    N,        // /n/ nasal
    L,        // /l/ liquid
    R,        // /r/ liquid
    S,        // /s/ fricative
    T,        // /t/ plosive
    D,        // /d/ plosive
    K,        // /k/ plosive

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

            // Check for digraphs (2-character patterns) first
            if (i + 1 < lower.length()) {
                std::string digraph = lower.substr(i, 2);

                // /i/ sounds: ee, ea
                if (digraph == "ee" || digraph == "ea") {
                    phonemes.push_back(Phoneme::I);
                    i += 2;
                    continue;
                }

                // /u/ sounds: oo, ou
                if (digraph == "oo" || digraph == "ou") {
                    phonemes.push_back(Phoneme::U);
                    i += 2;
                    continue;
                }

                // /a/ sounds: ah
                if (digraph == "ah") {
                    phonemes.push_back(Phoneme::A);
                    i += 2;
                    continue;
                }

                // /o/ sounds: oh, ow (in "low", not "cow")
                if (digraph == "oh") {
                    phonemes.push_back(Phoneme::O);
                    i += 2;
                    continue;
                }
            }

            // Single character mapping
            switch (c) {
                // Vowels
                case 'i':
                case 'y':  // y as /i/ in "happy"
                    phonemes.push_back(Phoneme::I);
                    break;

                case 'e':
                    phonemes.push_back(Phoneme::E);
                    break;

                case 'a':
                    phonemes.push_back(Phoneme::A);
                    break;

                case 'o':
                    phonemes.push_back(Phoneme::O);
                    break;

                case 'u':
                    phonemes.push_back(Phoneme::U);
                    break;

                // Consonants
                case 'm':
                    phonemes.push_back(Phoneme::M);
                    break;

                case 'n':
                    phonemes.push_back(Phoneme::N);
                    break;

                case 'l':
                    phonemes.push_back(Phoneme::L);
                    break;

                case 'r':
                    phonemes.push_back(Phoneme::R);
                    break;

                case 's':
                    phonemes.push_back(Phoneme::S);
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

                // Whitespace and unhandled consonants → skip
                default:
                    // Skip silently for unhandled characters
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
            case Phoneme::I: return "i";
            case Phoneme::E: return "e";
            case Phoneme::A: return "a";
            case Phoneme::O: return "o";
            case Phoneme::U: return "u";
            case Phoneme::M: return "m";
            case Phoneme::N: return "n";
            case Phoneme::L: return "l";
            case Phoneme::R: return "r";
            case Phoneme::S: return "s";
            case Phoneme::T: return "t";
            case Phoneme::D: return "d";
            case Phoneme::K: return "k";
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
