// drumkit_plugin.cpp
// LV2 plugin wrapper for hardcore industrial drumkit
// MIDI channel 10 only, 11 voices, 43 parameters

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
    PORT_KICK_LEVEL,
    PORT_SNARE_TONE,
    PORT_SNARE_SNAP,
    PORT_SNARE_LEVEL,
    PORT_CLAP_DENSITY,
    PORT_CLAP_TONE,
    PORT_CLAP_LEVEL,
    PORT_TOM1_PITCH,
    PORT_TOM1_DECAY,
    PORT_TOM1_LEVEL,
    PORT_TOM2_PITCH,
    PORT_TOM2_DECAY,
    PORT_TOM2_LEVEL,
    PORT_HH_CLOSED_BRIGHTNESS,
    PORT_HH_CLOSED_DECAY,
    PORT_HH_CLOSED_LEVEL,
    PORT_HH_OPEN_BRIGHTNESS,
    PORT_HH_OPEN_DECAY,
    PORT_HH_OPEN_LEVEL,
    PORT_CRASH_BRIGHTNESS,
    PORT_CRASH_DECAY,
    PORT_CRASH_LEVEL,
    PORT_COWBELL_TONE,
    PORT_COWBELL_DECAY,
    PORT_COWBELL_LEVEL,
    PORT_CLAVE_TONE,
    PORT_CLAVE_DECAY,
    PORT_CLAVE_LEVEL,
    PORT_BASH_SIZE,
    PORT_BASH_SPREAD,
    PORT_BASH_DECAY,
    PORT_BASH_DRIVE,
    PORT_BASH_NOISE,
    PORT_BASH_EDGE,
    PORT_BASH_LEVEL,
    PORT_BIT_CRUSH,
    PORT_MASTER_DRIVE,
    PORT_MASTER_REVERB,
    PORT_MASTER_GAIN,
    PORT_AUDIO_OUT_RIGHT,
    PORT_TOTAL_COUNT
};

// Plugin instance
struct DrumkitLV2 {
    std::unique_ptr<DrumKitEngine> engine;
    float sampleRate;

    // Port pointers
    const LV2_Atom_Sequence* midiIn;
    float* audioOut;
    float* audioOutRight;

    // Parameter port pointers (43 params)
    const float* kickPitch;
    const float* kickDecay;
    const float* kickDrive;
    const float* kickPunch;
    const float* kickLevel;
    const float* snareTone;
    const float* snareSnap;
    const float* snareLevel;
    const float* clapDensity;
    const float* clapTone;
    const float* clapLevel;
    const float* tom1Pitch;
    const float* tom1Decay;
    const float* tom1Level;
    const float* tom2Pitch;
    const float* tom2Decay;
    const float* tom2Level;
    const float* hhClosedBrightness;
    const float* hhClosedDecay;
    const float* hhClosedLevel;
    const float* hhOpenBrightness;
    const float* hhOpenDecay;
    const float* hhOpenLevel;
    const float* crashBrightness;
    const float* crashDecay;
    const float* crashLevel;
    const float* cowbellTone;
    const float* cowbellDecay;
    const float* cowbellLevel;
    const float* claveTone;
    const float* claveDecay;
    const float* claveLevel;
    const float* bashSize;
    const float* bashSpread;
    const float* bashDecay;
    const float* bashDrive;
    const float* bashNoise;
    const float* bashEdge;
    const float* bashLevel;
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
    if (self->kickLevel) self->engine->setKickLevel(*self->kickLevel);

    // Snare (2 params)
    if (self->snareTone) self->engine->setSnareTone(*self->snareTone);
    if (self->snareSnap) self->engine->setSnareSnap(*self->snareSnap);
    if (self->snareLevel) self->engine->setSnareLevel(*self->snareLevel);

    // Clap (2 params)
    if (self->clapDensity) self->engine->setClapDensity(*self->clapDensity);
    if (self->clapTone) self->engine->setClapTone(*self->clapTone);
    if (self->clapLevel) self->engine->setClapLevel(*self->clapLevel);

    // Toms (independent)
    if (self->tom1Pitch) self->engine->setTom1Pitch(*self->tom1Pitch);
    if (self->tom1Decay) self->engine->setTom1Decay(*self->tom1Decay);
    if (self->tom1Level) self->engine->setTom1Level(*self->tom1Level);
    if (self->tom2Pitch) self->engine->setTom2Pitch(*self->tom2Pitch);
    if (self->tom2Decay) self->engine->setTom2Decay(*self->tom2Decay);
    if (self->tom2Level) self->engine->setTom2Level(*self->tom2Level);

