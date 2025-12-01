// MidiGenerator.hpp - Writes MIDI CC messages to LV2 Atom Sequence output
// Handles formant CC messages for Chatterbox control

#pragma once

#include "PhonemeMapper.hpp"
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>
#include <cstring>
#include <cstdio>

namespace chatgen {

class MidiGenerator {
public:
    MidiGenerator()
        : midiEventUrid(0)
        , atomSequenceUrid(0)
        , lastFormants{63, 63, 63, 63}  // Start neutral
        , bufferCapacity(8192)  // Default capacity
    {
        fprintf(stderr, "ChatGen: MidiGenerator initialized\n");
    }

    // Initialize with URID map
    void init(LV2_URID midiEvent, LV2_URID atomSequence) {
        midiEventUrid = midiEvent;
        atomSequenceUrid = atomSequence;
    }

    // Set buffer capacity (called from engine)
    void setCapacity(uint32_t capacity) {
        this->bufferCapacity = capacity;
    }

    // Note: These methods are currently unused but kept for potential future use
    // The plugin now passes through MIDI notes from the host instead of generating them

    // Send CC messages for a phoneme at specific frame offset
    void sendPhonemeChange(LV2_Atom_Sequence* midiOut,
                          const FormantCCs& formants,
                          uint32_t frameOffset) {
        // Always send CCs to maintain formant values
        // (Don't skip on repeated phonemes - Chatterbox needs continuous updates)

        // Send 5 CC messages: F1, F2, F3, F4, Noise
        // CC 71: F1 (Jaw)
        appendCC(midiOut, frameOffset, 71, formants.f1);

        // CC 10: F2 (Tongue)
        appendCC(midiOut, frameOffset, 10, formants.f2);

        // CC 74: F3 (Lips)
        appendCC(midiOut, frameOffset, 74, formants.f3);

        // CC 75: F4 (Quality)
        appendCC(midiOut, frameOffset, 75, formants.f4);

        // CC 102: Noise Level (critical for fricatives!)
        appendCC(midiOut, frameOffset, 102, formants.noise);

        // Debug - only log when formants actually change
        if (formants.f1 != lastFormants.f1 || formants.f2 != lastFormants.f2 ||
            formants.f3 != lastFormants.f3 || formants.f4 != lastFormants.f4 ||
            formants.noise != lastFormants.noise) {
            fprintf(stderr, "ChatGen OUT: Formant CCs F1=%u F2=%u F3=%u F4=%u Noise=%u (frame %u)\n",
                    formants.f1, formants.f2, formants.f3, formants.f4, formants.noise, frameOffset);
        }

        // Remember last formants for debugging
        lastFormants = formants;
    }

    // Reset internal state
    void reset() {
        lastFormants = {63, 63, 63, 63};
    }

private:
    // Generic helper to append any MIDI message
    void appendMidiMessage(LV2_Atom_Sequence* seq, uint32_t frame, const uint8_t* midiMsg, uint32_t size) {
        // Use stored buffer capacity (not current size!)
        const uint32_t capacity = bufferCapacity;

        // Create event
        LV2_Atom_Event ev;
        ev.time.frames = frame;
        ev.body.type = midiEventUrid;
        ev.body.size = size;

        // Append event to sequence
        LV2_Atom_Event* const appendedEvent =
            lv2_atom_sequence_append_event(seq, capacity, &ev);

        if (appendedEvent) {
            // Copy MIDI data after event header
            uint8_t* const eventBody = reinterpret_cast<uint8_t*>(appendedEvent + 1);
            std::memcpy(eventBody, midiMsg, size);
        } else {
            // Failed to append - buffer full or other issue
            fprintf(stderr, "ChatGen ERROR: Failed to append MIDI message (status=0x%02x, capacity=%u, seq_size=%u)\n",
                    midiMsg[0], capacity, seq->atom.size);
        }
    }

    // Append a single MIDI CC message to the sequence
    void appendCC(LV2_Atom_Sequence* seq, uint32_t frame, uint8_t cc, uint8_t value) {
        const uint8_t midiMsg[3] = {
            LV2_MIDI_MSG_CONTROLLER,  // 0xB0 (CC on channel 0)
            cc,                        // CC number
            value                      // CC value (0-127)
        };
        fprintf(stderr, "ChatGen: Appending CC %u = %u at frame %u\n", cc, value, frame);
        appendMidiMessage(seq, frame, midiMsg, 3);
    }

    LV2_URID midiEventUrid;
    LV2_URID atomSequenceUrid;
    FormantCCs lastFormants;  // Track last sent values to avoid redundant CCs
    uint32_t bufferCapacity;  // MIDI output buffer capacity
};

} // namespace chatgen
