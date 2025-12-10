// drumkit_plugin.cpp
// LV2 plugin wrapper for hardcore industrial drumkit
// MIDI channel 10 only, 8 voices, 18 parameters

#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>

#include "DrumKitEngine.hpp"

#include <memory>
#include <cstring>

#define DRUMKIT_URI "https://danja.github.io/flues/plugins/drumkit"

using namespace flues::drumkit;

// Port indices (must match TTL order exactly)
enum PortIndex : uint32_t {
    PORT_AUDIO_OUT = 0,
    PORT_MIDI_IN,
    PORT_KICK_PITCH,
    PORT_KICK_DECAY,
    PORT_KICK_DRIVE,
    PORT_KICK_PUNCH,
    PORT_SNARE_TONE,
    PORT_SNARE_SNAP,
    PORT_CLAP_DENSITY,
    PORT_CLAP_TONE,
    PORT_TOM_PITCH,
    PORT_TOM_DECAY,
    PORT_HH_BRIGHTNESS,
    PORT_HH_DECAY,
    PORT_CRASH_BRIGHTNESS,
    PORT_CRASH_DECAY,
    PORT_BIT_CRUSH,
    PORT_MASTER_DRIVE,
    PORT_MASTER_REVERB,
    PORT_MASTER_GAIN,
    PORT_TOTAL_COUNT
};

// Plugin instance
struct DrumkitLV2 {
    std::unique_ptr<DrumKitEngine> engine;
    float sampleRate;

    // Port pointers
    const LV2_Atom_Sequence* midiIn;
    float* audioOut;

    // Parameter port pointers (18 params)
    const float* kickPitch;
    const float* kickDecay;
    const float* kickDrive;
    const float* kickPunch;
    const float* snareTone;
    const float* snareSnap;
    const float* clapDensity;
    const float* clapTone;
    const float* tomPitch;
    const float* tomDecay;
    const float* hhBrightness;
    const float* hhDecay;
    const float* crashBrightness;
    const float* crashDecay;
    const float* bitCrush;
    const float* masterDrive;
    const float* masterReverb;
    const float* masterGain;

    // URID mapping
    LV2_URID_Map* map;
    LV2_URID midiEventUrid;
    LV2_URID atomSequenceUrid;
};

/**
 * Apply all parameter values to the engine
 */
static void apply_parameters(DrumkitLV2* self) {
    if (!self || !self->engine) return;

    // Kick (4 params)
    if (self->kickPitch) self->engine->setKickPitch(*self->kickPitch);
    if (self->kickDecay) self->engine->setKickDecay(*self->kickDecay);
    if (self->kickDrive) self->engine->setKickDrive(*self->kickDrive);
    if (self->kickPunch) self->engine->setKickPunch(*self->kickPunch);

    // Snare (2 params)
    if (self->snareTone) self->engine->setSnareTone(*self->snareTone);
    if (self->snareSnap) self->engine->setSnareSnap(*self->snareSnap);

    // Clap (2 params)
    if (self->clapDensity) self->engine->setClapDensity(*self->clapDensity);
    if (self->clapTone) self->engine->setClapTone(*self->clapTone);

    // Toms (2 params shared)
    if (self->tomPitch) self->engine->setTomPitch(*self->tomPitch);
    if (self->tomDecay) self->engine->setTomDecay(*self->tomDecay);

    // Hi-Hats (2 params shared)
    if (self->hhBrightness) self->engine->setHHBrightness(*self->hhBrightness);
    if (self->hhDecay) self->engine->setHHDecay(*self->hhDecay);

    // Crash (2 params)
    if (self->crashBrightness) self->engine->setCrashBrightness(*self->crashBrightness);
    if (self->crashDecay) self->engine->setCrashDecay(*self->crashDecay);

    // Master (4 params)
    if (self->bitCrush) self->engine->setBitCrush(*self->bitCrush);
    if (self->masterDrive) self->engine->setMasterDrive(*self->masterDrive);
    if (self->masterReverb) self->engine->setMasterReverb(*self->masterReverb);
    if (self->masterGain) self->engine->setMasterGain(*self->masterGain);
}

/**
 * Handle MIDI message
 * Responds to all MIDI channels (not just channel 10)
 */
static void handle_midi(DrumkitLV2* self, const uint8_t* msg, uint32_t size) {
    if (size < 2) return;

    const uint8_t status = msg[0];
    const uint8_t command = status & 0xF0;

    if (command == 0x90 && size >= 3) {  // Note On
        const uint8_t note = msg[1];
        const uint8_t velocity = msg[2];

        if (velocity > 0) {
            self->engine->handleNoteOn(note, velocity);
        }
    } else if (command == 0xB0 && size >= 3) {  // Control Change
        const uint8_t cc = msg[1];
        const uint8_t value = msg[2];

        // Handle special CCs
        if (cc == 123 || cc == 120) {  // All Notes Off / All Sounds Off
            self->engine->reset();
        }
    }
}

// ===== LV2 Callbacks =====