    // Hi-Hats (independent)
    if (self->hhClosedBrightness) self->engine->setClosedHHBrightness(*self->hhClosedBrightness);
    if (self->hhClosedDecay) self->engine->setClosedHHDecay(*self->hhClosedDecay);
    if (self->hhClosedLevel) self->engine->setClosedHHLevel(*self->hhClosedLevel);
    if (self->hhOpenBrightness) self->engine->setOpenHHBrightness(*self->hhOpenBrightness);
    if (self->hhOpenDecay) self->engine->setOpenHHDecay(*self->hhOpenDecay);
    if (self->hhOpenLevel) self->engine->setOpenHHLevel(*self->hhOpenLevel);

    // Crash (2 params)
    if (self->crashBrightness) self->engine->setCrashBrightness(*self->crashBrightness);
    if (self->crashDecay) self->engine->setCrashDecay(*self->crashDecay);
    if (self->crashLevel) self->engine->setCrashLevel(*self->crashLevel);

    // Cowbell (2 params)
    if (self->cowbellTone) self->engine->setCowbellTone(*self->cowbellTone);
    if (self->cowbellDecay) self->engine->setCowbellDecay(*self->cowbellDecay);
    if (self->cowbellLevel) self->engine->setCowbellLevel(*self->cowbellLevel);

    // Clave (2 params)
    if (self->claveTone) self->engine->setClaveTone(*self->claveTone);
    if (self->claveDecay) self->engine->setClaveDecay(*self->claveDecay);
    if (self->claveLevel) self->engine->setClaveLevel(*self->claveLevel);

