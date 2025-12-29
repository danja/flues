// flues_control_plugin.cpp - LV2 plugin wrapper for Flues Control
// MIDI CC controller for flues-synth with 30 programs (0-29) and 9 sliders

#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/atom/forge.h>
#include <lv2/urid/urid.h>
#include <lv2/midi/midi.h>

#include <cstdlib>
#include <cstring>
#include <cstdio>

#define FLUES_CONTROL_URI "https://danja.github.io/flues/plugins/flues-control"

// Port indices (must match flues-control.ttl)
enum PortIndex {
    PORT_MIDI_IN = 0,
    PORT_MIDI_OUT = 1,
    PORT_PROGRAM = 2,
    PORT_SLIDER1 = 3,
    PORT_SLIDER2 = 4,
    PORT_SLIDER3 = 5,
    PORT_SLIDER4 = 6,
    PORT_SLIDER5 = 7,
    PORT_SLIDER6 = 8,
    PORT_SLIDER7 = 9,
    PORT_SLIDER8 = 10,
    PORT_SLIDER9 = 11
};

// Hardware slider CC mapping (fixed by controller)
static const uint8_t SLIDER_CCS[9] = {
    73,  // Slider 1
    72,  // Slider 2
    28,  // Slider 3
    30,  // Slider 4
    74,  // Slider 5
    71,  // Slider 6
    1,   // Slider 7
    27,  // Slider 8 (Attack)
    7    // Slider 9 (Release)
};

// Plugin instance data
typedef struct {
    // Ports
    const LV2_Atom_Sequence* midiIn;
    LV2_Atom_Sequence* midiOut;
    const float* programPort;
    const float* sliderPorts[9];

    // Features
    LV2_URID_Map* map;
    LV2_URID midiEventUrid;
    LV2_URID atomSequenceUrid;

    // Atom forge
    LV2_Atom_Forge forge;

    // Previous values (for change detection)
    uint8_t lastProgram;
    uint8_t lastSliderValues[9];

    // Buffer capacity
    uint32_t bufferCapacity;

} FluesControlLV2;

// Helper: Convert float (0.0-1.0) to MIDI CC value (0-127)
static inline uint8_t floatToCC(float value) {
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    return (uint8_t)(value * 127.0f + 0.5f);
}

// Helper: Append MIDI message to atom sequence
static void appendMidiMessage(LV2_Atom_Sequence* seq,
                              uint32_t capacity,
                              LV2_URID midiEventUrid,
                              uint32_t frame,
                              const uint8_t* midiMsg,
                              uint32_t size) {
    // Create event header
    LV2_Atom_Event ev;
    ev.time.frames = frame;
    ev.body.type = midiEventUrid;
    ev.body.size = size;

    // Append event to sequence
    LV2_Atom_Event* appendedEvent = lv2_atom_sequence_append_event(seq, capacity, &ev);

    if (appendedEvent) {
        // Copy MIDI data after event header
        uint8_t* eventBody = (uint8_t*)(appendedEvent + 1);
        std::memcpy(eventBody, midiMsg, size);
    } else {
        fprintf(stderr, "Flues Control: Failed to append MIDI message (buffer full)\n");
    }
}

// Helper: Send MIDI Program Change
static void sendProgramChange(FluesControlLV2* self, uint8_t program, uint32_t frame) {
    const uint8_t midiMsg[2] = {
        0xC0,     // Program Change on channel 1
        program   // Program number (0-29)
    };
    appendMidiMessage(self->midiOut, self->bufferCapacity, self->midiEventUrid,
                     frame, midiMsg, 2);
}

// Helper: Send MIDI CC
static void sendCC(FluesControlLV2* self, uint8_t cc, uint8_t value, uint32_t frame) {
    const uint8_t midiMsg[3] = {
        0xB0,    // CC on channel 1
        cc,      // CC number
        value    // CC value (0-127)
    };
    appendMidiMessage(self->midiOut, self->bufferCapacity, self->midiEventUrid,
                     frame, midiMsg, 3);
}