extern "C" {

static LV2_Handle instantiate(
    const LV2_Descriptor*,
    double rate,
    const char*,
    const LV2_Feature* const* features
) {
    auto* self = new DrumkitLV2();
    self->sampleRate = static_cast<float>(rate);
    self->engine = std::make_unique<DrumKitEngine>(self->sampleRate);

    // Initialize port pointers to nullptr
    self->midiIn = nullptr;
    self->audioOut = nullptr;
    self->kickPitch = nullptr;
    self->kickDecay = nullptr;
    self->kickDrive = nullptr;
    self->kickPunch = nullptr;
    self->snareTone = nullptr;
    self->snareSnap = nullptr;
    self->clapDensity = nullptr;
    self->clapTone = nullptr;
    self->tomPitch = nullptr;
    self->tomDecay = nullptr;
    self->hhBrightness = nullptr;
    self->hhDecay = nullptr;
    self->crashBrightness = nullptr;
    self->crashDecay = nullptr;
    self->bitCrush = nullptr;
    self->masterDrive = nullptr;
    self->masterReverb = nullptr;
    self->masterGain = nullptr;

    // Get URID map feature (required)
    self->map = nullptr;
    for (const LV2_Feature* const* f = features; f && *f; ++f) {
        if (!strcmp((*f)->URI, LV2_URID__map)) {
            self->map = static_cast<LV2_URID_Map*>((*f)->data);
        }
    }

    if (!self->map) {
        delete self;
        return nullptr;
    }

    // Map URIDs
    self->midiEventUrid = self->map->map(self->map->handle, LV2_MIDI__MidiEvent);
    self->atomSequenceUrid = self->map->map(self->map->handle, LV2_ATOM__Sequence);

    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    auto* self = static_cast<DrumkitLV2*>(instance);

    switch (port) {
        case PORT_AUDIO_OUT: self->audioOut = static_cast<float*>(data); break;
        case PORT_MIDI_IN: self->midiIn = static_cast<const LV2_Atom_Sequence*>(data); break;
        case PORT_KICK_PITCH: self->kickPitch = static_cast<const float*>(data); break;
        case PORT_KICK_DECAY: self->kickDecay = static_cast<const float*>(data); break;
        case PORT_KICK_DRIVE: self->kickDrive = static_cast<const float*>(data); break;
        case PORT_KICK_PUNCH: self->kickPunch = static_cast<const float*>(data); break;
        case PORT_SNARE_TONE: self->snareTone = static_cast<const float*>(data); break;
        case PORT_SNARE_SNAP: self->snareSnap = static_cast<const float*>(data); break;
        case PORT_CLAP_DENSITY: self->clapDensity = static_cast<const float*>(data); break;
        case PORT_CLAP_TONE: self->clapTone = static_cast<const float*>(data); break;
        case PORT_TOM_PITCH: self->tomPitch = static_cast<const float*>(data); break;
        case PORT_TOM_DECAY: self->tomDecay = static_cast<const float*>(data); break;
        case PORT_HH_BRIGHTNESS: self->hhBrightness = static_cast<const float*>(data); break;
        case PORT_HH_DECAY: self->hhDecay = static_cast<const float*>(data); break;
        case PORT_CRASH_BRIGHTNESS: self->crashBrightness = static_cast<const float*>(data); break;
        case PORT_CRASH_DECAY: self->crashDecay = static_cast<const float*>(data); break;
        case PORT_BIT_CRUSH: self->bitCrush = static_cast<const float*>(data); break;
        case PORT_MASTER_DRIVE: self->masterDrive = static_cast<const float*>(data); break;
        case PORT_MASTER_REVERB: self->masterReverb = static_cast<const float*>(data); break;
        case PORT_MASTER_GAIN: self->masterGain = static_cast<const float*>(data); break;
        default: break;
    }
}

static void activate(LV2_Handle instance) {
    auto* self = static_cast<DrumkitLV2*>(instance);
    self->engine = std::make_unique<DrumKitEngine>(self->sampleRate);
}

static void run(LV2_Handle instance, uint32_t n_samples) {
    auto* self = static_cast<DrumkitLV2*>(instance);
    if (!self || !self->audioOut || !self->engine) return;

    // Apply all parameters once per block
    apply_parameters(self);

    // Clear output buffer
    float* out = self->audioOut;
    std::memset(out, 0, n_samples * sizeof(float));

    // Process MIDI events (sample-accurate)
    if (self->midiIn && self->midiIn->atom.type == self->atomSequenceUrid) {
        LV2_ATOM_SEQUENCE_FOREACH(self->midiIn, ev) {
            if (ev->body.type == self->midiEventUrid) {
                const uint8_t* msg = reinterpret_cast<const uint8_t*>(ev + 1);
                handle_midi(self, msg, ev->body.size);
            }
        }
    }

    // Generate audio samples
    for (uint32_t i = 0; i < n_samples; ++i) {
        out[i] = self->engine->process();
    }
}

static void deactivate(LV2_Handle) {}

static void cleanup(LV2_Handle instance) {
    delete static_cast<DrumkitLV2*>(instance);
}

static const void* extension_data(const char*) {
    return nullptr;
}

// LV2 descriptor
static const LV2_Descriptor descriptor = {
    DRUMKIT_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return index == 0 ? &descriptor : nullptr;
}

} // extern "C"
