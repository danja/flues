#ifndef FLUES_SYNTH_MIDI_BACKEND_ALSA_H
#define FLUES_SYNTH_MIDI_BACKEND_ALSA_H

#include <stdbool.h>
#include <stdint.h>

// MIDI event types
typedef enum {
    MIDI_NOTE_ON = 0,
    MIDI_NOTE_OFF = 1,
    MIDI_CONTROL_CHANGE = 2,
    MIDI_ALL_NOTES_OFF = 3,
    MIDI_PROGRAM_CHANGE = 4
} MidiEventType;

// MIDI event structure
typedef struct {
    MidiEventType type;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
    uint8_t cc_number;
    uint8_t cc_value;
    uint8_t program_number;
} MidiEvent;

// MIDI event handler callback
typedef void (*MidiEventCallback)(const MidiEvent* event, void* user_data);

// Opaque MIDI backend structure
typedef struct MidiBackendALSA MidiBackendALSA;

// Create MIDI backend
// Auto-connects to the best available external MIDI port
MidiBackendALSA* midi_backend_alsa_create(MidiEventCallback callback, void* user_data);

// Destroy MIDI backend
void midi_backend_alsa_destroy(MidiBackendALSA* backend);

// Start MIDI processing
bool midi_backend_alsa_start(MidiBackendALSA* backend);

// Stop MIDI processing
void midi_backend_alsa_stop(MidiBackendALSA* backend);

#endif // FLUES_SYNTH_MIDI_BACKEND_ALSA_H