// LV2 instantiate callback
static LV2_Handle instantiate(const LV2_Descriptor* descriptor,
                              double rate,
                              const char* bundle_path,
                              const LV2_Feature* const* features) {
    (void)descriptor;
    (void)rate;
    (void)bundle_path;

    FluesControlLV2* self = new FluesControlLV2;
    if (!self) {
        return nullptr;
    }

    // Initialize ports to null
    self->midiIn = nullptr;
    self->midiOut = nullptr;
    self->programPort = nullptr;
    for (int i = 0; i < 9; i++) {
        self->sliderPorts[i] = nullptr;
    }

    // Get required URID_Map feature
    self->map = nullptr;
    for (int i = 0; features[i]; i++) {
        if (std::strcmp(features[i]->URI, LV2_URID__map) == 0) {
            self->map = static_cast<LV2_URID_Map*>(features[i]->data);
        }
    }

    if (!self->map) {
        fprintf(stderr, "Flues Control: Required feature urid:map not available\n");
        delete self;
        return nullptr;
    }

    // Map URIDs
    self->midiEventUrid = self->map->map(self->map->handle, LV2_MIDI__MidiEvent);
    self->atomSequenceUrid = self->map->map(self->map->handle, LV2_ATOM__Sequence);

    // Initialize atom forge
    lv2_atom_forge_init(&self->forge, self->map);

    // Initialize state
    self->lastProgram = 0;
    for (int i = 0; i < 9; i++) {
        self->lastSliderValues[i] = 63;  // Midpoint (0.5 * 127)
    }
    self->bufferCapacity = 8192;

    return static_cast<LV2_Handle>(self);
}

// LV2 connect_port callback
static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    FluesControlLV2* self = static_cast<FluesControlLV2*>(instance);

    switch (port) {
        case PORT_MIDI_IN:
            self->midiIn = static_cast<const LV2_Atom_Sequence*>(data);
            break;
        case PORT_MIDI_OUT:
            self->midiOut = static_cast<LV2_Atom_Sequence*>(data);
            break;
        case PORT_PROGRAM:
            self->programPort = static_cast<const float*>(data);
            break;
        case PORT_SLIDER1:
        case PORT_SLIDER2:
        case PORT_SLIDER3:
        case PORT_SLIDER4:
        case PORT_SLIDER5:
        case PORT_SLIDER6:
        case PORT_SLIDER7:
        case PORT_SLIDER8:
        case PORT_SLIDER9:
            self->sliderPorts[port - PORT_SLIDER1] = static_cast<const float*>(data);
            break;
        default:
            break;
    }
}

// LV2 activate callback
static void activate(LV2_Handle instance) {
    FluesControlLV2* self = static_cast<FluesControlLV2*>(instance);

    // Reset state
    self->lastProgram = 0;
    for (int i = 0; i < 9; i++) {
        self->lastSliderValues[i] = 63;
    }
}

// LV2 run callback - main processing function
static void run(LV2_Handle instance, uint32_t nframes) {
    FluesControlLV2* self = static_cast<FluesControlLV2*>(instance);

    // Initialize MIDI output sequence (empty)
    if (!self->midiOut) return;

    self->midiOut->atom.type = self->atomSequenceUrid;
    self->midiOut->atom.size = sizeof(LV2_Atom_Sequence_Body);
    self->midiOut->body.unit = 0;
    self->midiOut->body.pad = 0;

    // Pass through incoming MIDI events
    if (self->midiIn) {
        LV2_ATOM_SEQUENCE_FOREACH(self->midiIn, ev) {
            if (ev->body.type == self->midiEventUrid) {
                const uint8_t* const msg = (const uint8_t*)(ev + 1);
                appendMidiMessage(self->midiOut, self->bufferCapacity, self->midiEventUrid,
                                 ev->time.frames, msg, ev->body.size);
            }
        }
    }

    // Check program change
    if (self->programPort) {
        uint8_t currentProgram = (uint8_t)(*self->programPort);
        if (currentProgram != self->lastProgram) {
            sendProgramChange(self, currentProgram, 0);
            self->lastProgram = currentProgram;
            printf("Flues Control: Program Change → %u\n", currentProgram);
        }
    }

    // Check slider changes
    for (int i = 0; i < 9; i++) {
        if (self->sliderPorts[i]) {
            uint8_t currentValue = floatToCC(*self->sliderPorts[i]);
            if (currentValue != self->lastSliderValues[i]) {
                sendCC(self, SLIDER_CCS[i], currentValue, 0);
                self->lastSliderValues[i] = currentValue;
                printf("Flues Control: Slider %d (CC %u) → %u\n", i + 1, SLIDER_CCS[i], currentValue);
            }
        }
    }
}

// LV2 deactivate callback
static void deactivate(LV2_Handle instance) {
    (void)instance;
    // Nothing to do
}

// LV2 cleanup callback
static void cleanup(LV2_Handle instance) {
    FluesControlLV2* self = static_cast<FluesControlLV2*>(instance);
    delete self;
}

// LV2 extension data
static const void* extension_data(const char* uri) {
    (void)uri;
    return nullptr;
}

// LV2 descriptor
static const LV2_Descriptor descriptor = {
    FLUES_CONTROL_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

// Plugin entry point
LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return (index == 0) ? &descriptor : nullptr;
}