    // Bash (6 params)
    if (self->bashSize) self->engine->setBashSize(*self->bashSize);
    if (self->bashSpread) self->engine->setBashSpread(*self->bashSpread);
    if (self->bashDecay) self->engine->setBashDecay(*self->bashDecay);
    if (self->bashDrive) self->engine->setBashDrive(*self->bashDrive);
    if (self->bashNoise) self->engine->setBashNoise(*self->bashNoise);
    if (self->bashEdge) self->engine->setBashEdge(*self->bashEdge);
    if (self->bashLevel) self->engine->setBashLevel(*self->bashLevel);

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
    self->audioOutRight = nullptr;
    self->kickPitch = nullptr;
    self->kickDecay = nullptr;
    self->kickDrive = nullptr;
    self->kickPunch = nullptr;
    self->kickLevel = nullptr;
    self->snareTone = nullptr;
    self->snareSnap = nullptr;
    self->snareLevel = nullptr;
    self->clapDensity = nullptr;
    self->clapTone = nullptr;
    self->clapLevel = nullptr;
    self->tom1Pitch = nullptr;
    self->tom1Decay = nullptr;
    self->tom1Level = nullptr;
    self->tom2Pitch = nullptr;
    self->tom2Decay = nullptr;
    self->tom2Level = nullptr;
    self->hhClosedBrightness = nullptr;
    self->hhClosedDecay = nullptr;
    self->hhClosedLevel = nullptr;
    self->hhOpenBrightness = nullptr;
    self->hhOpenDecay = nullptr;
    self->hhOpenLevel = nullptr;
    self->crashBrightness = nullptr;
    self->crashDecay = nullptr;
    self->crashLevel = nullptr;
    self->cowbellTone = nullptr;
    self->cowbellDecay = nullptr;
    self->cowbellLevel = nullptr;
    self->claveTone = nullptr;
    self->claveDecay = nullptr;
    self->claveLevel = nullptr;
    self->bashSize = nullptr;
    self->bashSpread = nullptr;
    self->bashDecay = nullptr;
    self->bashDrive = nullptr;
    self->bashNoise = nullptr;
    self->bashEdge = nullptr;
    self->bashLevel = nullptr;
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
        case PORT_KICK_LEVEL: self->kickLevel = static_cast<const float*>(data); break;
        case PORT_SNARE_TONE: self->snareTone = static_cast<const float*>(data); break;
        case PORT_SNARE_SNAP: self->snareSnap = static_cast<const float*>(data); break;
        case PORT_SNARE_LEVEL: self->snareLevel = static_cast<const float*>(data); break;
        case PORT_CLAP_DENSITY: self->clapDensity = static_cast<const float*>(data); break;
        case PORT_CLAP_TONE: self->clapTone = static_cast<const float*>(data); break;
        case PORT_CLAP_LEVEL: self->clapLevel = static_cast<const float*>(data); break;
        case PORT_TOM1_PITCH: self->tom1Pitch = static_cast<const float*>(data); break;
        case PORT_TOM1_DECAY: self->tom1Decay = static_cast<const float*>(data); break;
        case PORT_TOM1_LEVEL: self->tom1Level = static_cast<const float*>(data); break;
        case PORT_TOM2_PITCH: self->tom2Pitch = static_cast<const float*>(data); break;
        case PORT_TOM2_DECAY: self->tom2Decay = static_cast<const float*>(data); break;
        case PORT_TOM2_LEVEL: self->tom2Level = static_cast<const float*>(data); break;
        case PORT_HH_CLOSED_BRIGHTNESS: self->hhClosedBrightness = static_cast<const float*>(data); break;
        case PORT_HH_CLOSED_DECAY: self->hhClosedDecay = static_cast<const float*>(data); break;
        case PORT_HH_CLOSED_LEVEL: self->hhClosedLevel = static_cast<const float*>(data); break;
        case PORT_HH_OPEN_BRIGHTNESS: self->hhOpenBrightness = static_cast<const float*>(data); break;
        case PORT_HH_OPEN_DECAY: self->hhOpenDecay = static_cast<const float*>(data); break;
        case PORT_HH_OPEN_LEVEL: self->hhOpenLevel = static_cast<const float*>(data); break;
        case PORT_CRASH_BRIGHTNESS: self->crashBrightness = static_cast<const float*>(data); break;
        case PORT_CRASH_DECAY: self->crashDecay = static_cast<const float*>(data); break;
        case PORT_CRASH_LEVEL: self->crashLevel = static_cast<const float*>(data); break;
        case PORT_COWBELL_TONE: self->cowbellTone = static_cast<const float*>(data); break;
        case PORT_COWBELL_DECAY: self->cowbellDecay = static_cast<const float*>(data); break;
        case PORT_COWBELL_LEVEL: self->cowbellLevel = static_cast<const float*>(data); break;
        case PORT_CLAVE_TONE: self->claveTone = static_cast<const float*>(data); break;
        case PORT_CLAVE_DECAY: self->claveDecay = static_cast<const float*>(data); break;
        case PORT_CLAVE_LEVEL: self->claveLevel = static_cast<const float*>(data); break;
        case PORT_BASH_SIZE: self->bashSize = static_cast<const float*>(data); break;
        case PORT_BASH_SPREAD: self->bashSpread = static_cast<const float*>(data); break;
        case PORT_BASH_DECAY: self->bashDecay = static_cast<const float*>(data); break;
        case PORT_BASH_DRIVE: self->bashDrive = static_cast<const float*>(data); break;
        case PORT_BASH_NOISE: self->bashNoise = static_cast<const float*>(data); break;
        case PORT_BASH_EDGE: self->bashEdge = static_cast<const float*>(data); break;
        case PORT_BASH_LEVEL: self->bashLevel = static_cast<const float*>(data); break;
        case PORT_BIT_CRUSH: self->bitCrush = static_cast<const float*>(data); break;
        case PORT_MASTER_DRIVE: self->masterDrive = static_cast<const float*>(data); break;
        case PORT_MASTER_REVERB: self->masterReverb = static_cast<const float*>(data); break;
        case PORT_MASTER_GAIN: self->masterGain = static_cast<const float*>(data); break;
        case PORT_AUDIO_OUT_RIGHT: self->audioOutRight = static_cast<float*>(data); break;
        default: break;
    }
}

static void activate(LV2_Handle instance) {
    auto* self = static_cast<DrumkitLV2*>(instance);
    self->engine = std::make_unique<DrumKitEngine>(self->sampleRate);
}

static void run(LV2_Handle instance, uint32_t n_samples) {
    auto* self = static_cast<DrumkitLV2*>(instance);
    if (!self || (!self->audioOut && !self->audioOutRight) || !self->engine) return;

    // Apply all parameters once per block
    apply_parameters(self);

    // Clear output buffer
    if (self->audioOut) {
        std::memset(self->audioOut, 0, n_samples * sizeof(float));
    }
    if (self->audioOutRight) {
        std::memset(self->audioOutRight, 0, n_samples * sizeof(float));
    }

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
        const StereoFrame frame = self->engine->processStereo();
        if (self->audioOut) {
            self->audioOut[i] = frame.left;
        }
        if (self->audioOutRight) {
            self->audioOutRight[i] = frame.right;
        }
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
