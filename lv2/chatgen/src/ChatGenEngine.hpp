// ChatGenEngine.hpp - Main coordinator for text-to-MIDI conversion
// Orchestrates TextParser, PhonemeMapper, ClockSync, and MidiGenerator

#pragma once

#include "modules/TextParser.hpp"
#include "modules/PhonemeMapper.hpp"
#include "modules/ClockSync.hpp"
#include "modules/MidiGenerator.hpp"
#include <lv2/atom/atom.h>
#include <string>
#include <vector>
#include <cstdio>

namespace chatgen {

class ChatGenEngine {
public:
    ChatGenEngine()
        : currentPhonemeIndex(0)
        , playing(false)
        , loop(true)
        , samplesSinceLastCC(999999)  // Force immediate send on first process
    {
        // Start with default text (matches UI default)
        setText("hello world");
    }

    // Initialize with sample rate and URID map
    void init(double sampleRate, LV2_URID midiEvent, LV2_URID atomSequence) {
        clock.init(sampleRate);
        midiGen.init(midiEvent, atomSequence);
    }

    // Update text and reparse
    void setText(const std::string& newText) {
        text = newText;
        phonemes = parser.parse(text);
        currentPhonemeIndex = 0;
    }

    // Get current text
    const std::string& getText() const {
        return text;
    }

    // Get current phoneme sequence (for debugging/display)
    const std::vector<Phoneme>& getPhonemes() const {
        return phonemes;
    }

    // Set BPM
    void setBPM(float bpm) {
        clock.setBPM(bpm);
    }

    // Set playing state
    void setPlaying(bool shouldPlay) {
        if (shouldPlay && !playing) {
            // Starting playback - reset position
            currentPhonemeIndex = 0;
            clock.reset();
        }
        playing = shouldPlay;
    }

    // Set loop mode
    void setLoop(bool shouldLoop) {
        loop = shouldLoop;
    }

    // Get current state
    bool isPlaying() const { return playing; }
    bool isLooping() const { return loop; }
    float getBPM() const { return clock.getBPM(); }

    // Main processing function
    void process(LV2_Atom_Sequence* midiOut, uint32_t capacity, uint32_t nframes) {
        // NOTE: midiOut is already initialized by plugin and may contain pass-through MIDI events
        // We just add formant CCs to it
        midiGen.setCapacity(capacity);

        // If not playing, don't add CCs
        if (!playing || phonemes.empty()) {
            return;
        }

        // Check if we crossed a beat boundary
        uint32_t frameOffset = 0;
        bool beatOccurred = clock.process(nframes, frameOffset);

        if (beatOccurred) {
            // Beat occurred - send CCs for NEW phoneme AFTER advancing
            // Advance to next phoneme
            currentPhonemeIndex++;

            // Handle end of sequence
            if (currentPhonemeIndex >= phonemes.size()) {
                if (loop) {
                    // Loop back to start
                    currentPhonemeIndex = 0;
                } else {
                    // Stop playback
                    playing = false;
                    currentPhonemeIndex = 0;
                    return;
                }
            }

            // Send CCs for the new phoneme
            if (currentPhonemeIndex < phonemes.size()) {
                Phoneme currentPhoneme = phonemes[currentPhonemeIndex];
                FormantCCs formants = mapper.getFormantCCs(currentPhoneme);

                // Send MIDI CC messages at beat boundary
                midiGen.sendPhonemeChange(midiOut, formants, frameOffset);
                samplesSinceLastCC = 0;

                fprintf(stderr, "ChatGen beat! Phoneme[%zu] = %s, F1=%u F2=%u F3=%u F4=%u\n",
                        currentPhonemeIndex,
                        TextParser::getPhonemeName(currentPhoneme),
                        formants.f1, formants.f2, formants.f3, formants.f4);
            }
        }

        // Refresh CCs every ~200ms to ensure formants are maintained
        // (but not too often to avoid warbling from filter reconfiguration)
        samplesSinceLastCC += nframes;
        const uint32_t ccRefreshInterval = 8820; // ~200ms at 44.1kHz

        if (samplesSinceLastCC >= ccRefreshInterval && currentPhonemeIndex < phonemes.size()) {
            Phoneme currentPhoneme = phonemes[currentPhonemeIndex];
            FormantCCs formants = mapper.getFormantCCs(currentPhoneme);

            // Refresh CCs at frame 0
            midiGen.sendPhonemeChange(midiOut, formants, 0);
            samplesSinceLastCC = 0;
        }
    }

    // Reset to initial state
    void reset() {
        currentPhonemeIndex = 0;
        playing = false;
        samplesSinceLastCC = 999999;  // Force immediate CC send
        clock.reset();
        midiGen.reset();
    }

private:
    TextParser parser;
    PhonemeMapper mapper;
    ClockSync clock;
    MidiGenerator midiGen;

    std::string text;
    std::vector<Phoneme> phonemes;
    size_t currentPhonemeIndex;

    bool playing;
    bool loop;

    // CC refresh timing to maintain formant values
    uint32_t samplesSinceLastCC;
};

} // namespace chatgen
